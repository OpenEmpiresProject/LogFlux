#include "FileSource.h"

#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStandardItem>
#include <QSet>
#include <QDebug>
#include <algorithm> // for std::sort


static bool tryParseJsonLine(const QString& line, QJsonObject& outObj)
{
	const auto bytes = line.toUtf8();
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
	if (err.error != QJsonParseError::NoError)
		return false;
	if (!doc.isObject())
		return false;
	outObj = doc.object();
	return true;
}

FileSource::FileSource(const QString& filePath)
	: m_filePath(filePath)
{
	m_processedLines.reserve(10000);
	watcher.addPath(filePath);

}

QString FileSource::description() const
{
	return QStringLiteral("File: %1").arg(m_filePath);
}

void FileSource::startProcessing()
{
	QFile file(m_filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qWarning() << "Failed to open file" << m_filePath;
		return;
	}

	QTextStream in(&file);
	QVector<QString> lines;
	lines.reserve(1024);
	while (!in.atEnd()) 
	{
		lines.push_back(in.readLine());
	}

	m_lastFilePos = file.pos();

	// Quick detection: try parse first N non-empty lines as JSON and count successes.
	int tryCount = qMin<int>(static_cast<int>(lines.size()), 50);
	int jsonCount = 0;
	int checked = 0;
	for (int i = 0; i < tryCount; ++i) {
		const QString& l = lines[i].trimmed();
		if (l.isEmpty())
			continue;
		++checked;
		QJsonObject tmp;
		if (tryParseJsonLine(l, tmp))
			++jsonCount;
	}

	m_isJson = (checked > 0 && jsonCount * 2 >= checked); // majority heuristic

	if (m_isJson) {
		parseAsJson(lines);
	}
	else {
		parseAsRawLine(lines);
	}
}

void FileSource::refresh()
{
	startProcessing();
}

void FileSource::onFileChanged(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return;

	// file truncated or rotated
	if (file.size() < m_lastFilePos) {
		m_lastFilePos = 0;
	}

	file.seek(m_lastFilePos);

	QStringList lines;
	QTextStream in(&file);
	while (!in.atEnd())
	{
		auto line = in.readLine();
		if (not line.isEmpty())
			lines.push_back(line);
	}
	m_lastFilePos = file.pos();

	if (m_isJson) {
		parseAsJson(lines);
	}
	else {
		parseAsRawLine(lines);
	}
}

void FileSource::startTailing()
{
	QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, this, &FileSource::onFileChanged);
}

void FileSource::stopTailing()
{
	QObject::disconnect(&watcher, &QFileSystemWatcher::fileChanged, this, &FileSource::onFileChanged);
}

void FileSource::parseAsJson(QVector<QString>& lines)
{
	// Collect keys from all JSON objects
	QSet<QString> allKeys;
	QVector<QJsonObject> objects;
	objects.reserve(lines.size());
	for (const QString& rawLine : lines) {
		const QString l = rawLine.trimmed();
		if (l.isEmpty())
			continue;
		QJsonObject obj;
		if (tryParseJsonLine(l, obj)) {
			objects.push_back(obj);
			for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
				allKeys.insert(it.key());
		}
		else {
			// Non-json line -> add as raw under "raw"
			QJsonObject fallback;
			fallback.insert(QStringLiteral("raw"), QJsonValue(l));
			objects.push_back(fallback);
			allKeys.insert(QStringLiteral("raw"));
		}
	}

	// Setup headers in deterministic order: gather keys then sort
	QStringList headers;
	headers.reserve(allKeys.size());
	for (const QString& k : allKeys)
		headers.append(k);
	std::sort(headers.begin(), headers.end(), [](const QString& a, const QString& b) { return a < b; });

	//model->setColumnCount(headers.size());
	//model->setHorizontalHeaderLabels(headers);
	emit onHeader(this, headers);

	// Fill rows
	for (const QJsonObject& obj : qAsConst(objects)) {
		QList<QStandardItem*> rowItems;
		rowItems.reserve(headers.size());
		for (const QString& h : headers) {
			if (obj.contains(h)) {
				QJsonValue v = obj.value(h);
				QString text;
				if (v.isString()) text = v.toString();
				else if (v.isBool()) text = v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
				else if (v.isDouble()) text = QString::number(v.toDouble());
				else if (v.isObject()) text = QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
				else if (v.isArray())
					text = QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
				else text = QString();
				rowItems.append(new QStandardItem(text));
			}
			else {
				rowItems.append(new QStandardItem(QString()));
			}
		}

		m_processedLines.push_back(rowItems);
		emit onNewLine(this, rowItems);
		//model->appendRow(rowItems);
	}
}

void FileSource::parseAsRawLine(QVector<QString> lines)
{
	// --- Timestamp patterns
	QVector<QRegularExpression> tsPatterns = {
		QRegularExpression(R"((\d{4}-\d{2}-\d{2}[T\s]\d{2}:\d{2}:\d{2}(?:\.\d+)?Z?))"),
		QRegularExpression(R"((\d{2}:\d{2}:\d{2}(?:\.\d+)?))"),
		QRegularExpression(R"((\d{4}/\d{2}/\d{2} \d{2}:\d{2}:\d{2}))"),
		QRegularExpression(R"(([A-Z][a-z]{2} \d{1,2} \d{2}:\d{2}:\d{2}))")
	};

	QRegularExpression reLevel(
		R"(\b(DEBUG|INFO|WARN|WARNING|ERROR|TRACE|FATAL|CRITICAL)\b)",
		QRegularExpression::CaseInsensitiveOption);

	QRegularExpression reMDC(R"(\[(\w+)=([^\]]+)\])");

	QVector<QVariantMap> parsedRows;
	QSet<QString> dynamicKeys = { "timestamp", "level", "message" };

	for (const QString& rawLine : lines) {
		QString line = rawLine;
		QString timestamp, level;
		QMap<QString, QString> mdc;

		// track spans removed (to later clean only generated empty [])
		QVector<QPair<int, int>> removedSpans;

		// --- timestamp
		for (const auto& re : tsPatterns) {
			auto m = re.match(line);
			if (m.hasMatch()) {
				timestamp = m.captured(1);

				int start = m.capturedStart(1);
				int len = m.capturedLength(1);

				// include surrounding []
				if (start > 0 && line[start - 1] == '[' &&
					start + len < line.size() && line[start + len] == ']') {
					start -= 1;
					len += 2;
				}

				removedSpans.append({ start, len });
				break;
			}
		}

		// --- level
		auto ml = reLevel.match(line);
		if (ml.hasMatch()) {
			level = ml.captured(1).toUpper();

			int start = ml.capturedStart(1);
			int len = ml.capturedLength(1);

			if (start > 0 && line[start - 1] == '[' &&
				start + len < line.size() && line[start + len] == ']') {
				start -= 1;
				len += 2;
			}

			removedSpans.append({ start, len });
		}

		// --- MDC
		auto it = reMDC.globalMatch(line);
		while (it.hasNext()) {
			auto m = it.next();

			QString key = m.captured(1);
			QString value = m.captured(2);

			mdc[key] = value;
			dynamicKeys.insert(key);

			removedSpans.append({ m.capturedStart(0), m.capturedLength(0) });
		}

		// --- remove spans (reverse order)
		std::sort(removedSpans.begin(), removedSpans.end(),
			[](auto a, auto b) { return a.first > b.first; });

		for (auto [start, len] : removedSpans)
			line.remove(start, len);

		// --- remove ONLY empty brackets created by removals
		QRegularExpression emptyBrackets(R"(\[\s*\])");
		line.replace(emptyBrackets, QString());

		QString message = line.trimmed();

		QVariantMap row;
		row["timestamp"] = timestamp;
		row["level"] = level;
		row["message"] = message;

		for (auto it = mdc.begin(); it != mdc.end(); ++it)
			row[it.key()] = it.value();

		parsedRows.append(row);
	}

	// --- headers
	QStringList headers = dynamicKeys.values();
	std::sort(headers.begin(), headers.end());
	headers.removeAll("timestamp");
	headers.removeAll("level");
	headers.removeAll("message");
	headers.prepend("message");
	headers.prepend("level");
	headers.prepend("timestamp");

	emit onHeader(this, headers);

	for (const auto& row : parsedRows)
	{
		QBrush brush;

		if (row.contains("level"))
		{
			auto level = row.value("level");
			if (level.toString().toLower().contains("info"))
			{
				brush = QBrush(Qt::darkGreen);
			}
			/*else if (level.toString().toLower().contains("debug"))
			{
				brush = QBrush(Qt::blue);
			}*/
			else if (level.toString().toLower().contains("warn"))
			{
				brush = QBrush(Qt::yellow);
			}
			else if (level.toString().toLower().contains("err"))
			{
				brush = QBrush(Qt::red);
			}
		}

		QList<QStandardItem*> items;
		for (const QString& h : headers)
		{
			auto item = new QStandardItem(row.value(h).toString());
			item->setForeground(brush);
			items.append(item);
		}

		m_processedLines.push_back(items);
		emit onNewLine(this, items);
	}
}

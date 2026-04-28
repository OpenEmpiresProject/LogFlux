#pragma once

#include "DataSource.h"
#include <QString>
#include <QFileSystemWatcher>

class FileSource : public DataSource
{
public:
    explicit FileSource(const QString &filePath);
    ~FileSource() override = default;

    QString description() const override;

private:
    QString m_filePath;
	std::vector<QList<QStandardItem*>> m_processedLines;
	QFileSystemWatcher watcher;
	bool m_isJson = false;
	qint64 m_lastFilePos = 0;
	void parseAsRawLine(QVector<QString> lines);
	void parseAsJson(QVector<QString>& lines);

public slots:
	void startProcessing() override;
	void refresh() override;
	void onFileChanged(const QString& path);
	void startTailing() override;
	void stopTailing() override;
};
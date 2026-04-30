#include "LogFlux.h"
#include "FileSource.h"
#include "ServerSource.h"

#include <QFileDialog>
#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QListWidget>
#include <QScrollBar>
#include "DataSourceItem.h"
#include "QThread"
#include <QInputDialog>

LogFlux::LogFlux(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	ui.splitterMain->setStretchFactor(0, 1);
	ui.splitterMain->setStretchFactor(1, 4);
    ui.tableLog->horizontalHeader()->setStretchLastSection(true);
	ui.tableLog->setWordWrap(true);
    ui.tableLog->setShowGrid(false);
    ui.tableLog->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui.tableLog->setEditTriggers(QAbstractItemView::NoEditTriggers); // Disable editing

	// Reduce the height of rows
    ui.tableLog->verticalHeader()->setMinimumSectionSize(0); // Allow shrinking below default
    ui.tableLog->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	// Set monospace font
	QFont font;
	font.setFamilies({  "Consolas", "Courier New", "Monaco", "Menlo", "DejaVu Sans Mono", "Monospace" });
	font.setStyleHint(QFont::Monospace);
	font.setFixedPitch(true);
	ui.tableLog->setFont(font);

	// Ensure user select rows but not individual cells
    ui.tableLog->setSelectionBehavior(QAbstractItemView::SelectRows);

	ui.actionStopTailing->setVisible(false);

	// Without these toolbar buttons will not be guaranteed to be square shape
	ui.mainToolBar->setIconSize(QSize(24, 24));
	ui.mainToolBar->setStyleSheet(R"(
QToolButton {
    padding: 2px;
    margin: 0px;
}
)");

	QObject::connect(ui.btnAddFile, &QPushButton::clicked, this, &LogFlux::onAddFileSource);
	QObject::connect(ui.btnAddServer, &QPushButton::clicked, this, &LogFlux::onAddServerSource);
	QObject::connect(ui.actionClearLogs, &QAction::triggered, this, &LogFlux::onClearLog);
	QObject::connect(ui.actionRefresh, &QAction::triggered, this, &LogFlux::onRefreshLog);
	QObject::connect(ui.actionTailLog, &QAction::triggered, this, &LogFlux::onStartTailing);
	QObject::connect(ui.actionStopTailing, &QAction::triggered, this, &LogFlux::onStopTailing);
    QObject::connect(ui.listSources, &QListWidget::currentItemChanged, this, &LogFlux::onSourceChange);
}

LogFlux::~LogFlux()
{

}

void LogFlux::onAddFileSource()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select log file"), QString(), tr("Log files (*.*)"));
    if (path.isEmpty())
        return;

	auto source = new FileSource(path);
	auto* thread = new QThread;
	source->moveToThread(thread);

	SourceData sourceData;
	sourceData.source = source;
	sourceData.thread = thread;
	sourceData.model = new QStandardItemModel(this);
	sourceData.signalDelagator = new SourceSignalDelegator();
	m_sources.push_back(std::move(sourceData));

	m_currentSource = m_sources.size() - 1;

	QObject::connect(source, &DataSource::onHeader, this, &LogFlux::onHeader);
	QObject::connect(source, &DataSource::onNewLine, this, &LogFlux::onNewLine);
	QObject::connect(thread, &QThread::started, source, &DataSource::startProcessing);
	QObject::connect(sourceData.signalDelagator, &SourceSignalDelegator::refresh, source, &DataSource::refresh);
	QObject::connect(sourceData.signalDelagator, &SourceSignalDelegator::startTailing, source, &DataSource::startTailing);
	QObject::connect(sourceData.signalDelagator, &SourceSignalDelegator::stopTailing, source, &DataSource::stopTailing);

    DataSourceItem* item = new DataSourceItem(ui.listSources);
	item->ui.labelOffline->setVisible(false);

    QFileInfo fi(path);
    item->ui.labelName->setText(fi.fileName());
    item->setToolTip(fi.absoluteFilePath());

    QListWidgetItem* listItem = new QListWidgetItem(ui.listSources);
    listItem->setData(Qt::UserRole, m_currentSource);

    ui.listSources->addItem(listItem);
    ui.listSources->setItemWidget(listItem, item);
	ui.listSources->setCurrentItem(listItem);

	ui.tableLog->setModel(sourceData.model);

	thread->start();
}

void LogFlux::onAddServerSource()
{
	// Ask for host and port
	bool ok = false;
	int port = QInputDialog::getInt(this, tr("Server port"), tr("Port:"), 5000, 1, 65535, 1, &ok);
	if (!ok)
		return;

	QString host = QInputDialog::getText(this, tr("Server host"), tr("Host:"), QLineEdit::Normal, QStringLiteral(""), &ok);
	if (!ok)
		return;

	auto* server = new ServerSource(host, port);
	auto* thread = new QThread;
	server->moveToThread(thread);

	SourceData sourceData;
	sourceData.source = server;
	sourceData.thread = thread;
	sourceData.model = new QStandardItemModel(this);
	sourceData.signalDelagator = new SourceSignalDelegator();
	m_sources.push_back(std::move(sourceData));

	m_currentSource = m_sources.size() - 1;

	QObject::connect(server, &DataSource::onHeader, this, &LogFlux::onHeader);
	QObject::connect(server, &DataSource::onNewLine, this, &LogFlux::onNewLine);
	QObject::connect(thread, &QThread::started, server, &DataSource::startProcessing);
	QObject::connect(m_sources[m_currentSource].signalDelagator, &SourceSignalDelegator::refresh, server, &DataSource::refresh);
	QObject::connect(m_sources[m_currentSource].signalDelagator, &SourceSignalDelegator::startTailing, server, &DataSource::startTailing);
	QObject::connect(m_sources[m_currentSource].signalDelagator, &SourceSignalDelegator::stopTailing, server, &DataSource::stopTailing);

	DataSourceItem* item = new DataSourceItem(ui.listSources);
	// server initially offline until it starts listening
	item->ui.labelOffline->setVisible(true);

	item->ui.labelName->setText(QStringLiteral("%1:%2").arg(host).arg(port));
	item->setToolTip(item->ui.labelName->text());

	QListWidgetItem* listItem = new QListWidgetItem(ui.listSources);
	listItem->setData(Qt::UserRole, static_cast<int>(m_currentSource));

	ui.listSources->addItem(listItem);
	ui.listSources->setItemWidget(listItem, item);
	ui.listSources->setCurrentItem(listItem);

	ui.tableLog->setModel(m_sources[m_currentSource].model);

	thread->start();
}

void LogFlux::clearModel(QStandardItemModel* model)
{
	if (model)
	{
		model->removeRows(0, model->rowCount());
		model->removeColumns(0, model->columnCount());
	}
}

void LogFlux::onClearLog()
{
	if (m_currentSource < m_sources.size())
	{
		auto& source = m_sources[m_currentSource];
		clearModel(source.model);
	}
}

void LogFlux::onRefreshLog()
{
	if (m_currentSource < m_sources.size())
	{
		auto& source = m_sources[m_currentSource];
		clearModel(source.model);
		source.signalDelagator->emiRefresh();
	}
}

void LogFlux::onSourceChange(QListWidgetItem* current, QListWidgetItem* previous)
{
    int index = current->data(Qt::UserRole).toInt();
    auto& source = m_sources[index];
	ui.tableLog->setModel(source.model);
}

void LogFlux::onNewLine(DataSource* source, QList<QStandardItem*> cells)
{
	auto* bar = ui.tableLog->verticalScrollBar();
	bool follow = (bar->value() == bar->maximum());

	for (auto& sourceData : m_sources)
	{
		if (sourceData.source == source)
		{
			sourceData.model->appendRow(cells);
			break;
		}
	}

	auto& currentViewingSource = m_sources[m_currentSource];
	if (currentViewingSource.source == source)
	{
		if (follow) 
		{
			// QTableView has not updated its scrollbar yet. So we need to queue/delay this
			// calculation
			QMetaObject::invokeMethod(bar, [bar]() {
				bar->setValue(bar->maximum());
				}, Qt::QueuedConnection);
		}
	}
}

void LogFlux::onHeader(DataSource* source, QStringList headers)
{
	for (auto& sourceData : m_sources)
	{
		if (sourceData.source == source)
		{
			sourceData.model->setColumnCount(headers.size());
			sourceData.model->setHorizontalHeaderLabels(headers);
			return;
		}
	}
}

void LogFlux::onStartTailing()
{
	if (m_currentSource < m_sources.size())
	{
		auto& source = m_sources[m_currentSource];
		source.signalDelagator->emitStartTailing();

		ui.actionStopTailing->setVisible(true);
		ui.actionTailLog->setVisible(false);
	}
}

void LogFlux::onStopTailing()
{
	if (m_currentSource < m_sources.size())
	{
		auto& source = m_sources[m_currentSource];
		source.signalDelagator->emitStopTailing();

		ui.actionStopTailing->setVisible(false);
		ui.actionTailLog->setVisible(true);
	}
}

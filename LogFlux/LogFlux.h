#pragma once

#include "QMetaType"
#include "ui_LogFlux.h"

#include <QtWidgets/QMainWindow>
#include <QStandardItemModel>
#include <memory>
#include <vector>

class DataSource;

// This class will make it possible to have 1:1 signal:slot connections between the
// LogFlux main window and the data source running in worker threads.
// Otherwise, LogFlux has to broadcast while worker filter out signals.
class SourceSignalDelegator : public QObject
{
    Q_OBJECT

public:
    void emiRefresh()
    {
        emit refresh();
    }

    void emitStartTailing()
    {
        emit startTailing();
    }

	void emitStopTailing()
	{
		emit stopTailing();
	}


signals:
	void refresh();
	void startTailing();
	void stopTailing();
};

struct SourceData
{
    DataSource* source = nullptr;
    QStandardItemModel* model = nullptr;
    QThread* thread = nullptr;
    SourceSignalDelegator* signalDelagator = nullptr;

	// allow move
	SourceData(SourceData&&) = default;
	SourceData& operator=(SourceData&&) = default;

	// disallow copy
	SourceData(const SourceData&) = delete;
	SourceData& operator=(const SourceData&) = delete;

	SourceData() = default;
};

class LogFlux : public QMainWindow
{
    Q_OBJECT

public:
    LogFlux(QWidget *parent = nullptr);
    ~LogFlux();

private:
    Ui::LogViewerClass ui;
    std::vector<SourceData> m_sources;
    size_t m_currentSource = 0;

    void clearModel(QStandardItemModel* model);

private slots:
    void onAddFileSource();
    void onAddServerSource();
	void onClearLog();
	void onRefreshLog();
    void onSourceChange(QListWidgetItem* current, QListWidgetItem* previous);
	void onNewLine(DataSource* source, QList<QStandardItem*> cells);
	void onHeader(DataSource* source, QStringList headers);
    void onStartTailing();
    void onStopTailing();
};


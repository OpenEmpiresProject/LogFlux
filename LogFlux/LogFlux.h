#pragma once

#include "QMetaType"
#include "ui_LogFlux.h"
#include "Settings.h"
#include "Filters.h"

#include <QtWidgets/QMainWindow>
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

signals:
	void refresh();
};

struct SourceData
{
    DataSource* source = nullptr;
    QThread* thread = nullptr;
    SourceSignalDelegator* signalDelagator = nullptr;
	bool online = false;
	int lineCount = 0;
	int errorCount = 0;
	int warnCount = 0;
};

enum SourceType
{
	UNKNOWN_SOURCE = -1,
	SERVER_SOURCE = 0,
	FILE_SOURCE = 1
};

class LogFlux : public QMainWindow
{
    Q_OBJECT

public:
    LogFlux(QWidget *parent = nullptr);
    ~LogFlux();

private:
    Ui::LogViewerClass ui;

	// Sources related
    QMap<SourceType, SourceData> m_sources;
	SourceType m_currentSource = SourceType::UNKNOWN_SOURCE;

	// Search related
	QString m_searchText;
	QList<QTextEdit::ExtraSelection> m_searchSelections;
	QTextCursor m_currentMatch;

	// Filtering
	QList<QString> m_filters;        // active filters as raw strings (for UI/backup)
	QList<QString> m_filtersBackup;
	// full buffer of lines currently shown (cleared on onClearLog)
	QList<QString> m_allLines;

	// Parsed/compiled filter objects (each corresponds to a tag from m_filters).
	// Each filter object's .matches() implements OR internally if the tag used '|'.

	std::vector<std::shared_ptr<IFilter>> m_filterObjects;

	// When true, quick filters are used instead of m_filterObjects (normal filters).
	bool m_useQuickFilters = false;

	QTextCharFormat formatForLine(const QString& line);

	void startServer(const QString& host, int port);
	void updateSelections();
	void updateSearchCount();
	void ensureCursorVisibleOnlyIfNeeded(const QTextCursor& cursor);
	void goToStartOfLog();
	void goToEndOfLog();
	void goToNextError();
	void goToPreviousError();
	void goToNextWarning();
	void goToPreviousWarning();
	// helper for navigating to next/previous token (case-insensitive, wraps)
	void navigateToLogToken(const QString& token, QTextDocument::FindFlags findFlags = {});
	void setupShortcuts();
	void setupStatusBar();
	void destroyExistingSource(SourceType type);
	void addNewSource(SourceType type, DataSource* source);

	// Bookmarks
	void addBookmark();
	void goToNextBookmark();
	void goToPreviousBookmark();

	// Build compiled filter objects from the current m_filters list.
	void rebuildFilterObjects();

	// Apply quick filters to the currently buffered lines and rebuild view.
	void applyQuickFilters();

private slots:
	void onAddFileSource();
	void onClearLog();
	void onRefreshLog();
	void onServerSelect(bool selected);
	void onFileSelect(bool selected);
	void onNewLine(DataSource* source, const QString& line);
	void onSourceStatusChange(DataSource* source, bool online);
	void highlightCurrentLine();
	void highlightAllMatches(const QString& text);
	void findNext();
	void findPrevious();
	bool eventFilter(QObject* obj, QEvent* event);
	void launchSettingsWindow();
	void filtersChanged(const QStringList& filters);
	void filtersEnabled(bool enabled);
	void onQuickFiltersChanged(bool checked);

signals:
	void sourceStatusChange(SourceType type, DataSource* source, bool online);
};
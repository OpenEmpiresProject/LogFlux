#include "LogFlux.h"
#include "FileSource.h"
#include "ServerSource.h"
#include "Settings.h"
#include "MiniMap.h"
#include "Filters.h"

#include <QFileDialog>
#include <QDebug>
#include <QFile>
#include <QFontDatabase>
#include <QListWidget>
#include <QScrollBar>
#include "QThread"
#include <QInputDialog>
#include <QWidgetAction>
#include <QShortcut>
#include <QRegularExpression>
#include <QCoreApplication>
#include <algorithm>

LogFlux::LogFlux(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

	// Set monospace font
	QFont font;
	font.setFamilies({  "Consolas", "Courier New", "Monaco", "Menlo", "DejaVu Sans Mono", "Monospace" });
	font.setStyleHint(QFont::Monospace);
	font.setFixedPitch(true);
	ui.plainTextEdit->setFont(font);

	// To detect pressing Shift while pressing enter to go to previous search result
	ui.editSearch->installEventFilter(this);

	ui.bookmarkBar->setBookmarkIcon(QPixmap(":/images/bookmark.png"));
	ui.bookmarkBar->setEditor(ui.plainTextEdit); // promoted to LogTextEdit
	ui.lineNumberArea->setEditor(ui.plainTextEdit);
	// Let the line number area know the authoritative visible->absolute mapping
	ui.lineNumberArea->setVisibleToAbsoluteMap(&m_visibleToAbsolute);
	ui.scrollbarMinimap->setEditor(ui.plainTextEdit);
	
	// Hide default scrollbar in favour of minimap scrollbar
	ui.plainTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	setupShortcuts();
	setupStatusBar();

	connect(ui.btnClearLog, &QPushButton::clicked, this, &LogFlux::onClearLog);
	connect(ui.btnReload, &QPushButton::clicked, this, &LogFlux::onRefreshLog);
	connect(ui.btnBrowseFile, &QPushButton::clicked, this, &LogFlux::onAddFileSource);
	connect(ui.btnSettings, &QPushButton::clicked, this, &LogFlux::launchSettingsWindow);
	connect(ui.plainTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &LogFlux::highlightCurrentLine);
	connect(ui.plainTextEdit, &QPlainTextEdit::cursorPositionChanged, this, &LogFlux::updateSelections);
	connect(ui.editSearch, &QLineEdit::textChanged, this, &LogFlux::highlightAllMatches);
	connect(ui.btnSearchDown, &QPushButton::clicked, this, &LogFlux::findNext);
	connect(ui.btnSearchUp, &QPushButton::clicked, this, &LogFlux::findPrevious);
	connect(ui.radioServer, &QRadioButton::toggled, this, &LogFlux::onServerSelect);
	connect(ui.radioFile, &QRadioButton::toggled, this, &LogFlux::onFileSelect);
	connect(ui.filters, &TagBar::tagsChanged, this, &LogFlux::filtersChanged);
	connect(ui.lineEditFilter, &TagLineEdit::tagEntered, ui.filters, &TagBar::addTag);
	connect(ui.lineEditFilter, &TagLineEdit::backspaceOnEmpty, ui.filters, &TagBar::removeLastTag);
	// Keep minimap in sync with bookmark changes (bookmarks can change anytime).
	connect(ui.bookmarkBar, &BookmarkArea::bookmarkToggled, ui.scrollbarMinimap, &MiniMap::setBookmark);
	// When user toggles a bookmark in the view, update authoritative set in LogFlux.
	connect(ui.bookmarkBar, &BookmarkArea::bookmarkToggled, this, &LogFlux::onBookmarkToggled);
	connect(ui.checkBoxFilters, &QCheckBox::toggled, this, &LogFlux::filtersEnabled);
	connect(ui.checkBoxErrors, &QCheckBox::toggled, this, &LogFlux::onQuickFiltersChanged);
	connect(ui.checkBoxWarnings, &QCheckBox::toggled, this, &LogFlux::onQuickFiltersChanged);

	startServer("", 5000); // Start server, but file would be the default source
}

LogFlux::~LogFlux()
{

}

void LogFlux::rebuildFilterObjects()
{
	// Rebuild compiled filter objects from the m_filters strings
	m_filterObjects = parseFilters(m_filters);
}

void LogFlux::addNewSource(SourceType type, DataSource* source)
{
	auto* thread = new QThread;
	source->moveToThread(thread);

	SourceData sourceData;
	sourceData.source = source;
	sourceData.thread = thread;
	sourceData.signalDelagator = new SourceSignalDelegator();
	m_sources.insert(type, std::move(sourceData));

	QObject::connect(source, &DataSource::onNewLine, this, &LogFlux::onNewLine);
	QObject::connect(source, &DataSource::onStatusChange, this, &LogFlux::onSourceStatusChange);
	QObject::connect(thread, &QThread::started, source, &DataSource::startProcessing);
	QObject::connect(sourceData.signalDelagator, &SourceSignalDelegator::refresh, source, &DataSource::refresh);

	thread->start();
}

void LogFlux::onAddFileSource()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Select log file"), QString(), tr("Log files (*.*)"));
    if (path.isEmpty())
        return;

	destroyExistingSource(SourceType::FILE_SOURCE);

	DataSource* source = new FileSource(path);
	auto description = source->description();
	m_currentSource = SourceType::FILE_SOURCE;

	addNewSource(SourceType::FILE_SOURCE, source);

	ui.labelSourceStatus->setText(description);
	ui.labelSourceStatus->setVisible(true);
	onClearLog();
}

void LogFlux::startServer(const QString& host, int port)
{
	destroyExistingSource(SourceType::SERVER_SOURCE);
	addNewSource(SourceType::SERVER_SOURCE, new ServerSource(host, port));
	m_currentSource = SourceType::FILE_SOURCE;
}

void LogFlux::onClearLog()
{
	ui.plainTextEdit->clear();
	ui.labelLineCount->setText("Lines: 0");
	ui.labelErrorCount->setText("Errors: 0");
	ui.labelWarningCount->setText("Warns: 0");
 
 	// clear the in-memory buffer of lines (document will be rebuilt from this buffer on filtering)
 	m_allLines.clear();
 
	// clear bookmarks (they refer to absolute indices into m_allLines)
	m_bookmarks.clear();
	m_visibleToAbsolute.clear();
	ui.bookmarkBar->clearAllBookmarks();
	ui.scrollbarMinimap->clearMarkers();

	// Notify line number gutter that mapping/content changed
	ui.lineNumberArea->refresh();

	if (m_sources.contains(m_currentSource))
	{
		auto& sourceData = m_sources[m_currentSource];
		sourceData.lineCount = 0;
		sourceData.errorCount = 0;
		sourceData.warnCount = 0;
	}
}

void LogFlux::onRefreshLog()
{
	onClearLog();

	if (m_sources.contains(m_currentSource))
	{
		auto& source = m_sources[m_currentSource];
		source.signalDelagator->emiRefresh();
	}
}

void LogFlux::onServerSelect(bool selected)
{
	if (not selected)
		return;

	onClearLog();
	m_currentSource = SourceType::SERVER_SOURCE;

	if (m_sources.contains(m_currentSource))
	{
		const auto& sourceData = m_sources[m_currentSource];
		
		ui.labelSourceStatus->setText(sourceData.source->description());
		ui.labelSourceStatus->setVisible(true);
		ui.labelOnline->setVisible(sourceData.online);
		ui.labelOffline->setVisible(not sourceData.online);
	}
	else
	{
		ui.labelSourceStatus->setVisible(false);
		ui.labelOnline->setVisible(false);
		ui.labelOffline->setVisible(false);
	}
}

void LogFlux::onFileSelect(bool selected)
{
	if (not selected)
		return;

	onClearLog();
	m_currentSource = SourceType::FILE_SOURCE;

	if (m_sources.contains(m_currentSource))
	{
		const auto& sourceData = m_sources[m_currentSource];

		ui.labelSourceStatus->setText(sourceData.source->description());
		ui.labelSourceStatus->setVisible(true);
		ui.labelOnline->setVisible(sourceData.online);
		ui.labelOffline->setVisible(not sourceData.online);
	}
	else
	{
		ui.labelSourceStatus->setVisible(false);
		ui.labelOnline->setVisible(false);
		ui.labelOffline->setVisible(false);
	}
}

QTextCharFormat LogFlux::formatForLine(const QString& line)
{
	QTextCharFormat fmt;
	// classification with case-insensitive checks to keep color logic robust
	if (line.contains("trace", Qt::CaseInsensitive) || line.contains("info", Qt::CaseInsensitive))
	{
		fmt.setForeground(QColor(80, 160, 255));  // softer bright blue
	}
	else if (line.contains("warn", Qt::CaseInsensitive))
	{
		fmt.setForeground(Qt::yellow);
	}
	else if (line.contains("error", Qt::CaseInsensitive))
	{
		fmt.setForeground(Qt::red);
	}
	else
	{
		fmt.setForeground(Qt::white);
	}
	return fmt;
}

void LogFlux::onNewLine(DataSource* source, const QString& line)
{
	// Data from different source than currently active, ignore
	if (m_sources.value(m_currentSource, SourceData()).source != source)
		return;

	int absoluteIndex = m_allLines.size();

	// keep a full buffer of all lines for the current view (used for filtering)
	m_allLines.append(line);

	auto& sourceData = m_sources[m_currentSource];
	sourceData.lineCount++;
	ui.labelLineCount->setText("Lines: " + QString::number(sourceData.lineCount));

	// update counts for warns/errors based on content (total counts remain even if filtered)
	if (line.contains("warn", Qt::CaseInsensitive))
	{
		sourceData.warnCount++;
		ui.labelWarningCount->setText("Warns: " + QString::number(sourceData.warnCount));
	}
	else if (line.contains("error", Qt::CaseInsensitive))
	{
		sourceData.errorCount++;
		ui.labelErrorCount->setText("Errors: " + QString::number(sourceData.errorCount));
	}

	// Decide whether to show the line depending on active filters:
	// - across tags: AND (every tag must match)
	// - within a tag: OR (handled by filter object)
	bool shouldShow = false;

	if (m_useQuickFilters)
	{
		// Quick filters: OR semantics between error/warning checkboxes.
		const bool wantError = ui.checkBoxErrors->isChecked();
		const bool wantWarn = ui.checkBoxWarnings->isChecked();

		if (wantError && line.contains("error", Qt::CaseInsensitive))
			shouldShow = true;
		if (wantWarn && line.contains("warn", Qt::CaseInsensitive))
			shouldShow = true;
	}
	else
	{
		// existing normal filters behavior
		shouldShow = m_filterObjects.empty();
		if (!shouldShow)
		{
			shouldShow = true;
			for (const auto& f : m_filterObjects)
			{
				if (!f || !f->matches(line))
				{
					shouldShow = false;
					break;
				}
			}
		}
	}

	if (!shouldShow)
		return;

	QTextCharFormat fmt = formatForLine(line);

	bool atEnd = ui.plainTextEdit->textCursor().atEnd();
	
	// create a separate cursor for insertion
	QTextCursor insertCursor(ui.plainTextEdit->document());
	insertCursor.movePosition(QTextCursor::End);
	insertCursor.insertText(line + "\n", fmt);

	// If the inserted line is visible, add a minimap marker for warn/error.
	// The new block will be the last block in the document.
	int blockNumber = ui.plainTextEdit->document()->blockCount() - 1;
	if (line.contains("warn", Qt::CaseInsensitive))
	{
		ui.scrollbarMinimap->addMarker(blockNumber, MiniMap::Warning);
	}
	else if (line.contains("error", Qt::CaseInsensitive))
	{
		ui.scrollbarMinimap->addMarker(blockNumber, MiniMap::Error);
	}

	// Update visible-to-absolute mapping (new visible line maps to absolute index)
	m_visibleToAbsolute.append(absoluteIndex);

	// Notify line number gutter about new mapping/possible width change
	ui.lineNumberArea->refresh();

	// If this absolute line is bookmarked, reflect it in bookmark view + minimap
	if (m_bookmarks.contains(absoluteIndex))
	{
		ui.bookmarkBar->setBookmark(blockNumber, true);
		ui.scrollbarMinimap->setBookmark(blockNumber, true);
	}

 	if (atEnd)
 	{
 		ui.plainTextEdit->moveCursor(QTextCursor::End);
 	}
}

void LogFlux::onSourceStatusChange(DataSource* source, bool online)
{
	for (auto it = m_sources.begin(); it != m_sources.end(); ++it)
	{
		const auto& type = it.key();
		auto& sourceData = it.value();

		if (sourceData.source == source)
		{
			sourceData.online = online;
			emit sourceStatusChange(type, source, online);

			if (type == m_currentSource)
			{
				ui.labelOnline->setVisible(online);
				ui.labelOffline->setVisible(not online);
			}
		}
	}
}

void LogFlux::highlightCurrentLine()
{
	QTextEdit::ExtraSelection selection;

	QColor lineColor = QColor(60, 60, 60); // subtle background
	selection.format.setBackground(lineColor);
	selection.format.setProperty(QTextFormat::FullWidthSelection, true);

	selection.cursor = ui.plainTextEdit->textCursor();
	selection.cursor.clearSelection();

	QList<QTextEdit::ExtraSelection> selections;
	selections.append(selection);
	ui.plainTextEdit->setExtraSelections(selections);
}

void LogFlux::highlightAllMatches(const QString& text)
{
	m_searchSelections.clear();
	m_searchText = text;
	m_currentMatch = QTextCursor();

	if (text.isEmpty())
	{
		updateSelections();
		updateSearchCount();

		// Done with search, so move the focus back to log area. otherwise user
		// has to click there again.
		ui.editSearch->clearFocus();
		ui.plainTextEdit->setFocus();

		return;
	}

	QTextDocument* doc = ui.plainTextEdit->document();
	QTextCursor cursor(doc);

	QTextCharFormat fmt;
	fmt.setBackground(QColor(255, 255, 0, 100)); // yellow

	while (!cursor.isNull() && !cursor.atEnd())
	{
		cursor = doc->find(text, cursor);

		if (!cursor.isNull())
		{
			QTextEdit::ExtraSelection sel;
			sel.cursor = cursor;
			sel.format = fmt;
			m_searchSelections.append(sel);
		}
	}

	// set first match as current (optional)
	if (!m_searchSelections.isEmpty())
	{
		m_currentMatch = m_searchSelections.first().cursor;
		ui.plainTextEdit->setTextCursor(m_currentMatch);
	}

	updateSelections();
	updateSearchCount();
}

void LogFlux::filtersChanged(const QStringList& filters)
{
	m_filters = filters;
	rebuildFilterObjects();

	ui.plainTextEdit->clear();
	m_visibleToAbsolute.clear();

	QVector<MiniMap::Marker> minimapMarkers;

	// Re-insert only lines matching filters
	for (int absoluteIndex = 0; absoluteIndex < m_allLines.size(); ++absoluteIndex)
	{
		const QString& line = m_allLines[absoluteIndex];

		bool matches = m_filterObjects.empty();
		if (!matches)
		{
			matches = true;
			for (const auto& f : m_filterObjects)
			{
				if (!f || !f->matches(line))
				{
					matches = false;
					break;
				}
			}
		}

		if (m_filterObjects.empty() || matches)
		{
			QTextCharFormat fmt = formatForLine(line);

			QTextCursor insertCursor(ui.plainTextEdit->document());
			insertCursor.movePosition(QTextCursor::End);
			insertCursor.insertText(line + "\n", fmt);

			m_visibleToAbsolute.append(absoluteIndex);

			int blockNumber = ui.plainTextEdit->document()->blockCount() - 1;
			if (line.contains("warn", Qt::CaseInsensitive))
			{
				minimapMarkers.append({ blockNumber, MiniMap::Warning });
			}
			else if (line.contains("error", Qt::CaseInsensitive))
			{
				minimapMarkers.append({ blockNumber, MiniMap::Error });
			}

			if (m_bookmarks.contains(absoluteIndex))
			{
				minimapMarkers.append({ blockNumber, MiniMap::Bookmark });
			}
		}
	}

	// Update bookmark view to reflect which absolute bookmarks are currently visible.
	QSet<int> visibleBookmarks;
	for (int visibleBlock = 0; visibleBlock < m_visibleToAbsolute.size(); ++visibleBlock)
	{
		if (m_bookmarks.contains(m_visibleToAbsolute[visibleBlock]))
			visibleBookmarks.insert(visibleBlock);
	}

	ui.bookmarkBar->setBookmarks(visibleBookmarks);
	ui.scrollbarMinimap->setMarkers(minimapMarkers);

	// Notify line number gutter that mapping and document were rebuilt
	ui.lineNumberArea->refresh();

	// Update search highlights (if any search text active)
	if (!m_searchText.isEmpty())
		highlightAllMatches(m_searchText);
	else
		updateSelections();
}

void LogFlux::filtersEnabled(bool enabled)
{
	if (enabled)
	{
		// When user re-enables normal filters, we must stop using quick filters
		// and clear quick-filter checkboxes.
		if (m_useQuickFilters)
		{
			// prevent quick filter handlers from running while we clear UI
			ui.checkBoxErrors->blockSignals(true);
			ui.checkBoxWarnings->blockSignals(true);

			ui.checkBoxErrors->setChecked(false);
			ui.checkBoxWarnings->setChecked(false);

			ui.checkBoxErrors->blockSignals(false);
			ui.checkBoxWarnings->blockSignals(false);

			m_useQuickFilters = false;
		}

		// Restore previous filters
		filtersChanged(m_filtersBackup);
	}
	else
	{
		// disable normal filters but keep them backed up
		m_filtersBackup = m_filters;
		filtersChanged({});
	}
}

void LogFlux::updateSelections()
{
	QList<QTextEdit::ExtraSelection> selections;

	// current line highlight
	QTextEdit::ExtraSelection lineSel;
	lineSel.format.setBackground(QColor(60, 60, 60));
	lineSel.format.setProperty(QTextFormat::FullWidthSelection, true);
	lineSel.cursor = ui.plainTextEdit->textCursor();
	lineSel.cursor.clearSelection();
	selections.append(lineSel);

	// search matches
	for (auto sel : m_searchSelections)
	{
		// highlight current match differently
		if (!m_currentMatch.isNull() &&
			sel.cursor.selectionStart() == m_currentMatch.selectionStart() &&
			sel.cursor.selectionEnd() == m_currentMatch.selectionEnd())
		{
			sel.format.setBackground(QColor(255, 165, 0)); // orange
		}

		selections.append(sel);
	}

	ui.plainTextEdit->setExtraSelections(selections);
}

void LogFlux::findNext()
{
	if (m_searchText.isEmpty())
		return;

	QTextCursor currentCursor = ui.plainTextEdit->textCursor();
	QTextCursor found = ui.plainTextEdit->document()->find(m_searchText, currentCursor);

	if (found.isNull())
	{
		// wrap
		QTextCursor start(ui.plainTextEdit->document());
		found = ui.plainTextEdit->document()->find(m_searchText, start);
	}

	if (!found.isNull())
	{
		m_currentMatch = found;
		ui.plainTextEdit->setTextCursor(found);
		ui.plainTextEdit->centerCursor();
	}

	updateSelections();
	updateSearchCount();
}

void LogFlux::findPrevious()
{
	if (m_searchText.isEmpty())
		return;

	QTextCursor currentCursor = ui.plainTextEdit->textCursor();
	QTextCursor found = ui.plainTextEdit->document()->find(
		m_searchText,
		currentCursor,
		QTextDocument::FindBackward
	);

	if (found.isNull())
	{
		QTextCursor end(ui.plainTextEdit->document());
		end.movePosition(QTextCursor::End);

		found = ui.plainTextEdit->document()->find(
			m_searchText,
			end,
			QTextDocument::FindBackward
		);
	}

	if (!found.isNull())
	{
		m_currentMatch = found;
		ui.plainTextEdit->setTextCursor(found);
		ui.plainTextEdit->centerCursor();
	}

	updateSelections();
	updateSearchCount();
}

bool LogFlux::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == ui.editSearch && event->type() == QEvent::KeyPress)
	{
		auto* keyEvent = static_cast<QKeyEvent*>(event);

		if (keyEvent->key() == Qt::Key_Return)
		{
			if (keyEvent->modifiers() & Qt::ShiftModifier)
				findPrevious();
			else
				findNext();

			return true;
		}
		else if (keyEvent->key() == Qt::Key_Escape)
		{
			// Clear the search bar
			ui.editSearch->clear();
 
			return true;
		}
	}
	return QMainWindow::eventFilter(obj, event);
}

void LogFlux::launchSettingsWindow()
{
	Settings settings(this);

	// Server restart
	connect(settings.ui.btnRestartServer, &QPushButton::clicked,
		this, [this, &settings]()
		{
			startServer(
				settings.ui.lineEditHost->text(),
				settings.ui.lineEditPort->text().toInt());
		});

	// Setting window's server status label update 
	connect(this, &LogFlux::sourceStatusChange, this, 
		[this, &settings](SourceType type, DataSource*, bool online)
		{
			if (type == SourceType::SERVER_SOURCE)
			{
				settings.ui.lineEditSettingServerStatus->setText(online ? "Online" : "Offline");
			}
		});

	// Set the current status of the server before launching the setting window
	auto sourceData = m_sources.value(SourceType::SERVER_SOURCE, SourceData());
	settings.ui.lineEditSettingServerStatus->setText(sourceData.online ? "Online" : "Offline");
	
	settings.exec();
}

void LogFlux::ensureCursorVisibleOnlyIfNeeded(const QTextCursor& cursor)
{
	QRect cursorRect = ui.plainTextEdit->cursorRect(cursor);
	QRect viewportRect = ui.plainTextEdit->viewport()->rect();

	int margin = 5;

	bool above = cursorRect.top() < viewportRect.top() + margin;
	bool below = cursorRect.bottom() > viewportRect.bottom() - margin;

	if (above || below)
	{
		ui.plainTextEdit->ensureCursorVisible();
	}
}

void LogFlux::goToStartOfLog()
{
	QTextCursor cursor(ui.plainTextEdit->document());
	cursor.movePosition(QTextCursor::Start);

	ui.plainTextEdit->setTextCursor(cursor);
}

void LogFlux::goToEndOfLog()
{
	QTextCursor cursor(ui.plainTextEdit->document());
	cursor.movePosition(QTextCursor::End);

	ui.plainTextEdit->setTextCursor(cursor);
}

void LogFlux::navigateToLogToken(const QString& token, QTextDocument::FindFlags findFlags)
{
	QTextDocument* doc = ui.plainTextEdit->document();
	if (!doc)
		return;

	// case-insensitive match for the token
	QRegularExpression re(token, QRegularExpression::CaseInsensitiveOption);

	QTextCursor current = ui.plainTextEdit->textCursor();
	QTextCursor found = doc->find(re, current, findFlags);

	// wrap if none found in the given direction
	if (found.isNull())
	{
		QTextCursor edge(doc);
		if (findFlags & QTextDocument::FindBackward)
			edge.movePosition(QTextCursor::End); // search backward from end
		// else default constructed cursor() starts at beginning

		found = doc->find(re, edge, findFlags);
	}

	if (!found.isNull())
	{
		ui.plainTextEdit->setTextCursor(found);
		ui.plainTextEdit->centerCursor();
		ensureCursorVisibleOnlyIfNeeded(found);
	}
}

void LogFlux::goToNextError()
{
	navigateToLogToken(QStringLiteral("error"));
}

void LogFlux::goToPreviousError()
{
	navigateToLogToken(QStringLiteral("error"), QTextDocument::FindBackward);
}

void LogFlux::goToNextWarning()
{
	navigateToLogToken(QStringLiteral("warn"));
}

void LogFlux::goToPreviousWarning()
{
	navigateToLogToken(QStringLiteral("warn"), QTextDocument::FindBackward);
}

void LogFlux::updateSearchCount()
{
	int total = m_searchSelections.size();
	int currentIndex = 0;

	if (!m_currentMatch.isNull())
	{
		for (int i = 0; i < m_searchSelections.size(); ++i)
		{
			const auto& sel = m_searchSelections[i];
			if (sel.cursor.selectionStart() == m_currentMatch.selectionStart() &&
				sel.cursor.selectionEnd() == m_currentMatch.selectionEnd())
			{
				currentIndex = i + 1; // 1-based
				break;
			}
		}
		ui.editSearch->setSearchInfo(currentIndex, total);
	}
	else
	{
		// No match for the current non-empty text
		if (not m_searchText.isEmpty())
			ui.editSearch->setAsNoResults();
		else
			ui.editSearch->setSearchInfo(0, 0);
	}
}

void LogFlux::addBookmark()
{
	// Toggle bookmark at the current block (line)
	QTextCursor cur = ui.plainTextEdit->textCursor();
	int blockNumber = cur.block().blockNumber();
	ui.bookmarkBar->toggleBookmark(blockNumber);
}

void LogFlux::goToNextBookmark()
{
	// Use authoritative bookmarks mapped to visible blocks
	QSet<int> visible = ui.bookmarkBar->bookmarks();
	if (visible.isEmpty())
		return;

	QList<int> list = visible.values();
	std::sort(list.begin(), list.end());

	QTextCursor cur = ui.plainTextEdit->textCursor();
	int currentBlock = cur.block().blockNumber();

	// find first bookmark strictly greater than current
	int target = -1;
	for (int b : list)
	{
		if (b > currentBlock)
		{
			target = b;
			break;
		}
	}

	// wrap to the first if none found
	if (target == -1)
		target = list.first();

	QTextBlock tb = ui.plainTextEdit->document()->findBlockByNumber(target);
	if (!tb.isValid())
		return;

	QTextCursor c(tb);
	c.movePosition(QTextCursor::StartOfBlock);
	ui.plainTextEdit->setTextCursor(c);
	ui.plainTextEdit->centerCursor();
	ensureCursorVisibleOnlyIfNeeded(c);
}

void LogFlux::goToPreviousBookmark()
{
	QSet<int> visible = ui.bookmarkBar->bookmarks();
	if (visible.isEmpty())
		return;

	QList<int> list = visible.values();
	std::sort(list.begin(), list.end());

	QTextCursor cur = ui.plainTextEdit->textCursor();
	int currentBlock = cur.block().blockNumber();

	// find last bookmark strictly less than current
	int target = -1;
	for (int i = list.size() - 1; i >= 0; --i)
	{
		if (list[i] < currentBlock)
		{
			target = list[i];
			break;
		}
	}

	// wrap to the last if none found
	if (target == -1)
		target = list.last();

	QTextBlock tb = ui.plainTextEdit->document()->findBlockByNumber(target);
	if (!tb.isValid())
		return;

	QTextCursor c(tb);
	c.movePosition(QTextCursor::StartOfBlock);
	ui.plainTextEdit->setTextCursor(c);
	ui.plainTextEdit->centerCursor();
	ensureCursorVisibleOnlyIfNeeded(c);
}

void LogFlux::setupShortcuts()
{
	new QShortcut(QKeySequence::Find, this, [this]()
		{
			ui.editSearch->setFocus();
			ui.editSearch->selectAll();
		});

	new QShortcut(Qt::Key_G, this, [this]()
		{
			goToStartOfLog();
		});

	new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_G), this, [this]()
		{
			goToEndOfLog();
		});

	new QShortcut(Qt::Key_E, this, [this]()
		{
			goToNextError();
		});

	new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_E), this, [this]()
		{
			goToPreviousError();
		});

	new QShortcut(Qt::Key_W, this, [this]()
		{
			goToNextWarning();
		});

	new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_W), this, [this]()
		{
			goToPreviousWarning();
		});

	// Scroll forward one window (page)
	new QShortcut(Qt::Key_Space, this, [this]()
		{
			QScrollBar* sb = ui.plainTextEdit->verticalScrollBar();
			if (sb)
				sb->setValue(sb->value() + sb->pageStep());
		});

	// Scroll backward one window (page)
	new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Space), this, [this]()
		{
			QScrollBar* sb = ui.plainTextEdit->verticalScrollBar();
			if (sb)
				sb->setValue(sb->value() - sb->pageStep());
		});

	new QShortcut(Qt::Key_N, this, [this]()
		{
			findNext();
		});

	new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_N), this, [this]()
		{
			findPrevious();
		});

	// Bookmarks:
	new QShortcut(Qt::Key_M, this, [this]()
		{
			addBookmark();
		});

	new QShortcut(Qt::Key_B, this, [this]()
		{
			goToNextBookmark();
		});

	new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_B), this, [this]()
		{
			goToPreviousBookmark();
		});
}

void LogFlux::setupStatusBar()
{
	statusBar()->addWidget(ui.labelOnline);
	statusBar()->addWidget(ui.labelOffline);
	statusBar()->addWidget(ui.labelSourceStatus);

	ui.labelOnline->setVisible(false);
	ui.labelOffline->setVisible(false);
	ui.labelSourceStatus->setVisible(false);

	statusBar()->addPermanentWidget(ui.labelErrorCount);
	statusBar()->addPermanentWidget(ui.labelWarningCount);
	statusBar()->addPermanentWidget(ui.labelLineCount);

	ui.labelErrorCount->setText("Errors: 0  ");
	ui.labelWarningCount->setText("Warns: 0  ");
	ui.labelLineCount->setText("Lines: 0");
}

void LogFlux::destroyExistingSource(SourceType type)
{
	if (m_sources.contains(type))
	{
		auto sourceData = m_sources[type];

		// Move the source back to the main thread so deletion happens on the GUI thread's event loop.
		// If the object is left in a worker thread whose event loop stops, deleteLater() won't run and
		// the object can be left in an invalid state.
		if (sourceData.source)
		{
			QThread* mainThread = QCoreApplication::instance()->thread();
			sourceData.source->moveToThread(mainThread);
		}

		// Ensure the QThread object is deleted after it finishes.
		connect(sourceData.thread, &QThread::finished,
			sourceData.thread, &QObject::deleteLater);

		// Tell the worker thread to stop and wait for it.
		sourceData.thread->quit();
		sourceData.thread->wait(); // Wait indefinitely 

		// signalDelagator was created on the main thread; delete it directly.
		delete sourceData.signalDelagator;
		sourceData.signalDelagator = nullptr;

		// Now the source object can be safely deleted on the main (GUI) thread.
		if (sourceData.source)
		{
			sourceData.source->deleteLater();
		}

		// Remove from map
		m_sources.remove(type);
	}
}

// Apply quick filters (error/warning OR) by rebuilding the visible document from the buffered lines.
void LogFlux::applyQuickFilters()
{
	ui.plainTextEdit->clear();
	m_visibleToAbsolute.clear();

	QVector<MiniMap::Marker> markers;

	const bool wantError = ui.checkBoxErrors->isChecked();
	const bool wantWarn = ui.checkBoxWarnings->isChecked();

	for (int absoluteIndex = 0; absoluteIndex < m_allLines.size(); ++absoluteIndex)
	{
		const QString& line = m_allLines[absoluteIndex];
		bool show = false;
		if (wantError && line.contains("error", Qt::CaseInsensitive))
			show = true;
		if (wantWarn && line.contains("warn", Qt::CaseInsensitive))
			show = true;

		if (!show)
			continue;

		QTextCharFormat fmt = formatForLine(line);

		QTextCursor insertCursor(ui.plainTextEdit->document());
		insertCursor.movePosition(QTextCursor::End);
		insertCursor.insertText(line + "\n", fmt);

		m_visibleToAbsolute.append(absoluteIndex);

		int blockNumber = ui.plainTextEdit->document()->blockCount() - 1;
		if (line.contains("warn", Qt::CaseInsensitive))
		{
			markers.append({ blockNumber, MiniMap::Warning });
		}
		else if (line.contains("error", Qt::CaseInsensitive))
		{
			markers.append({ blockNumber, MiniMap::Error });
		}
		// bookmark markers for visible bookmarked absolute lines
		if (m_bookmarks.contains(absoluteIndex))
		{
			markers.append({ blockNumber, MiniMap::Bookmark });
		}
	}

	// Merge bookmarks
	QSet<int> visibleBookmarks;
	for (int visibleBlock = 0; visibleBlock < m_visibleToAbsolute.size(); ++visibleBlock)
	{
		if (m_bookmarks.contains(m_visibleToAbsolute[visibleBlock]))
			visibleBookmarks.insert(visibleBlock);
	}

	ui.bookmarkBar->setBookmarks(visibleBookmarks);
	ui.scrollbarMinimap->setMarkers(markers);

	// Notify gutter that mapping changed
	ui.lineNumberArea->refresh();

	// Update search highlights (if any search text active)
	if (!m_searchText.isEmpty())
		highlightAllMatches(m_searchText);
	else
		updateSelections();
}

// Called when either quick-filter checkbox changes state.
void LogFlux::onQuickFiltersChanged(bool /*checked*/)
{
	const bool err = ui.checkBoxErrors->isChecked();
	const bool warn = ui.checkBoxWarnings->isChecked();

	// If any quick filter is selected, disable normal filters (uncheck checkBoxFilters)
	// and switch to quick filter mode.
	if (err || warn)
	{
		// Uncheck normal filters if enabled. This will call filtersEnabled(false) and
		// backup the current filters.
		if (ui.checkBoxFilters->isChecked())
		{
			// Allow filtersEnabled(false) to run (don't block), it will call filtersChanged({})
			ui.checkBoxFilters->setChecked(false);
		}

		m_useQuickFilters = true;
		applyQuickFilters();
	}
	else
	{
		// No quick filters selected anymore: stop using quick filters and show everything
		// according to the current normal-filter state (which is currently disabled).
		if (m_useQuickFilters)
		{
			m_useQuickFilters = false;
			// Rebuild view with no normal filters (filters were disabled when quick filters were activated).
			// Keep current m_filters empty; just call filtersChanged({}) so logic is consistent.
			filtersChanged({});
		}
	}
}

void LogFlux::onBookmarkToggled(int visibleBlockNumber, bool enabled)
{
	if (visibleBlockNumber < 0 || visibleBlockNumber >= m_visibleToAbsolute.size())
		return;

	int absoluteIndex = m_visibleToAbsolute[visibleBlockNumber];

	if (enabled)
		m_bookmarks.insert(absoluteIndex);
	else
		m_bookmarks.remove(absoluteIndex);

	// Recompute visible bookmarks and update the bookmark view so it shows only bookmarks for visible lines.
	QSet<int> visibleBookmarks;
	for (int visible = 0; visible < m_visibleToAbsolute.size(); ++visible)
	{
		if (m_bookmarks.contains(m_visibleToAbsolute[visible]))
			visibleBookmarks.insert(visible);
	}

	ui.bookmarkBar->setBookmarks(visibleBookmarks);
	ui.scrollbarMinimap->setBookmark(visibleBlockNumber, enabled);
}

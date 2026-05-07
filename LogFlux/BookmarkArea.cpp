#include "BookmarkArea.h"
#include "LogTextEdit.h"

#include <QPainter>
#include <QMouseEvent>
#include <QTextBlock>

BookmarkArea::BookmarkArea(QWidget* parent)
	: QWidget(parent)
{
}

void BookmarkArea::setEditor(LogTextEdit* editor)
{
	m_editor = editor;

	connect(editor, &QPlainTextEdit::updateRequest,
		this, &BookmarkArea::updateArea);

	connect(editor, &QPlainTextEdit::blockCountChanged,
		this, &BookmarkArea::updateWidth);

	update();
}

void BookmarkArea::setBookmarkIcon(const QPixmap& icon)
{
	m_icon = icon;
	update();
}

bool BookmarkArea::hasBookmark(int blockNumber) const
{
	return m_bookmarks.contains(blockNumber);
}

void BookmarkArea::toggleBookmark(int blockNumber)
{
	if (m_bookmarks.contains(blockNumber))
	{
		m_bookmarks.remove(blockNumber);
		emit bookmarkToggled(blockNumber, false);
	}
	else
	{
		m_bookmarks.insert(blockNumber);
		emit bookmarkToggled(blockNumber, true);
	}

	update();
}

void BookmarkArea::setBookmark(int blockNumber, bool enabled)
{
	if (enabled)
	{
		if (!m_bookmarks.contains(blockNumber))
		{
			m_bookmarks.insert(blockNumber);
			emit bookmarkToggled(blockNumber, true);
		}
	}
	else
	{
		if (m_bookmarks.remove(blockNumber))
		{
			emit bookmarkToggled(blockNumber, false);
		}
	}

	update();
}

void BookmarkArea::clearBookmark(int blockNumber)
{
	setBookmark(blockNumber, false);
}

void BookmarkArea::clearAllBookmarks()
{
	if (m_bookmarks.isEmpty())
		return;

	m_bookmarks.clear();
	update();
}

QSet<int> BookmarkArea::bookmarks() const
{
	return m_bookmarks;
}

void BookmarkArea::setBookmarks(const QSet<int>& bookmarks)
{
	// Update view-only bookmark set without emitting toggle signals.
	if (m_bookmarks == bookmarks)
		return;

	m_bookmarks = bookmarks;
	update();
}

void BookmarkArea::updateWidth()
{
	// Intentionally empty:
	// Width is controlled by Qt Designer layout
}

void BookmarkArea::updateArea(const QRect& rect, int dy)
{
	if (dy)
		scroll(0, dy);
	else
		update(0, rect.y(), width(), rect.height());
}

void BookmarkArea::mousePressEvent(QMouseEvent* event)
{
	if (!m_editor)
		return;

	int blockNumber = blockNumberAtY(event->pos().y());
	if (blockNumber < 0)
		return;

	toggleBookmark(blockNumber);
}

void BookmarkArea::paintEvent(QPaintEvent* event)
{
	if (!m_editor)
		return;

	QPainter painter(this);
	painter.fillRect(event->rect(), QColor(45, 45, 45));

	QTextBlock block = m_editor->getFirstVisibleBlock();
	int blockNumber = block.blockNumber();

	int top = static_cast<int>(
		m_editor->getBlockBoundingGeometry(block)
		.translated(m_editor->getContentOffset()).top()
		);

	int bottom = top + static_cast<int>(
		m_editor->getBlockBoundingRect(block).height()
		);

	// IMPORTANT: icon size depends ONLY on available widget height
	const int iconSize = qMin(width(), m_editor->fontMetrics().height());

	while (block.isValid() && top <= event->rect().bottom())
	{
		if (block.isVisible() && bottom >= event->rect().top())
		{
			if (m_bookmarks.contains(blockNumber) && !m_icon.isNull())
			{
				QPixmap scaled = m_icon.scaled(
					iconSize,
					iconSize,
					Qt::KeepAspectRatio,
					Qt::SmoothTransformation
				);

				int x = (width() - scaled.width()) / 2;
				int y = top + (m_editor->fontMetrics().height() - scaled.height()) / 2;

				painter.drawPixmap(x, y, scaled);
			}
		}

		block = block.next();
		top = bottom;
		bottom = top + static_cast<int>(
			m_editor->getBlockBoundingRect(block).height()
			);
		++blockNumber;
	}
}

int BookmarkArea::blockNumberAtY(int y) const
{
	if (!m_editor)
		return -1;

	QTextBlock block = m_editor->getFirstVisibleBlock();
	int blockNumber = block.blockNumber();

	int top = static_cast<int>(
		m_editor->getBlockBoundingGeometry(block)
		.translated(m_editor->getContentOffset()).top()
		);

	int bottom = top + static_cast<int>(
		m_editor->getBlockBoundingRect(block).height()
		);

	while (block.isValid())
	{
		if (y >= top && y <= bottom)
			return blockNumber;

		block = block.next();
		top = bottom;
		bottom = top + static_cast<int>(
			m_editor->getBlockBoundingRect(block).height()
			);
		++blockNumber;
	}

	return -1;
}
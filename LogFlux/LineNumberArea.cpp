#include "LineNumberArea.h"
#include "LogTextEdit.h"

#include <QPainter>
#include <QTextBlock>

LineNumberArea::LineNumberArea(QWidget* parent)
	: QWidget(parent)
{
}

void LineNumberArea::setEditor(LogTextEdit* editor)
{
	m_editor = editor;

	connect(editor, &QPlainTextEdit::updateRequest,
		this, &LineNumberArea::updateArea);

	connect(editor, &QPlainTextEdit::blockCountChanged,
		this, &LineNumberArea::updateWidth);

	updateWidth();
}

int LineNumberArea::calculateWidth() const
{
	if (!m_editor) return 0;

	int digits = QString::number(m_editor->blockCount()).length();
	return 10 + m_editor->fontMetrics().horizontalAdvance('9') * digits;
}

void LineNumberArea::updateWidth()
{
	setFixedWidth(calculateWidth());
	update();
}

void LineNumberArea::updateArea(const QRect& rect, int dy)
{
	if (dy)
		scroll(0, dy);
	else
		update(0, rect.y(), width(), rect.height());
}

void LineNumberArea::paintEvent(QPaintEvent* event)
{
	if (!m_editor)
		return;

	QPainter painter(this);
	painter.fillRect(event->rect(), QColor(30, 30, 30));

	QTextBlock block = m_editor->getFirstVisibleBlock();
	int blockNumber = block.blockNumber();

	int top = static_cast<int>(
		m_editor->getBlockBoundingGeometry(block)
		.translated(m_editor->getContentOffset()).top()
		);

	int bottom = top + static_cast<int>(
		m_editor->getBlockBoundingRect(block).height()
		);

	while (block.isValid() && top <= event->rect().bottom())
	{
		if (block.isVisible() && bottom >= event->rect().top())
		{
			painter.setPen(Qt::gray);
			painter.drawText(0,
				top,
				width() - 5,
				m_editor->fontMetrics().height(),
				Qt::AlignRight,
				QString::number(blockNumber + 1));
		}

		block = block.next();
		top = bottom;
		bottom = top + static_cast<int>(
			m_editor->getBlockBoundingRect(block).height()
			);
		++blockNumber;
	}
}
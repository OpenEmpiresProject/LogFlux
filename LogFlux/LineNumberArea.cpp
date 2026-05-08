#include "LineNumberArea.h"

#include "LogTextEdit.h"

#include <QPainter>
#include <QTextBlock>
#include <algorithm>

LineNumberArea::LineNumberArea(QWidget* parent) : QWidget(parent)
{
}

void LineNumberArea::setEditor(LogTextEdit* editor)
{
    m_editor = editor;

    connect(editor, &QPlainTextEdit::updateRequest, this, &LineNumberArea::updateArea);

    connect(editor, &QPlainTextEdit::blockCountChanged, this, &LineNumberArea::updateWidth);

    updateWidth();
}

void LineNumberArea::setVisibleToAbsoluteMap(const QVector<int>* map)
{
    m_visibleToAbsolute = map;
    // Mapping change can affect gutter width (number of digits), so refresh.
    refresh();
}

void LineNumberArea::refresh()
{
    updateWidth();
    update();
}

int LineNumberArea::calculateWidth() const
{
    if (!m_editor)
        return 0;

    // Determine the number that should be used to compute digit width:
    // - prefer the maximum absolute line number from the mapping (if provided),
    //   because filtered views can show high-numbered lines (e.g., line 9999 of 10000)
    //   and the gutter must be wide enough to display them without truncation
    // - otherwise fall back to the number of visible blocks
    int maxNumber = m_editor->blockCount();
    if (m_visibleToAbsolute && !m_visibleToAbsolute->isEmpty())
    {
        int maxAbs =
            *std::max_element(m_visibleToAbsolute->constBegin(), m_visibleToAbsolute->constEnd()) +
            1;
        maxNumber = std::max(maxNumber, maxAbs);
    }

    int digits = QString::number(maxNumber).length();
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
    painter.fillRect(event->rect(), QColor(45, 45, 45));

    QTextBlock block = m_editor->getFirstVisibleBlock();
    int blockNumber = block.blockNumber();

    int top = static_cast<int>(
        m_editor->getBlockBoundingGeometry(block).translated(m_editor->getContentOffset()).top());

    int bottom = top + static_cast<int>(m_editor->getBlockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            // Show the original (absolute) line number when a mapping is available
            // so the gutter reflects the position in the full unfiltered log,
            // not the position within the current filtered view.
            QString numberText;
            if (m_visibleToAbsolute && blockNumber >= 0 &&
                blockNumber < m_visibleToAbsolute->size())
            {
                numberText = QString::number((*m_visibleToAbsolute)[blockNumber] + 1);
            }
            else
            {
                numberText = QString::number(blockNumber + 1);
            }

            painter.setPen(Qt::gray);
            painter.drawText(0, top, width() - 5, m_editor->fontMetrics().height(), Qt::AlignRight,
                             numberText);
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(m_editor->getBlockBoundingRect(block).height());
        ++blockNumber;
    }
}
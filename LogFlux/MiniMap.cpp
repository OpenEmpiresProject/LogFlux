#include "MiniMap.h"

#include "LogTextEdit.h"

#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextDocument>
#include <algorithm>

MiniMap::MiniMap(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
}

void MiniMap::setEditor(LogTextEdit* editor)
{
    m_editor = editor;

    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { update(); });

    connect(editor, &QPlainTextEdit::updateRequest, this, [this](const QRect&, int) { update(); });

    update();
}

void MiniMap::setMarkers(const QVector<Marker>& markers)
{
    m_markers = markers;
    update();
}

void MiniMap::addMarker(int blockNumber, MarkerType type)
{
    if (blockNumber < 0)
        return;

    // Avoid duplicates
    for (const auto& m : m_markers)
    {
        if (m.blockNumber == blockNumber && m.type == type)
            return;
    }

    m_markers.append({blockNumber, type});
    update();
}

void MiniMap::removeMarker(int blockNumber, MarkerType type)
{
    bool changed = false;
    for (int i = m_markers.size() - 1; i >= 0; --i)
    {
        if (m_markers[i].blockNumber == blockNumber && m_markers[i].type == type)
        {
            m_markers.removeAt(i);
            changed = true;
        }
    }
    if (changed)
        update();
}

void MiniMap::clearMarkers()
{
    if (!m_markers.isEmpty())
    {
        m_markers.clear();
        update();
    }
}

void MiniMap::setBookmark(int blockNumber, bool enabled)
{
    if (enabled)
        addMarker(blockNumber, Bookmark);
    else
        removeMarker(blockNumber, Bookmark);
}

void MiniMap::setBookmarks(const QSet<int>& bookmarks)
{
    // Remove existing bookmark markers
    bool removed = false;
    for (int i = m_markers.size() - 1; i >= 0; --i)
    {
        if (m_markers[i].type == Bookmark)
        {
            m_markers.removeAt(i);
            removed = true;
        }
    }

    // Add bookmarks from the set
    for (int b : bookmarks)
    {
        if (b >= 0)
            m_markers.append({b, Bookmark});
    }

    if (removed || !bookmarks.isEmpty())
        update();
}

void MiniMap::updateViewportRect()
{
    if (!m_editor)
        return;

    QScrollBar* sb = m_editor->verticalScrollBar();

    // The full scroll range is maximum + pageStep, not just maximum.
    // maximum() alone represents the last scroll position, but the viewport
    // still covers one pageStep worth of content from there, so we must
    // include it to get the correct proportional size of the viewport rect.
    int fullRange = sb->maximum() + sb->pageStep();
    if (fullRange <= 0)
        return;

    double topRatio = double(sb->value()) / fullRange;
    double bottomRatio = double(sb->value() + sb->pageStep()) / fullRange;

    int y = int(topRatio * height());
    int h = int((bottomRatio - topRatio) * height());
    h = std::max(h, 2);

    m_viewportRect = QRect(0, y, width(), h);
}

void MiniMap::paintEvent(QPaintEvent*)
{
    if (!m_editor)
        return;

    QPainter p(this);
    p.fillRect(rect(), QColor(45, 45, 45));

    int totalBlocks = m_editor->document()->blockCount();
    if (totalBlocks == 0)
        return;

    double scale = double(height()) / totalBlocks;

    // Markers: drawn on top as thick filled rects so they stay prominent
    // regardless of document length always at least kMarkerMinHeight px tall.
    constexpr int kMarkerMinHeight = 1;
    p.setPen(Qt::NoPen);
    for (const auto& m : m_markers)
    {
        QColor color;
        switch (m.type)
        {
        case Bookmark:
            color = QColor(80, 180, 255);
            break; // bright blue
        case Warning:
            color = QColor(255, 253, 85);
            break; // orange
        case Error:
            color = QColor(220, 50, 50);
            break; // red
        default:
            continue;
        }

        // Block numbers could be out of range after a document rebuild
        // (e.g., stale bookmarks that haven't been remapped yet).
        // Clamp to the last block so the marker stays visible at the bottom
        // of the minimap rather than being silently dropped.
        int blockNum = m.blockNumber;
        if (blockNum < 0)
            continue;
        if (blockNum >= totalBlocks)
            blockNum = totalBlocks - 1;

        int y = int(blockNum * scale);
        int h = std::max(kMarkerMinHeight, int(scale));
        p.fillRect(0, y, width(), h, color);
    }

    // Viewport highlight overlay (i.e. scrollbar handle) — hidden when the entire document fits
    // in the window (scrollbar maximum == 0 means nothing to scroll).
    if (m_editor->verticalScrollBar()->maximum() > 0)
    {
        updateViewportRect();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(100, 100, 255, 80));
        p.drawRect(m_viewportRect);
    }
}

int MiniMap::blockAtY(int y) const
{
    if (!m_editor)
        return -1;

    int totalBlocks = m_editor->document()->blockCount();
    if (totalBlocks == 0)
        return 0;

    double scale = double(height()) / totalBlocks;
    int block = int(y / scale);

    if (block < 0)
        block = 0;
    if (block >= totalBlocks)
        block = totalBlocks - 1;
    return block;
}

void MiniMap::scrollEditorToBlock(int blockNumber)
{
    if (!m_editor)
        return;

    // Set the scrollbar value directly rather than moving the text cursor.
    // Moving the cursor would trigger onCursorPositionChanged, which stops tailing
    // and changes the user's selection — standard scrollbar behaviour should not do that.
    QScrollBar* sb = m_editor->verticalScrollBar();
    int totalBlocks = m_editor->document()->blockCount();
    if (sb->maximum() <= 0 || totalBlocks <= 0)
        return;

    double ratio = double(blockNumber) / totalBlocks;
    sb->setValue(int(ratio * sb->maximum()));
}

void MiniMap::mousePressEvent(QMouseEvent* e)
{
    m_dragging = true;
    scrollEditorToBlock(blockAtY(int(e->position().y())));
}

void MiniMap::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging)
        return;

    scrollEditorToBlock(blockAtY(int(e->position().y())));
}

void MiniMap::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
}
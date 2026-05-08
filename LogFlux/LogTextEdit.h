#pragma once

#include <QPlainTextEdit>
#include <QTextBlock>

class LogTextEdit : public QPlainTextEdit
{
    Q_OBJECT

  public:
    explicit LogTextEdit(QWidget* parent = nullptr);

    // Qt declares these methods protected in QPlainTextEdit to discourage general use,
    // but gutter widgets (BookmarkArea, LineNumberArea) need them to align their own
    // painting with the editor's scroll position. Exposing them here.
    QTextBlock getFirstVisibleBlock() const;
    QPointF getContentOffset() const;
    QRectF getBlockBoundingGeometry(const QTextBlock& block) const;
    QRectF getBlockBoundingRect(const QTextBlock& block) const;
};
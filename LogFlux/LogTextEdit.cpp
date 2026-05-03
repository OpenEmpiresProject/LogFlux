#include "LogTextEdit.h"

LogTextEdit::LogTextEdit(QWidget* parent)
	: QPlainTextEdit(parent)
{
}

// Wrappers
QTextBlock LogTextEdit::getFirstVisibleBlock() const
{
	return firstVisibleBlock();
}

QPointF LogTextEdit::getContentOffset() const
{
	return contentOffset();
}

QRectF LogTextEdit::getBlockBoundingGeometry(const QTextBlock& block) const
{
	return blockBoundingGeometry(block);
}

QRectF LogTextEdit::getBlockBoundingRect(const QTextBlock& block) const
{
	return blockBoundingRect(block);
}
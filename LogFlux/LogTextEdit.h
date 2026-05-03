#pragma once

#include <QPlainTextEdit>
#include <QTextBlock>

class LogTextEdit : public QPlainTextEdit
{
	Q_OBJECT

public:
	explicit LogTextEdit(QWidget* parent = nullptr);

	// Expose protected APIs safely
	QTextBlock getFirstVisibleBlock() const;
	QPointF getContentOffset() const;
	QRectF getBlockBoundingGeometry(const QTextBlock& block) const;
	QRectF getBlockBoundingRect(const QTextBlock& block) const;
};
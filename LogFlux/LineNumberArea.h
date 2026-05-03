#pragma once

#include <QWidget>

class LogTextEdit;

class LineNumberArea : public QWidget
{
	Q_OBJECT

public:
	explicit LineNumberArea(QWidget* parent = nullptr);

	void setEditor(LogTextEdit* editor);

protected:
	void paintEvent(QPaintEvent* event) override;

private slots:
	void updateArea(const QRect& rect, int dy);
	void updateWidth();

private:
	int calculateWidth() const;

private:
	LogTextEdit* m_editor = nullptr;
};
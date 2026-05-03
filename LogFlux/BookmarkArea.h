#pragma once

#include <QWidget>
#include <QSet>
#include <QPixmap>

class LogTextEdit;

class BookmarkArea : public QWidget
{
	Q_OBJECT

public:
	explicit BookmarkArea(QWidget* parent = nullptr);

	void setEditor(LogTextEdit* editor);
	void setBookmarkIcon(const QPixmap& icon);

	void toggleBookmark(int blockNumber);
	void setBookmark(int blockNumber, bool enabled);
	void clearBookmark(int blockNumber);
	void clearAllBookmarks();

	bool hasBookmark(int blockNumber) const;
	QSet<int> bookmarks() const;

signals:
	void bookmarkToggled(int blockNumber, bool enabled);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;

private slots:
	void updateArea(const QRect& rect, int dy);
	void updateWidth();

private:
	int calculateWidth() const;
	int blockNumberAtY(int y) const;

private:
	LogTextEdit* m_editor = nullptr;
	QSet<int> m_bookmarks;
	QPixmap m_icon;
};
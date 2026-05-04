#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>
#include <QRect>
#include <QSet>

class LogTextEdit;

class MiniMap : public QWidget
{
	Q_OBJECT

public:
	explicit MiniMap(QWidget* parent = nullptr);

	void setEditor(LogTextEdit* editor);

	enum MarkerType {
		None,
		Bookmark,
		Warning,
		Error
	};

	struct Marker {
		int blockNumber;
		MarkerType type;
	};

	// replace all markers (keeps callers that rebuild full marker lists)
	void setMarkers(const QVector<Marker>& markers);

public slots:
	// incremental updates for dynamic scenarios
	void addMarker(int blockNumber, MarkerType type);
	void removeMarker(int blockNumber, MarkerType type);
	void clearMarkers();

	// bookmark updates (single and bulk). Matches BookmarkArea::bookmarkToggled signature.
	void setBookmark(int blockNumber, bool enabled);
	void setBookmarks(const QSet<int>& bookmarks);

protected:
	void paintEvent(QPaintEvent* event)    override;
	void mousePressEvent(QMouseEvent* e)   override;
	void mouseMoveEvent(QMouseEvent* e)    override;
	void mouseReleaseEvent(QMouseEvent* e) override;

private:
	int  blockAtY(int y) const;
	void updateViewportRect();
	void scrollEditorToBlock(int blockNumber); // scrolls viewport only, no cursor move

private:
	LogTextEdit* m_editor = nullptr;
	QVector<Marker> m_markers;
	QRect           m_viewportRect;
	bool            m_dragging = false;
};
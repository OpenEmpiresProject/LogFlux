#pragma once

#include <QWidget>

class LogTextEdit;

class LineNumberArea : public QWidget
{
    Q_OBJECT

  public:
    explicit LineNumberArea(QWidget* parent = nullptr);

    void setEditor(LogTextEdit* editor);

    // Provide access to the mapping maintained by LogFlux so line numbers
    // shown in the gutter correspond to original (absolute) line numbers.
    void setVisibleToAbsoluteMap(const QVector<int>* map);

    // Force recalculation of width and repaint (call after mapping or content changes).
    void refresh();

  protected:
    void paintEvent(QPaintEvent* event) override;

  private slots:
    void updateArea(const QRect& rect, int dy);
    void updateWidth();

  private:
    int calculateWidth() const;

  private:
    LogTextEdit* m_editor = nullptr;
    // Pointer to authoritative mapping: visible block index -> absolute line index.
    const QVector<int>* m_visibleToAbsolute = nullptr;
};
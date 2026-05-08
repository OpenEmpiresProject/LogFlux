#pragma once
#include <QLabel>
#include <QLineEdit>

class SearchLineEdit : public QLineEdit
{
    Q_OBJECT
  public:
    explicit SearchLineEdit(QWidget* parent = nullptr) : QLineEdit(parent)
    {
        m_info = new QLabel(this);
        m_info->setStyleSheet("QLabel { color: gray; padding: 0 6px; }");
        m_info->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        setStyleSheet(R"(
QLineEdit {
	placeholder-text-color: gray;
}
)");
        // Reserve space on the right so typed text never flows under the count label.
        setTextMargins(0, 0, 60, 0);
    }

    void setAsNoResults()
    {
        m_info->setText("");
        setStyleSheet(R"(
QLineEdit {
    border: 1px solid red;
	padding: 2px;
	placeholder-text-color: gray;
}
)");
    }

    void setSearchInfo(int current, int total)
    {
        m_info->setText(QString("%1/%2").arg(current).arg(total));
        m_info->adjustSize();
        setStyleSheet(R"(
QLineEdit {
	placeholder-text-color: gray;
}
)");

        updateMargins();
        updateInfoGeometry();
    }

  protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QLineEdit::resizeEvent(e);
        updateInfoGeometry();
    }

  private:
    void updateInfoGeometry()
    {
        int w = m_info->sizeHint().width();
        int h = m_info->sizeHint().height();
        int x = width() - w - 4;
        int y = (height() - h) / 2;
        m_info->setGeometry(x, y, w, h);
    }

    void updateMargins()
    {
        int w = m_info->sizeHint().width();
        // Keep the right margin in sync with the actual label width so the
        // text cursor never overlaps the count display as the number grows.
        setTextMargins(0, 0, w + 10, 0);
    }

    QLabel* m_info = nullptr;
};
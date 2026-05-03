#pragma once

#include <QWidget>
#include <QLayout>
#include <QLayoutItem>
#include <QList>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QLineEdit>
#include <QKeyEvent>
#include <QStringList>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>

// ============================================================================
// FlowLayout
// ============================================================================
class FlowLayout : public QLayout
{
public:
	explicit FlowLayout(QWidget* parent = nullptr, int margin = 4, int spacing = 4)
		: QLayout(parent)
	{
		setContentsMargins(margin, margin, margin, margin);
		setSpacing(spacing);
	}

	~FlowLayout()
	{
		QLayoutItem* item;
		while ((item = takeAt(0)))
			delete item;
	}

	void addItem(QLayoutItem* item) override { m_items.append(item); }
	int count() const override { return m_items.size(); }
	QLayoutItem* itemAt(int i) const override { return m_items.value(i); }
	QLayoutItem* takeAt(int i) override
	{
		return (i >= 0 && i < m_items.size()) ? m_items.takeAt(i) : nullptr;
	}

	void removeWidget(QWidget* w)
	{
		for (int i = 0; i < m_items.size(); ++i)
		{
			if (m_items[i]->widget() == w)
			{
				delete m_items.takeAt(i);
				return;
			}
		}
	}

	Qt::Orientations expandingDirections() const override { return {}; }
	bool hasHeightForWidth() const override { return true; }
	int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
	QSize sizeHint() const override { return minimumSize(); }

	QSize minimumSize() const override
	{
		QSize size;
		for (auto* item : m_items)
			size = size.expandedTo(item->minimumSize());
		const auto m = contentsMargins();
		size += QSize(m.left() + m.right(), m.top() + m.bottom());
		return size;
	}

	void setGeometry(const QRect& rect) override
	{
		QLayout::setGeometry(rect);
		doLayout(rect, false);
	}

private:
	int doLayout(const QRect& rect, bool testOnly) const
	{
		const auto m = contentsMargins();
		const QRect r = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());

		int x = r.x(), y = r.y(), lineHeight = 0;

		for (auto* item : m_items)
		{
			QWidget* w = item->widget();
			if (!w || !w->isVisible())
				continue;

			const QSize sz = item->sizeHint();
			if (x > r.x() && x + sz.width() > r.right() + 1)
			{
				x = r.x();
				y += lineHeight + spacing();
				lineHeight = 0;
			}

			if (!testOnly)
				item->setGeometry(QRect(QPoint(x, y), sz));

			x += sz.width() + spacing();
			lineHeight = qMax(lineHeight, sz.height());
		}

		return y + lineHeight - rect.y() + m.top() + m.bottom();
	}

	QList<QLayoutItem*> m_items;
};

// ============================================================================
// TagChip  (internal — not promoted directly)
// ============================================================================
class TagChip : public QWidget
{
	Q_OBJECT
public:
	explicit TagChip(const QString& text, QWidget* parent = nullptr)
		: QWidget(parent), m_text(text)
	{
		auto* hbox = new QHBoxLayout(this);
		hbox->setContentsMargins(10, 2, 5, 4);   // 1px less top/bottom → shorter chip
		hbox->setSpacing(4);
		hbox->setAlignment(Qt::AlignVCenter);      // keep children vertically centred

		m_label = new QLabel(text, this);
		m_label->setAlignment(Qt::AlignVCenter);

		m_close = new QToolButton(this);
		m_close->setText("\xc3\x97"); // × U+00D7
		m_close->setCursor(Qt::PointingHandCursor);
		m_close->setFixedSize(16, 16);             // slightly smaller, easier to centre
		m_close->setAutoRaise(true);

		hbox->addWidget(m_label, 0, Qt::AlignVCenter);
		hbox->addWidget(m_close, 0, Qt::AlignVCenter);

		// Lock height to exactly what the layout needs so no invisible pixels
		// bleed outside the painted border and overlap neighbouring chips.
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		adjustSize();
		setFixedHeight(sizeHint().height());
		refreshStyle();

		connect(m_close, &QToolButton::clicked, this, [this]() { emit removed(this); });
	}

	QString text() const { return m_text; }

signals:
	void removed(TagChip*);

protected:
	void changeEvent(QEvent* e) override
	{
		QWidget::changeEvent(e);
		if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange)
			refreshStyle();
	}

	void paintEvent(QPaintEvent*) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);

		const QPalette& pal = palette();
		QColor bg = pal.color(QPalette::Window);
		bg = bg.lighter(bg.lightnessF() < 0.5f ? 130 : 92);

		// 0.5px inset keeps the 1px stroke fully inside the widget rect
		// so it never bleeds into the spacing gap between chips.
		const QRectF chipRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		QPainterPath chipPath;
		chipPath.addRoundedRect(chipRect, 10, 10);
		p.fillPath(chipPath, bg);
		p.setPen(QPen(pal.color(QPalette::Mid), 1));
		p.drawPath(chipPath);
	}

private:
	void refreshStyle()
	{
		const QPalette& pal = palette();
		m_label->setStyleSheet(
			QString("QLabel { color: %1; font-size: 13px; background: transparent; }")
			.arg(pal.color(QPalette::WindowText).name()));
		m_close->setStyleSheet(
			QString(R"(
                QToolButton { color: %1; background: transparent; border: none;
                              font-size: 15px; line-height: 16px;
                              padding: 0; margin: 0 0 2px 0; }
                QToolButton:hover { color: %2; }
            )")
			.arg(pal.color(QPalette::PlaceholderText).name())
			.arg(pal.color(QPalette::WindowText).name()));
	}

	QLabel* m_label = nullptr;
	QToolButton* m_close = nullptr;
	QString      m_text;
};

// ============================================================================
// TagBar  — promote a QWidget to this
//
// Displays chips in a wrapping flow layout.
// Wire up in your parent widget's constructor:
//
//   connect(ui->lineEdit, &TagLineEdit::tagEntered,
//           ui->tagBar,   &TagBar::addTag);
//   connect(ui->lineEdit, &TagLineEdit::backspaceOnEmpty,
//           ui->tagBar,   &TagBar::removeLastTag);
// ============================================================================
class TagBar : public QWidget
{
	Q_OBJECT
public:
	explicit TagBar(QWidget* parent = nullptr)
		: QWidget(parent)
	{
		setMinimumHeight(32);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		m_layout = new FlowLayout(this, 4, 4);
	}

	QStringList tags() const
	{
		QStringList out;
		for (auto* chip : m_chips)
			out << chip->text();
		return out;
	}

signals:
	void tagsChanged(const QStringList&);

public slots:
	void addTag(const QString& rawText)
	{
		const QString t = rawText.trimmed();
		if (t.isEmpty() || tags().contains(t, Qt::CaseInsensitive))
			return;

		auto* chip = new TagChip(t, this);
		chip->show();
		m_layout->addWidget(chip);
		m_chips.append(chip);

		connect(chip, &TagChip::removed, this, [this](TagChip* c) {
			m_chips.removeOne(c);
			m_layout->removeWidget(c);
			c->deleteLater();
			m_layout->invalidate();
			adjustSize();
			updateGeometry();
			emit tagsChanged(tags());
			});

		m_layout->invalidate();
		adjustSize();
		updateGeometry();
		emit tagsChanged(tags());
	}

	void removeLastTag()
	{
		if (m_chips.isEmpty())
			return;

		auto* chip = m_chips.takeLast();
		m_layout->removeWidget(chip);
		chip->deleteLater();
		m_layout->invalidate();
		adjustSize();
		updateGeometry();
		emit tagsChanged(tags());
	}

	void clearTags()
	{
		for (auto* chip : m_chips)
		{
			m_layout->removeWidget(chip);
			chip->deleteLater();
		}
		m_chips.clear();
		m_layout->invalidate();
		adjustSize();
		updateGeometry();
		emit tagsChanged({});
	}

private:
	FlowLayout* m_layout = nullptr;
	QList<TagChip*> m_chips;
};

// ============================================================================
// TagLineEdit  — promote a QLineEdit to this
//
// Enter / Return / Tab / Comma / Space  →  emit tagEntered(text)
// Backspace on empty text              →  emit backspaceOnEmpty()
// ============================================================================
class TagLineEdit : public QLineEdit
{
	Q_OBJECT
public:
	using QLineEdit::QLineEdit;

signals:
	void tagEntered(const QString&);
	void backspaceOnEmpty();

protected:
	void keyPressEvent(QKeyEvent* e) override
	{
		const bool hardCommit = e->key() == Qt::Key_Return
			|| e->key() == Qt::Key_Enter
			|| e->key() == Qt::Key_Tab;

		if (hardCommit)
		{
			const QString t = text().trimmed();
			if (!t.isEmpty())
				emit tagEntered(t);
			clear();
			e->accept();
			return;
		}

		if (e->key() == Qt::Key_Backspace && text().isEmpty())
		{
			emit backspaceOnEmpty();
			e->accept();
			return;
		}

		QLineEdit::keyPressEvent(e);
	}
};
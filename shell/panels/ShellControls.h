/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_SHELLCONTROLS_H
#define ARTFLOW_SHELLCONTROLS_H

#include <QIcon>
#include <QStyledItemDelegate>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

/**
 * The Flip / Motion / Studio switch: one rounded track with a highlight
 * that slides to the chosen mode.
 */
class ModeSwitch : public QWidget
{
    Q_OBJECT
public:
    struct Segment {
        QString id;
        QString title;
        QString toolTip;
    };

    explicit ModeSwitch(QWidget *parent = nullptr);

    void setSegments(const QVector<Segment> &segments);
    void setCurrent(const QString &id);

    QSize sizeHint() const override;

Q_SIGNALS:
    void segmentClicked(const QString &id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    QRectF segmentRect(int index) const;
    int segmentAt(const QPoint &pos) const;

    QVector<Segment> m_segments;
    int m_current {-1};
    int m_hovered {-1};
    qreal m_knob {0.0};  ///< animated position of the highlight, in segments
    QVariantAnimation m_slide;
};

/**
 * A tall rounded slider with the value written inside, used beside the
 * canvas for brush size and opacity. Drag anywhere on it, or scroll.
 */
class PillSlider : public QWidget
{
    Q_OBJECT
public:
    explicit PillSlider(QWidget *parent = nullptr);

    void setRange(int minimum, int maximum);
    void setValue(int value);
    int value() const;

    void setIcon(const QIcon &icon);
    void setValueText(const QString &text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF trackRect() const;
    void setFromY(qreal y);

    int m_min {0};
    int m_max {100};
    int m_value {0};
    QIcon m_icon;
    QString m_text;
    bool m_dragging {false};
    bool m_hovered {false};
};

/**
 * Paints the engine's colour palette as rounded swatches: a grid of tiles
 * with a ring around the chosen one, or rows with the colour's name.
 */
class SwatchDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SwatchDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    static bool isGrid(const QStyleOptionViewItem &option);
};

#endif // ARTFLOW_SHELLCONTROLS_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ShellControls.h"

#include <QAbstractItemView>
#include <QEnterEvent>
#include <QHelpEvent>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>

#include "ArtflowTheme.h"

// ---------------------------------------------------------------- ModeSwitch

namespace {
const int SwitchHeight = 36;
const int SwitchPadding = 3;
const int SegmentWidth = 76;
}

ModeSwitch::ModeSwitch(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_slide.setDuration(180);
    m_slide.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_knob = value.toReal();
        update();
    });
}

void ModeSwitch::setSegments(const QVector<Segment> &segments)
{
    m_segments = segments;
    updateGeometry();
    update();
}

void ModeSwitch::setCurrent(const QString &id)
{
    for (int i = 0; i < m_segments.size(); ++i) {
        if (m_segments[i].id != id) continue;
        if (i == m_current) return;

        const bool first = m_current < 0;
        m_current = i;
        if (first || !isVisible()) {
            m_knob = i;
            update();
        } else {
            m_slide.stop();
            m_slide.setStartValue(m_knob);
            m_slide.setEndValue(qreal(i));
            m_slide.start();
        }
        return;
    }
}

QSize ModeSwitch::sizeHint() const
{
    return QSize(2 * SwitchPadding + qMax(1, int(m_segments.size())) * SegmentWidth, SwitchHeight);
}

QRectF ModeSwitch::segmentRect(int index) const
{
    return QRectF(SwitchPadding + index * SegmentWidth, SwitchPadding, SegmentWidth, height() - 2 * SwitchPadding);
}

int ModeSwitch::segmentAt(const QPoint &pos) const
{
    for (int i = 0; i < m_segments.size(); ++i) {
        if (segmentRect(i).contains(pos)) return i;
    }
    return -1;
}

void ModeSwitch::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(ArtflowTheme::line(), 1));
    painter.setBrush(ArtflowTheme::panel());
    painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);

    if (m_hovered >= 0 && m_hovered != m_current) {
        const QRectF hover = segmentRect(m_hovered).adjusted(1, 1, -1, -1);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ArtflowTheme::raised());
        painter.drawRoundedRect(hover, hover.height() / 2, hover.height() / 2);
    }

    if (m_current >= 0) {
        QRectF knob = segmentRect(0);
        knob.moveLeft(SwitchPadding + m_knob * SegmentWidth);
        QLinearGradient gradient(knob.topLeft(), knob.bottomRight());
        gradient.setColorAt(0, ArtflowTheme::accentHover());
        gradient.setColorAt(1, ArtflowTheme::accent());
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(knob, knob.height() / 2, knob.height() / 2);
    }

    for (int i = 0; i < m_segments.size(); ++i) {
        const bool selected = i == m_current;
        painter.setFont(ArtflowTheme::uiFont(9.5, selected ? QFont::DemiBold : QFont::Medium));
        painter.setPen(selected ? QColor(Qt::white) : (i == m_hovered ? ArtflowTheme::text() : ArtflowTheme::textMuted()));
        painter.drawText(segmentRect(i), Qt::AlignCenter, m_segments[i].title);
    }
}

void ModeSwitch::mousePressEvent(QMouseEvent *event)
{
    const int index = segmentAt(event->position().toPoint());
    if (index >= 0 && event->button() == Qt::LeftButton) {
        Q_EMIT segmentClicked(m_segments[index].id);
    }
}

void ModeSwitch::mouseMoveEvent(QMouseEvent *event)
{
    const int index = segmentAt(event->position().toPoint());
    if (index != m_hovered) {
        m_hovered = index;
        update();
    }
}

void ModeSwitch::leaveEvent(QEvent *event)
{
    m_hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

bool ModeSwitch::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *help = static_cast<QHelpEvent *>(event);
        const int index = segmentAt(help->pos());
        if (index >= 0) {
            QToolTip::showText(help->globalPos(), m_segments[index].toolTip, this, segmentRect(index).toRect());
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

// ---------------------------------------------------------------- PillSlider

namespace {
const int PillWidth = 34;
const int IconBox = 30;
}

PillSlider::PillSlider(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::SizeVerCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void PillSlider::setRange(int minimum, int maximum)
{
    m_min = minimum;
    m_max = qMax(minimum + 1, maximum);
    setValue(m_value);
}

void PillSlider::setValue(int value)
{
    value = qBound(m_min, value, m_max);
    if (value == m_value) return;
    m_value = value;
    update();
}

int PillSlider::value() const
{
    return m_value;
}

void PillSlider::setIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

void PillSlider::setValueText(const QString &text)
{
    m_text = text;
    update();
}

QSize PillSlider::sizeHint() const
{
    return QSize(PillWidth + 8, 220);
}

QSize PillSlider::minimumSizeHint() const
{
    return QSize(PillWidth + 8, 120);
}

QRectF PillSlider::trackRect() const
{
    return QRectF((width() - PillWidth) / 2.0, 0.5, PillWidth, height() - IconBox - 1.0);
}

void PillSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF track = trackRect();
    const qreal radius = track.width() / 2;
    QPainterPath shape;
    shape.addRoundedRect(track, radius, radius);

    painter.setPen(QPen(m_hovered || m_dragging ? ArtflowTheme::hover().lighter(130) : ArtflowTheme::line(), 1));
    painter.setBrush(ArtflowTheme::raised());
    painter.drawPath(shape);

    // Fill from the bottom up to the value; a slider that does not apply to
    // the current tool shows an empty track.
    const qreal t = qreal(m_value - m_min) / (m_max - m_min);
    if (isEnabled()) {
        const qreal fillHeight = qMax(track.width(), t * track.height());
        QRectF fill(track.left(), track.bottom() - fillHeight, track.width(), fillHeight);
        QLinearGradient gradient(fill.bottomLeft(), fill.topLeft());
        gradient.setColorAt(0, ArtflowTheme::accent());
        gradient.setColorAt(1, ArtflowTheme::accentHover());
        QPainterPath fillShape;
        fillShape.addRoundedRect(fill.adjusted(3, 3, -3, -3), radius - 3, radius - 3);
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawPath(fillShape);

        // A small grip line at the top of the fill.
        painter.setPen(QPen(QColor(255, 255, 255, 200), 2, Qt::SolidLine, Qt::RoundCap));
        const qreal gripY = fill.top() + 9;
        painter.drawLine(QPointF(track.center().x() - 6, gripY), QPointF(track.center().x() + 6, gripY));
    }

    // Value, written along the top of the track.
    painter.setFont(ArtflowTheme::uiFont(8.5, QFont::DemiBold));
    painter.setPen(!isEnabled() ? ArtflowTheme::textFaint() : t > 0.86 ? QColor(Qt::white) : ArtflowTheme::text());
    painter.drawText(QRectF(track.left(), track.top() + 6, track.width(), 18), Qt::AlignCenter, m_text);

    if (!m_icon.isNull()) {
        const QRect iconRect(int(track.center().x()) - 9, int(track.bottom()) + 8, 18, 18);
        m_icon.paint(&painter, iconRect, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
    }

    if (!isEnabled()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(15, 17, 22, 150));
        painter.drawPath(shape);
    }
}

void PillSlider::setFromY(qreal y)
{
    const QRectF track = trackRect();
    const qreal t = 1.0 - qBound(0.0, (y - track.top()) / track.height(), 1.0);
    const int value = qRound(m_min + t * (m_max - m_min));
    if (value != m_value) {
        m_value = value;
        update();
        Q_EMIT valueChanged(value);
    }
}

void PillSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_dragging = true;
    setFromY(event->position().y());
}

void PillSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) setFromY(event->position().y());
}

void PillSlider::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_dragging = false;
    update();
}

void PillSlider::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) return;
    const int step = qMax(1, (m_max - m_min) / 50);
    const int value = qBound(m_min, m_value + steps * step, m_max);
    if (value != m_value) {
        m_value = value;
        update();
        Q_EMIT valueChanged(value);
    }
    event->accept();
}

void PillSlider::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void PillSlider::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

// ------------------------------------------------------------ SwatchDelegate

namespace {
const int SwatchRowHeight = 34;
}

SwatchDelegate::SwatchDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

bool SwatchDelegate::isGrid(const QStyleOptionViewItem &option)
{
    const QListView *view = qobject_cast<const QListView *>(option.widget);
    return view && view->viewMode() == QListView::IconMode;
}

QSize SwatchDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    if (isGrid(option)) return option.decorationSize;
    return QSize(option.rect.width(), SwatchRowHeight);
}

void SwatchDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const bool grid = isGrid(option);
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    QRectF swatch;
    if (grid) {
        const qreal side = qMin(option.rect.width(), option.rect.height()) - 6.0;
        swatch = QRectF(0, 0, side, side);
        swatch.moveCenter(QRectF(option.rect).center());
    } else {
        const QRectF row = QRectF(option.rect).adjusted(2, 1, -2, -1);
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? ArtflowTheme::hover() : ArtflowTheme::raised());
            painter->drawRoundedRect(row, 8, 8);
        }
        const qreal side = row.height() - 10;
        swatch = QRectF(row.left() + 6, row.top() + 5, side, side);
        const QRectF text(swatch.right() + 10, row.top(), row.right() - swatch.right() - 14, row.height());
        painter->setFont(option.font);
        painter->setPen(selected ? ArtflowTheme::text() : ArtflowTheme::textMuted());
        painter->drawText(text, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(text.width())));
    }

    // The engine's plain swatch (with a checkerboard under see-through
    // colours), clipped to a rounded tile with a faint edge, so black still
    // reads on the dark panel.
    const qreal radius = qRound(swatch.width() * 0.28);
    QPainterPath shape;
    shape.addRoundedRect(swatch, radius, radius);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    painter->setClipPath(shape);
    painter->drawPixmap(swatch, icon.pixmap(swatch.size().toSize(), QIcon::Normal), QRectF());
    painter->setClipping(false);
    painter->setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(shape);

    if (grid && (selected || hovered)) {
        painter->setPen(QPen(selected ? ArtflowTheme::text() : ArtflowTheme::textFaint(), selected ? 2 : 1.2));
        const QRectF ring = swatch.adjusted(-2.5, -2.5, 2.5, 2.5);
        painter->drawRoundedRect(ring, radius + 2.5, radius + 2.5);
    }

    painter->restore();
}

// --------------------------------------------------------------- ValueSlider

namespace {
const int LabelHeight = 18;
const int TrackHeight = 6;
const qreal KnobRadius = 8.0;
}

ValueSlider::ValueSlider(const QString &label, int minimum, int maximum, int value, QWidget *parent)
    : QWidget(parent)
    , m_label(label)
    , m_min(minimum)
    , m_max(qMax(minimum + 1, maximum))
    , m_value(qBound(minimum, value, maximum))
    , m_default(qBound(minimum, value, maximum))
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

int ValueSlider::value() const
{
    return m_value;
}

void ValueSlider::setValue(int value)
{
    value = qBound(m_min, value, m_max);
    if (value == m_value) return;
    m_value = value;
    update();
}

void ValueSlider::setSuffix(const QString &suffix)
{
    m_suffix = suffix;
    update();
}

QSize ValueSlider::sizeHint() const
{
    return QSize(220, LabelHeight + 22);
}

QSize ValueSlider::minimumSizeHint() const
{
    return QSize(120, LabelHeight + 22);
}

QRectF ValueSlider::trackRect() const
{
    return QRectF(KnobRadius, LabelHeight + 8, width() - 2 * KnobRadius, TrackHeight);
}

void ValueSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setFont(ArtflowTheme::uiFont(8.5, QFont::Medium));
    painter.setPen(isEnabled() ? ArtflowTheme::textMuted() : ArtflowTheme::textFaint());
    painter.drawText(QRectF(2, 0, width() * 0.7, LabelHeight), Qt::AlignVCenter | Qt::AlignLeft, m_label);
    painter.setFont(ArtflowTheme::uiFont(8.5, QFont::DemiBold));
    painter.setPen(isEnabled() ? ArtflowTheme::text() : ArtflowTheme::textFaint());
    const QString number = (m_value > 0 && m_min < 0 ? QStringLiteral("+") : QString()) + QString::number(m_value) + m_suffix;
    painter.drawText(QRectF(width() * 0.5, 0, width() * 0.5 - 2, LabelHeight), Qt::AlignVCenter | Qt::AlignRight, number);

    const QRectF track = trackRect();
    painter.setPen(Qt::NoPen);
    painter.setBrush(ArtflowTheme::raised());
    painter.drawRoundedRect(track.adjusted(-1, -1, 1, 1), TrackHeight / 2.0 + 1, TrackHeight / 2.0 + 1);

    auto xOf = [this, &track](int value) { return track.left() + track.width() * qreal(value - m_min) / (m_max - m_min); };
    const qreal knobX = xOf(m_value);
    // Two-sided ranges fill from zero, the others from the low end.
    const qreal fromX = m_min < 0 ? xOf(0) : track.left();
    const QRectF fill(qMin(fromX, knobX), track.top(), qAbs(knobX - fromX), track.height());
    painter.setBrush(isEnabled() ? ArtflowTheme::accent() : ArtflowTheme::line());
    painter.drawRoundedRect(fill, TrackHeight / 2.0, TrackHeight / 2.0);
    if (m_min < 0) {
        painter.setBrush(ArtflowTheme::textFaint());
        painter.drawRoundedRect(QRectF(xOf(0) - 1, track.top() - 3, 2, track.height() + 6), 1, 1);
    }

    const QPointF knob(knobX, track.center().y());
    painter.setBrush(isEnabled() ? ArtflowTheme::text() : ArtflowTheme::textFaint());
    painter.setPen(m_hovered || m_dragging ? QPen(ArtflowTheme::accent(), 2.0) : QPen(ArtflowTheme::window(), 1.5));
    painter.drawEllipse(knob, KnobRadius - 1, KnobRadius - 1);
}

void ValueSlider::change(int value)
{
    value = qBound(m_min, value, m_max);
    if (value == m_value) return;
    m_value = value;
    update();
    Q_EMIT valueChanged(value);
}

void ValueSlider::setFromX(qreal x)
{
    const QRectF track = trackRect();
    const qreal t = qBound(0.0, (x - track.left()) / track.width(), 1.0);
    change(qRound(m_min + t * (m_max - m_min)));
}

void ValueSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    m_dragging = true;
    setFromX(event->position().x());
}

void ValueSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) setFromX(event->position().x());
}

void ValueSlider::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (!m_dragging) return;
    m_dragging = false;
    update();
    Q_EMIT released();
}

void ValueSlider::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    change(m_default);
    Q_EMIT released();
}

void ValueSlider::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps == 0) return;
    change(m_value + steps * qMax(1, (m_max - m_min) / 50));
    Q_EMIT released();
    event->accept();
}

void ValueSlider::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void ValueSlider::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

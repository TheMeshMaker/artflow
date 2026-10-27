/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ShapeTool.h"

#include <QLineF>
#include <QPainter>
#include <QSettings>
#include <QtMath>

#include "colormanager.h"
#include "editor.h"
#include "pointerevent.h"
#include "stroketool.h"
#include "toolmanager.h"

#include "BrushStroke.h"
#include "StudioBrushes.h"

namespace {

/// The corners of a polygon with points every couple of pixels in between,
/// so the brush's smoothing curve keeps the corners sharp.
QVector<QPointF> densify(const QPolygonF &polygon)
{
    QVector<QPointF> points;
    for (int i = 0; i + 1 < polygon.size(); ++i) {
        const QPointF a = polygon[i];
        const QPointF b = polygon[i + 1];
        const int steps = qMax(1, qCeil(QLineF(a, b).length() / 2.0));
        for (int s = 0; s < steps; ++s) points << a + (b - a) * (qreal(s) / steps);
    }
    if (!polygon.isEmpty()) points << polygon.last();
    return points;
}

}

ShapeTool::ShapeTool(StudioBrushes *brushes, QObject *parent)
    : ArtTool(parent)
    , m_brushes(brushes)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    m_shape = Shape(qBound(0, settings.value(QStringLiteral("ArtFlowTools/shape"), int(Rectangle)).toInt(), 2));
    m_filled = settings.value(QStringLiteral("ArtFlowTools/shapeFilled"), false).toBool();
}

QCursor ShapeTool::cursor()
{
    return QCursor(Qt::CrossCursor);
}

ShapeTool::Shape ShapeTool::shape() const
{
    return m_shape;
}

void ShapeTool::setShape(Shape shape)
{
    if (shape == m_shape) return;
    m_shape = shape;
    QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowTools/shape"), int(shape));
    Q_EMIT optionsChanged();
}

bool ShapeTool::filled() const
{
    return m_filled;
}

void ShapeTool::setFilled(bool filled)
{
    if (filled == m_filled) return;
    m_filled = filled;
    QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowTools/shapeFilled"), filled);
    Q_EMIT optionsChanged();
}

void ShapeTool::pointerPressEvent(PointerEvent *event)
{
    if (event->button() != Qt::LeftButton || !canDraw()) return;
    beginEdit();
    m_start = m_end = event->canvasPos();
}

void ShapeTool::pointerMoveEvent(PointerEvent *event)
{
    if (!editing()) return;
    m_end = event->canvasPos();
    render(event->modifiers());
}

void ShapeTool::pointerReleaseEvent(PointerEvent *event)
{
    if (!editing()) return;
    m_end = event->canvasPos();
    if (QLineF(m_start, m_end).length() < 2.0) {
        cancel();
        return;
    }
    render(event->modifiers());
    commit();
}

QPainterPath ShapeTool::path(const QPointF &from, const QPointF &to, Qt::KeyboardModifiers modifiers) const
{
    QPointF a = from;
    QPointF b = to;
    QPainterPath path;

    if (m_shape == Line) {
        if (modifiers & Qt::ShiftModifier) {
            // Snap to the nearest multiple of 45 degrees.
            const QLineF line(a, b);
            const qreal angle = qRound(line.angle() / 45.0) * 45.0;
            QLineF snapped = QLineF::fromPolar(line.length(), angle);
            b = a + (snapped.p2() - snapped.p1());
        }
        if (modifiers & Qt::AltModifier) a = from - (b - from);
        path.moveTo(a);
        path.lineTo(b);
        return path;
    }

    QPointF delta = b - a;
    if (modifiers & Qt::ShiftModifier) {
        const qreal side = qMax(qAbs(delta.x()), qAbs(delta.y()));
        delta = QPointF(delta.x() < 0 ? -side : side, delta.y() < 0 ? -side : side);
    }
    b = a + delta;
    if (modifiers & Qt::AltModifier) a = from - delta;

    const QRectF box = QRectF(a, b).normalized();
    if (m_shape == Rectangle) {
        path.addRect(box);
    } else {
        path.addEllipse(box);
    }
    return path;
}

void ShapeTool::render(Qt::KeyboardModifiers modifiers)
{
    const QPainterPath shapePath = path(m_start, m_end, modifiers);
    const QColor color = mEditor->color()->frontColor();

    // The outline uses the Brush tool's style and size, drawn steadily: no
    // start taper and no speed thinning, which only make sense by hand.
    BrushPreset style = m_brushes->current(BRUSH);
    style.taper = 0.0;
    style.speedThinning = false;
    const StrokeTool *brush = qobject_cast<StrokeTool *>(mEditor->tools()->getTool(BRUSH));
    const qreal width = brush ? brush->strokeToolProperties().width() : style.size;

    BrushStroke outline;
    QRect area = shapePath.boundingRect().toAlignedRect().adjusted(-2, -2, 2, 2);
    QImage outlineImage;
    QRect outlineBounds;
    for (const QPolygonF &polygon : shapePath.toSubpathPolygons()) {
        const QVector<QPointF> points = densify(polygon);
        if (points.size() < 2) continue;
        outline.begin(style, color, width, true, true, points.first(), 1.0);
        for (int i = 1; i < points.size(); ++i) outline.moveTo(points[i], 1.0);
        outline.finish();
        outlineBounds = outline.bounds();
        outlineImage = outline.render(outlineBounds);
        area = area.united(outlineBounds);
    }

    QImage image(area.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_filled && m_shape != Line) {
        painter.translate(-area.topLeft());
        painter.fillPath(shapePath, color);
        painter.resetTransform();
    }
    if (!outlineImage.isNull()) painter.drawImage(outlineBounds.topLeft() - area.topLeft(), outlineImage);
    painter.end();

    showPreview(image, area.topLeft());
}

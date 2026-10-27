/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "LassoFillTool.h"

#include <QLineF>
#include <QPainter>
#include <QPainterPath>

#include "colormanager.h"
#include "editor.h"
#include "pointerevent.h"
#include "viewmanager.h"

LassoFillTool::LassoFillTool(QObject *parent)
    : ArtTool(parent)
{
}

QCursor LassoFillTool::cursor()
{
    return QCursor(Qt::CrossCursor);
}

void LassoFillTool::pointerPressEvent(PointerEvent *event)
{
    if (event->button() != Qt::LeftButton || !canDraw()) return;
    beginEdit();
    m_points = QPolygonF() << event->canvasPos();
}

void LassoFillTool::pointerMoveEvent(PointerEvent *event)
{
    if (!editing()) return;
    if (QLineF(m_points.last(), event->canvasPos()).length() < 1.0) return;
    m_points << event->canvasPos();
    render(false);
}

void LassoFillTool::pointerReleaseEvent(PointerEvent *event)
{
    if (!editing()) return;
    m_points << event->canvasPos();
    if (m_points.size() < 3 || m_points.boundingRect().width() + m_points.boundingRect().height() < 4.0) {
        cancel();
        return;
    }
    render(true);
    commit();
    m_points.clear();
}

void LassoFillTool::render(bool finished)
{
    const QColor color = mEditor->color()->frontColor();
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);  // a loop that crosses itself still fills
    path.addPolygon(m_points);
    path.closeSubpath();

    const QRect area = path.boundingRect().toAlignedRect().adjusted(-3, -3, 3, 3);
    QImage image(area.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(-area.topLeft());
    painter.fillPath(path, color);
    if (!finished) {
        // While drawing, the path itself shows too, as a thin line of the
        // same colour (about one screen pixel wide at any zoom).
        QColor line = color;
        line.setAlpha(255);
        const qreal width = 1.5 / qMax(0.05, qreal(mEditor->view()->scaling()));
        painter.setPen(QPen(line, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(m_points);
    }
    painter.end();

    showPreview(image, area.topLeft());
}

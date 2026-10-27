/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_SHAPETOOL_H
#define ARTFLOW_SHAPETOOL_H

#include <QPainterPath>
#include <QPointF>

#include "ArtTool.h"

class StudioBrushes;

/**
 * Lines, rectangles and ellipses: drag from one corner to the other.
 * Shift makes a square, a circle or a line at a multiple of 45 degrees;
 * Alt draws from the centre. The outline is drawn with the Brush tool's
 * style and size, so a pencil-textured box or an ink circle is one drag;
 * closed shapes can be filled with the same colour.
 */
class ShapeTool : public ArtTool
{
    Q_OBJECT
public:
    enum Shape { Line, Rectangle, Ellipse };

    explicit ShapeTool(StudioBrushes *brushes, QObject *parent = nullptr);

    ToolType type() const override { return SHAPES; }
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent *event) override;
    void pointerMoveEvent(PointerEvent *event) override;
    void pointerReleaseEvent(PointerEvent *event) override;

    Shape shape() const;
    void setShape(Shape shape);
    bool filled() const;
    void setFilled(bool filled);

    /// The shape between two canvas points, with the modifier keys' rules.
    QPainterPath path(const QPointF &from, const QPointF &to, Qt::KeyboardModifiers modifiers) const;

Q_SIGNALS:
    void optionsChanged();

private:
    void render(Qt::KeyboardModifiers modifiers);

    StudioBrushes *m_brushes;
    Shape m_shape = Rectangle;
    bool m_filled = false;
    QPointF m_start;
    QPointF m_end;
};

#endif // ARTFLOW_SHAPETOOL_H

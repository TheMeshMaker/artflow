/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef BITMAPBRUSHENGINE_H
#define BITMAPBRUSHENGINE_H

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QVector>

#include "pencildef.h"

/**
 * A pluggable renderer for the bitmap strokes of the brush, pencil and
 * eraser tools. A program built on the core installs one with setEngine();
 * when none is installed (as in Pencil2D itself) the tools draw their own
 * dabs exactly as before.
 *
 * The tools hand the engine the stroke as it comes in (canvas position and
 * pressure) and lay the patches it returns into the stroke buffer, which is
 * committed to the layer when the stroke ends, as usual.
 */
class BitmapBrushEngine
{
public:
    /// A rendered piece of the stroke, in canvas pixels.
    struct Patch {
        QImage image;
        QPoint topLeft;
        QPainter::CompositionMode mode = QPainter::CompositionMode_Source;
    };

    struct StrokeStart {
        ToolType tool = INVALID_TOOL;
        QColor color;
        qreal width = 1.0;          ///< the tool's width setting, in canvas pixels
        bool pressureEnabled = true;
        bool tablet = false;        ///< false for a mouse, which has no real pressure
    };

    virtual ~BitmapBrushEngine() = default;

    /// Whether this engine draws the bitmap strokes of \p tool.
    virtual bool handles(ToolType tool) const = 0;

    virtual QVector<Patch> beginStroke(const StrokeStart& start, const QPointF& point, qreal pressure) = 0;
    virtual QVector<Patch> strokeTo(const QPointF& point, qreal pressure) = 0;
    virtual QVector<Patch> endStroke() = 0;

    static BitmapBrushEngine* engine() { return sEngine; }
    static void setEngine(BitmapBrushEngine* engine) { sEngine = engine; }

private:
    static inline BitmapBrushEngine* sEngine = nullptr;
};

#endif // BITMAPBRUSHENGINE_H

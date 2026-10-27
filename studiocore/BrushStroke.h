/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_BRUSHSTROKE_H
#define ARTFLOW_BRUSHSTROKE_H

#include <QColor>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRandomGenerator>
#include <QRect>
#include <QVector>

#include "bitmapbrushengine.h"

#include "BrushPreset.h"

/**
 * Renders one stroke with a brush preset.
 *
 * The pointer positions are joined by a centripetal Catmull-Rom curve, so
 * fast strokes stay round instead of turning into straight pieces, and
 * pressure is blended along the curve. Dabs are laid at an even spacing
 * into a coverage mask kept for the whole stroke: a stroke never gets
 * darker where its own dabs overlap (unless the brush builds up), and the
 * paper grain is applied once, at the end, anchored to the canvas.
 * The finished pixels of the parts that changed are handed out as patches
 * that replace the same area of the stroke buffer.
 */
class BrushStroke
{
public:
    BrushStroke();

    void begin(const BrushPreset &preset, const QColor &color, qreal width, bool pressureEnabled, bool tablet,
               const QPointF &point, qreal pressure);
    void moveTo(const QPointF &point, qreal pressure);
    /// Draws the last piece, which waits for the point after it until the stroke ends.
    void finish();
    /// Frees the stroke's memory.
    void clear();
    /// Every dab is also laid mirrored across the vertical line x = \p axisX.
    void setMirror(bool enabled, qreal axisX = 0.0);
    /// Paints only inside \p rect (canvas pixels); an empty rect paints anywhere.
    void setClip(const QRect &rect);

    /// The finished pixels of what changed since the last call.
    QVector<BitmapBrushEngine::Patch> takePatches();

    /// Everything painted so far, in canvas pixels.
    QRect bounds() const;
    QImage render(const QRect &rect) const;

private:
    struct Sample {
        QPointF pos;
        qreal pressure;
    };

    qreal effectivePressure(qreal raw, qreal distance);
    qreal diameterAt(qreal pressure, qreal travelled) const;
    void segment(const Sample &p0, const Sample &p1, const Sample &p2, const Sample &p3);
    void walk(const QPointF &from, const QPointF &to, qreal pressureFrom, qreal pressureTo);
    void dab(const QPointF &center, qreal pressure, qreal travelled);
    /// A stamp, and its mirror image when symmetry is on.
    void place(const QPointF &center, qreal diameter, qreal alpha, qreal hardness, qreal roundness);
    void stamp(const QPointF &center, qreal diameter, qreal alpha, qreal hardness, qreal roundness);
    float *tileFor(int tileX, int tileY);
    const float *tileAt(int tileX, int tileY) const;
    float finalAlpha(float coverage, int x, int y) const;

    BrushPreset m_preset;
    QColor m_color;
    qreal m_width = 10.0;
    bool m_pressureEnabled = true;
    bool m_tablet = false;
    qreal m_angleCos = 1.0;
    qreal m_angleSin = 0.0;
    QRect m_clip;
    bool m_mirror = false;
    qreal m_mirrorAxis = 0.0;

    QHash<quint64, QVector<float>> m_tiles;
    QRect m_dirty;
    QRect m_bounds;

    QVector<Sample> m_samples;
    qreal m_carry = 0.0;
    qreal m_travelled = 0.0;
    qreal m_speedPressure = 1.0;
    QRandomGenerator m_random;
};

#endif // ARTFLOW_BRUSHSTROKE_H

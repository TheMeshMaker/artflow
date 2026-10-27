/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "BrushStroke.h"

#include <QLineF>
#include <QtMath>

namespace {

const int TileSize = 64;
const int GrainSize = 256;

quint64 tileKey(int tileX, int tileY)
{
    return (quint64(quint32(tileY)) << 32) | quint32(tileX);
}

int floorDiv(int value, int divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

qreal smooth(qreal t)
{
    t = qBound(0.0, t, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

/// Paper tooth: tileable value noise, a few octaves on lattices that divide
/// the texture size, plus fine per-texel noise. 1 is a ridge that takes
/// paint easily, 0 a valley.
const QVector<float> &grainTexture()
{
    static const QVector<float> texture = [] {
        QVector<float> out(GrainSize * GrainSize, 0.0f);
        QRandomGenerator random(20261027);

        const QList<QPair<int, float>> octaves = {{8, 0.10f}, {16, 0.16f}, {32, 0.24f}, {64, 0.28f}, {128, 0.22f}};
        for (const auto &octave : octaves) {
            const int cells = octave.first;
            QVector<float> lattice(cells * cells);
            for (float &value : lattice) value = float(random.generateDouble());
            const float step = float(GrainSize) / cells;
            for (int y = 0; y < GrainSize; ++y) {
                const float fy = y / step;
                const int y0 = int(fy) % cells;
                const int y1 = (y0 + 1) % cells;
                const float ty = float(smooth(fy - int(fy)));
                for (int x = 0; x < GrainSize; ++x) {
                    const float fx = x / step;
                    const int x0 = int(fx) % cells;
                    const int x1 = (x0 + 1) % cells;
                    const float tx = float(smooth(fx - int(fx)));
                    const float top = lattice[y0 * cells + x0] * (1 - tx) + lattice[y0 * cells + x1] * tx;
                    const float bottom = lattice[y1 * cells + x0] * (1 - tx) + lattice[y1 * cells + x1] * tx;
                    out[y * GrainSize + x] += octave.second * (top * (1 - ty) + bottom * ty);
                }
            }
        }
        for (float &value : out) value += 0.12f * float(random.generateDouble());

        float low = 1e9f;
        float high = -1e9f;
        for (float value : out) {
            low = qMin(low, value);
            high = qMax(high, value);
        }
        for (float &value : out) {
            value = float(smooth((value - low) / (high - low) * 1.6 - 0.3));
        }
        return out;
    }();
    return texture;
}

float grainAt(qreal x, qreal y, qreal scale)
{
    const QVector<float> &texture = grainTexture();
    const qreal u = x / scale;
    const qreal v = y / scale;
    const int u0 = qFloor(u);
    const int v0 = qFloor(v);
    const float fu = float(u - u0);
    const float fv = float(v - v0);
    auto at = [&texture](int tx, int ty) {
        tx &= GrainSize - 1;
        ty &= GrainSize - 1;
        return texture[ty * GrainSize + tx];
    };
    const float top = at(u0, v0) * (1 - fu) + at(u0 + 1, v0) * fu;
    const float bottom = at(u0, v0 + 1) * (1 - fu) + at(u0 + 1, v0 + 1) * fu;
    return top * (1 - fv) + bottom * fv;
}

}

BrushStroke::BrushStroke()
    : m_random(1)
{
}

void BrushStroke::begin(const BrushPreset &preset, const QColor &color, qreal width, bool pressureEnabled, bool tablet,
                        const QPointF &point, qreal pressure)
{
    clear();
    m_preset = preset;
    m_color = color;
    m_width = qMax(0.5, width);
    m_pressureEnabled = pressureEnabled;
    m_tablet = tablet;
    m_angleCos = qCos(qDegreesToRadians(preset.angle));
    m_angleSin = qSin(qDegreesToRadians(preset.angle));
    // The same stroke drawn twice gets the same jitter and spray pattern.
    m_random.seed(quint32(qRound(point.x() * 7.0 + point.y() * 131.0)));

    m_speedPressure = 1.0;
    const Sample first {point, effectivePressure(pressure, 0.0)};
    m_samples << first;
    // A tap leaves a dot.
    dab(point, first.pressure, 0.0);
}

void BrushStroke::moveTo(const QPointF &point, qreal pressure)
{
    if (m_samples.isEmpty()) return;

    const qreal distance = QLineF(m_samples.last().pos, point).length();
    if (distance < 0.5) {
        // Too close to bend the curve; keep the newest pressure.
        if (m_tablet) m_samples.last().pressure = effectivePressure(pressure, 0.0);
        return;
    }

    m_samples << Sample {point, effectivePressure(pressure, distance)};
    const int n = m_samples.size();
    if (n >= 3) {
        // The curve from n-3 to n-2 is settled once n-1 is known.
        segment(m_samples[qMax(0, n - 4)], m_samples[n - 3], m_samples[n - 2], m_samples[n - 1]);
    }
    if (m_samples.size() > 4) m_samples.removeFirst();
}

void BrushStroke::finish()
{
    const int n = m_samples.size();
    if (n >= 2) {
        segment(m_samples[qMax(0, n - 3)], m_samples[n - 2], m_samples[n - 1], m_samples[n - 1]);
    }
    m_samples.clear();
}

void BrushStroke::clear()
{
    m_tiles.clear();
    m_dirty = QRect();
    m_bounds = QRect();
    m_samples.clear();
    m_carry = 0.0;
    m_travelled = 0.0;
}

QRect BrushStroke::bounds() const
{
    return m_bounds;
}

QVector<BitmapBrushEngine::Patch> BrushStroke::takePatches()
{
    if (m_dirty.isEmpty()) return {};
    const QRect rect = m_dirty;
    m_dirty = QRect();

    BitmapBrushEngine::Patch patch;
    patch.image = render(rect);
    patch.topLeft = rect.topLeft();
    // The buffer holds only this stroke, so the newer pixels simply replace it.
    patch.mode = QPainter::CompositionMode_Source;
    return {patch};
}

QImage BrushStroke::render(const QRect &rect) const
{
    QImage image(rect.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (rect.isEmpty()) return image;

    const float red = float(m_color.redF());
    const float green = float(m_color.greenF());
    const float blue = float(m_color.blueF());
    const float opacity = float(m_color.alphaF());

    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y - rect.top()));
        const int tileY = floorDiv(y, TileSize);
        const int rowInTile = y - tileY * TileSize;
        int x = rect.left();
        while (x <= rect.right()) {
            const int tileX = floorDiv(x, TileSize);
            const int spanEnd = qMin(rect.right(), tileX * TileSize + TileSize - 1);
            const float *tile = tileAt(tileX, tileY);
            if (tile) {
                const float *row = tile + rowInTile * TileSize;
                for (int px = x; px <= spanEnd; ++px) {
                    const float coverage = row[px - tileX * TileSize];
                    if (coverage <= 0.0f) continue;
                    const float alpha = finalAlpha(coverage, px, y) * opacity;
                    if (alpha <= 0.0f) continue;
                    line[px - rect.left()] = qRgba(qRound(red * alpha * 255), qRound(green * alpha * 255),
                                                   qRound(blue * alpha * 255), qRound(alpha * 255));
                }
            }
            x = spanEnd + 1;
        }
    }
    return image;
}

qreal BrushStroke::effectivePressure(qreal raw, qreal distance)
{
    if (!m_pressureEnabled) return 1.0;
    if (m_tablet) return qBound(0.0, raw, 1.0);
    if (!m_preset.speedThinning) return 1.0;

    // A mouse has no pressure: let speed stand in for it, gently.
    const qreal target = qBound(0.3, 1.0 - distance / (m_width * 1.5 + 30.0), 1.0);
    m_speedPressure += (target - m_speedPressure) * 0.3;
    return m_speedPressure;
}

qreal BrushStroke::diameterAt(qreal pressure, qreal travelled) const
{
    qreal diameter = m_width * (1.0 - m_preset.pressureSize * (1.0 - pressure));
    if (m_preset.taper > 0.0) {
        diameter *= 0.18 + 0.82 * smooth(travelled / (m_preset.taper * m_width));
    }
    return qMax(0.3, diameter);
}

void BrushStroke::segment(const Sample &p0, const Sample &p1, const Sample &p2, const Sample &p3)
{
    // Centripetal Catmull-Rom (Barry and Goldman's form): no loops or
    // overshoot at sharp turns, unlike the uniform kind.
    auto knot = [](qreal t, const QPointF &a, const QPointF &b) {
        return t + qMax(1e-3, qSqrt(QLineF(a, b).length()));
    };
    const qreal t0 = 0.0;
    const qreal t1 = knot(t0, p0.pos, p1.pos);
    const qreal t2 = knot(t1, p1.pos, p2.pos);
    const qreal t3 = knot(t2, p2.pos, p3.pos);

    auto curveAt = [&](qreal t) {
        const QPointF a1 = (t1 - t) / (t1 - t0) * p0.pos + (t - t0) / (t1 - t0) * p1.pos;
        const QPointF a2 = (t2 - t) / (t2 - t1) * p1.pos + (t - t1) / (t2 - t1) * p2.pos;
        const QPointF a3 = (t3 - t) / (t3 - t2) * p2.pos + (t - t2) / (t3 - t2) * p3.pos;
        const QPointF b1 = (t2 - t) / (t2 - t0) * a1 + (t - t0) / (t2 - t0) * a2;
        const QPointF b2 = (t3 - t) / (t3 - t1) * a2 + (t - t1) / (t3 - t1) * a3;
        return (t2 - t) / (t2 - t1) * b1 + (t - t1) / (t2 - t1) * b2;
    };

    const qreal chord = QLineF(p1.pos, p2.pos).length();
    const int steps = qMax(1, qCeil(chord / 1.5));
    QPointF previous = p1.pos;
    qreal previousPressure = p1.pressure;
    for (int i = 1; i <= steps; ++i) {
        const qreal u = qreal(i) / steps;
        const QPointF point = i == steps ? p2.pos : curveAt(t1 + (t2 - t1) * u);
        const qreal pressure = p1.pressure + (p2.pressure - p1.pressure) * u;
        walk(previous, point, previousPressure, pressure);
        previous = point;
        previousPressure = pressure;
    }
}

void BrushStroke::walk(const QPointF &from, const QPointF &to, qreal pressureFrom, qreal pressureTo)
{
    const qreal length = QLineF(from, to).length();
    if (length <= 0.0) return;

    qreal along = 0.0;
    for (;;) {
        const qreal pressure = pressureFrom + (pressureTo - pressureFrom) * (along / length);
        const qreal spacing = qMax(0.3, m_preset.spacing * diameterAt(pressure, m_travelled + along));
        const qreal needed = spacing - m_carry;
        if (along + needed > length) {
            m_carry += length - along;
            break;
        }
        along += needed;
        m_carry = 0.0;
        const qreal t = along / length;
        dab(from + (to - from) * t, pressureFrom + (pressureTo - pressureFrom) * t, m_travelled + along);
    }
    m_travelled += length;
}

void BrushStroke::dab(const QPointF &center, qreal pressure, qreal travelled)
{
    qreal diameter = diameterAt(pressure, travelled);
    if (m_preset.sizeJitter > 0.0) {
        diameter *= 1.0 + m_preset.sizeJitter * (m_random.generateDouble() * 2.0 - 1.0);
    }
    const qreal alpha = m_preset.flow * (1.0 - m_preset.pressureOpacity * (1.0 - pressure));

    QPointF position = center;
    if (m_preset.scatter > 0.0) {
        const qreal angle = m_random.generateDouble() * 2.0 * M_PI;
        const qreal reach = m_random.generateDouble() * m_preset.scatter * diameter * 0.5;
        position += QPointF(qCos(angle), qSin(angle)) * reach;
    }

    if (m_preset.particles > 0) {
        // Spray: small soft dots, denser towards the middle.
        const qreal radius = diameter * 0.5;
        const qreal dot = qBound(0.8, diameter * 0.035, 2.6);
        for (int i = 0; i < m_preset.particles; ++i) {
            const qreal angle = m_random.generateDouble() * 2.0 * M_PI;
            const qreal reach = radius * m_random.generateDouble();
            const qreal size = dot * (0.6 + 0.8 * m_random.generateDouble());
            stamp(position + QPointF(qCos(angle), qSin(angle)) * reach, size, alpha, 0.55, 1.0);
        }
        return;
    }
    stamp(position, diameter, alpha, m_preset.hardness, m_preset.roundness);
}

void BrushStroke::stamp(const QPointF &center, qreal diameter, qreal alpha, qreal hardness, qreal roundness)
{
    qreal radius = diameter * 0.5;
    if (radius < 0.5) {
        // Thinner than a pixel: keep the line faint rather than jagged.
        alpha *= (radius / 0.5) * (radius / 0.5);
        radius = 0.5;
    }
    if (alpha <= 0.0) return;

    const bool round = roundness >= 0.999;
    const qreal along = radius;
    const qreal across = qMax(0.5, radius * roundness);
    // How far the edge fades: about a pixel for a hard tip (smooth, not
    // jagged), the whole radius for a fully soft one.
    const qreal soft = qMax(1.0, (1.0 - hardness) * across);
    const bool shaped = soft > 1.0;
    const bool buildUp = m_preset.buildUp;
    const float amount = float(alpha);

    const int left = qFloor(center.x() - radius - 1.0);
    const int right = qCeil(center.x() + radius + 1.0);
    const int top = qFloor(center.y() - radius - 1.0);
    const int bottom = qCeil(center.y() + radius + 1.0);

    for (int tileY = floorDiv(top, TileSize); tileY <= floorDiv(bottom, TileSize); ++tileY) {
        for (int tileX = floorDiv(left, TileSize); tileX <= floorDiv(right, TileSize); ++tileX) {
            float *tile = tileFor(tileX, tileY);
            const int y0 = qMax(top, tileY * TileSize);
            const int y1 = qMin(bottom, tileY * TileSize + TileSize - 1);
            const int x0 = qMax(left, tileX * TileSize);
            const int x1 = qMin(right, tileX * TileSize + TileSize - 1);
            for (int y = y0; y <= y1; ++y) {
                float *row = tile + (y - tileY * TileSize) * TileSize;
                const qreal dy = y + 0.5 - center.y();
                for (int x = x0; x <= x1; ++x) {
                    const qreal dx = x + 0.5 - center.x();
                    qreal distance; // signed distance to the edge, negative inside
                    if (round) {
                        distance = qSqrt(dx * dx + dy * dy) - radius;
                    } else {
                        const qreal u = dx * m_angleCos + dy * m_angleSin;
                        const qreal v = -dx * m_angleSin + dy * m_angleCos;
                        const qreal k = qSqrt(u * u / (along * along) + v * v / (across * across));
                        if (k < 1e-6) {
                            distance = -across;
                        } else {
                            const qreal gradient = qSqrt(u * u / qPow(along, 4) + v * v / qPow(across, 4)) / k;
                            distance = (k - 1.0) / gradient;
                        }
                    }
                    qreal coverage = (0.5 - distance) / soft;
                    if (coverage <= 0.0) continue;
                    coverage = shaped ? smooth(coverage) : qMin(1.0, coverage);

                    const float value = amount * float(coverage);
                    float &mask = row[x - tileX * TileSize];
                    mask = buildUp ? mask + value * (1.0f - mask) : qMax(mask, value);
                }
            }
        }
    }

    const QRect box(QPoint(left, top), QPoint(right, bottom));
    m_dirty = m_dirty.united(box);
    m_bounds = m_bounds.united(box);
}

float *BrushStroke::tileFor(int tileX, int tileY)
{
    QVector<float> &tile = m_tiles[tileKey(tileX, tileY)];
    if (tile.isEmpty()) tile.fill(0.0f, TileSize * TileSize);
    return tile.data();
}

const float *BrushStroke::tileAt(int tileX, int tileY) const
{
    auto it = m_tiles.constFind(tileKey(tileX, tileY));
    return it == m_tiles.constEnd() ? nullptr : it->constData();
}

float BrushStroke::finalAlpha(float coverage, int x, int y) const
{
    if (m_preset.grain <= 0.0) return qMin(1.0f, coverage);

    // Light strokes only catch the ridges of the paper; pressing harder
    // fills the valleys too, but some tooth always shows.
    const float valley = float(m_preset.grain) * (1.0f - grainAt(x + 0.5, y + 0.5, m_preset.grainScale));
    const float reached = qBound(0.0f, (coverage - 0.6f * valley) / (1.0f - 0.6f * valley), 1.0f);
    return reached * (1.0f - 0.4f * valley);
}

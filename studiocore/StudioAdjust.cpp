/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioAdjust.h"

#include <QtMath>
#include <QVector>

#include "bitmapimage.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "selectionmanager.h"

#include "StudioCore.h"

namespace {

/// The widest blur needs this much room around the drawing.
const int BlurRoom = 3 * 40;

/// One box blur pass over premultiplied pixels, along rows or columns.
void boxPass(QVector<float> &channels, int width, int height, int radius, bool horizontal)
{
    if (radius < 1) return;
    const int length = horizontal ? width : height;
    const int lines = horizontal ? height : width;
    QVector<float> line(length * 4);
    const float scale = 1.0f / (2 * radius + 1);

    for (int l = 0; l < lines; ++l) {
        auto index = [&](int i) { return horizontal ? (l * width + i) * 4 : (i * width + l) * 4; };
        for (int i = 0; i < length; ++i) {
            for (int c = 0; c < 4; ++c) line[i * 4 + c] = channels[index(i) + c];
        }
        // A running sum; outside the image counts as transparent.
        float sum[4] = {0, 0, 0, 0};
        for (int i = 0; i <= radius && i < length; ++i) {
            for (int c = 0; c < 4; ++c) sum[c] += line[i * 4 + c];
        }
        for (int i = 0; i < length; ++i) {
            for (int c = 0; c < 4; ++c) channels[index(i) + c] = sum[c] * scale;
            const int add = i + radius + 1;
            const int remove = i - radius;
            for (int c = 0; c < 4; ++c) {
                if (add < length) sum[c] += line[add * 4 + c];
                if (remove >= 0) sum[c] -= line[remove * 4 + c];
            }
        }
    }
}

float hueToChannel(float p, float q, float t)
{
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
}

}

bool Adjustments::isIdentity() const
{
    return hue == 0 && saturation == 0 && brightness == 0 && contrast == 0 && blur == 0 && !invert && !grayscale;
}

StudioAdjust::StudioAdjust(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
}

bool StudioAdjust::active() const
{
    return m_active;
}

bool StudioAdjust::begin()
{
    if (m_active) cancel();

    Editor *editor = m_core->editor();
    Layer *layer = editor->layers()->currentLayer();
    if (!layer || layer->type() != Layer::BITMAP) return false;
    BitmapImage *bitmap = static_cast<LayerBitmap *>(layer)->getLastBitmapImageAtFrame(editor->currentFrame());
    if (!bitmap) return false;
    bitmap->loadFile();
    if (!bitmap->image() || bitmap->bounds().isEmpty()) return false;

    m_saveState = editor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    m_bitmap = bitmap;
    m_layerIndex = editor->currentLayerIndex();
    m_frame = editor->currentFrame();

    // Room around the drawing for a blur to spread into; transparent
    // pixels, cropped away again later. Every preview starts again from a
    // whole copy, since the canvas may crop the drawing in between.
    const QRect padded = bitmap->bounds().adjusted(-BlurRoom, -BlurRoom, BlurRoom, BlurRoom);
    BitmapImage room(padded, QColor(0, 0, 0, 0));
    bitmap->paste(&room);
    m_original = *bitmap;

    // Only the selected part changes, when something is selected.
    m_area = QRect();
    if (editor->select()->somethingSelected()) {
        m_area = editor->select()->mySelectionRect().toAlignedRect().translated(-padded.topLeft());
    }
    m_active = true;
    return true;
}

void StudioAdjust::preview(const Adjustments &adjustments)
{
    if (!m_active) return;
    BitmapImage next(m_original);
    if (!adjustments.isIdentity()) {
        QImage image = adjusted(*next.image(), adjustments, m_area);
        next.setImage(&image);
    }
    *m_bitmap = next;
    refresh();
}

void StudioAdjust::apply()
{
    if (!m_active) return;
    m_active = false;
    m_core->editor()->undoRedo()->record(m_saveState, tr("Adjust"));
}

void StudioAdjust::cancel()
{
    if (!m_active) return;
    m_active = false;
    *m_bitmap = m_original;
    refresh();
}

void StudioAdjust::refresh()
{
    m_core->editor()->setModified(m_layerIndex, m_frame);
    m_core->canvas()->onFrameModified(m_frame);
}

QImage StudioAdjust::adjusted(const QImage &source, const Adjustments &a, const QRect &area)
{
    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int width = image.width();
    const int height = image.height();
    const QRect inside = area.isEmpty() ? image.rect() : area.intersected(image.rect());
    if (inside.isEmpty()) return image;

    QVector<float> px(width * height * 4);
    for (int y = 0; y < height; ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            float *p = &px[(y * width + x) * 4];
            p[0] = qRed(row[x]) / 255.0f;
            p[1] = qGreen(row[x]) / 255.0f;
            p[2] = qBlue(row[x]) / 255.0f;
            p[3] = qAlpha(row[x]) / 255.0f;
        }
    }

    if (a.blur > 0) {
        // Three box passes come close to a gaussian blur.
        const int radius = qMax(1, qRound(a.blur / 1.7));
        for (int pass = 0; pass < 3; ++pass) {
            boxPass(px, width, height, radius, true);
            boxPass(px, width, height, radius, false);
        }
    }

    const float hueShift = a.hue / 360.0f;
    const float saturation = 1.0f + a.saturation / 100.0f;
    const float brightness = a.brightness / 100.0f;
    const float contrast = a.contrast >= 0 ? 1.0f / qMax(0.05f, 1.0f - a.contrast / 105.0f) : 1.0f + a.contrast / 100.0f;
    const bool colour = a.hue != 0 || a.saturation != 0 || a.brightness != 0 || a.contrast != 0 || a.invert || a.grayscale;

    for (int y = inside.top(); y <= inside.bottom(); ++y) {
        QRgb *row = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = inside.left(); x <= inside.right(); ++x) {
            const float *p = &px[(y * width + x) * 4];
            const float alpha = qBound(0.0f, p[3], 1.0f);
            if (alpha <= 0.0f) {
                row[x] = 0;
                continue;
            }
            float r = p[0] / alpha;
            float g = p[1] / alpha;
            float b = p[2] / alpha;

            if (colour) {
                if (a.grayscale) r = g = b = 0.299f * r + 0.587f * g + 0.114f * b;
                if (a.invert) {
                    r = 1 - r;
                    g = 1 - g;
                    b = 1 - b;
                }
                if (a.hue != 0 || a.saturation != 0) {
                    // Through hue, saturation and lightness.
                    const float high = qMax(r, qMax(g, b));
                    const float low = qMin(r, qMin(g, b));
                    const float light = (high + low) / 2;
                    float hue = 0;
                    float sat = 0;
                    if (high > low) {
                        const float d = high - low;
                        sat = light > 0.5f ? d / (2 - high - low) : d / (high + low);
                        if (high == r) hue = (g - b) / d + (g < b ? 6 : 0);
                        else if (high == g) hue = (b - r) / d + 2;
                        else hue = (r - g) / d + 4;
                        hue /= 6;
                    }
                    hue += hueShift;
                    hue -= qFloor(hue);
                    sat = qBound(0.0f, sat * saturation, 1.0f);
                    if (sat <= 0) {
                        r = g = b = light;
                    } else {
                        const float q = light < 0.5f ? light * (1 + sat) : light + sat - light * sat;
                        const float pp = 2 * light - q;
                        r = hueToChannel(pp, q, hue + 1.0f / 3);
                        g = hueToChannel(pp, q, hue);
                        b = hueToChannel(pp, q, hue - 1.0f / 3);
                    }
                }
                if (a.brightness != 0) {
                    auto lift = [brightness](float v) { return brightness > 0 ? v + (1 - v) * brightness : v * (1 + brightness); };
                    r = lift(r);
                    g = lift(g);
                    b = lift(b);
                }
                if (a.contrast != 0) {
                    r = (r - 0.5f) * contrast + 0.5f;
                    g = (g - 0.5f) * contrast + 0.5f;
                    b = (b - 0.5f) * contrast + 0.5f;
                }
            }
            r = qBound(0.0f, r, 1.0f) * alpha;
            g = qBound(0.0f, g, 1.0f) * alpha;
            b = qBound(0.0f, b, 1.0f) * alpha;
            row[x] = qRgba(qRound(r * 255), qRound(g * 255), qRound(b * 255), qRound(alpha * 255));
        }
    }
    return image;
}

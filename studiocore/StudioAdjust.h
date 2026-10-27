/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_STUDIOADJUST_H
#define ARTFLOW_STUDIOADJUST_H

#include <QImage>
#include <QObject>
#include <QRect>

#include "bitmapimage.h"
#include "undoredomanager.h"

class StudioCore;

/// Colour and blur changes for a drawing; all zero changes nothing.
struct Adjustments
{
    int hue = 0;         ///< degrees, -180 to 180
    int saturation = 0;  ///< -100 (grey) to 100
    int brightness = 0;  ///< -100 to 100
    int contrast = 0;    ///< -100 to 100
    int blur = 0;        ///< radius in canvas pixels, 0 to 40
    bool invert = false;
    bool grayscale = false;

    bool isIdentity() const;
};

/**
 * Adjusts the drawing on the current frame of the current layer, or only
 * the selected part of it: a live preview while the values change, then
 * one undo step (or back to how it was).
 */
class StudioAdjust : public QObject
{
    Q_OBJECT
public:
    explicit StudioAdjust(StudioCore *core);

    /// Starts on the current drawing. False when there is nothing to adjust.
    bool begin();
    void preview(const Adjustments &adjustments);
    void apply();
    void cancel();
    bool active() const;

    /// \p image with \p adjustments, changed only inside \p area (image
    /// pixels; empty means all of it).
    static QImage adjusted(const QImage &image, const Adjustments &adjustments, const QRect &area = QRect());

private:
    void refresh();

    StudioCore *m_core;
    BitmapImage *m_bitmap = nullptr;
    int m_layerIndex = -1;
    int m_frame = 0;
    BitmapImage m_original;  ///< the drawing as it was, with room for a blur
    QRect m_area;
    SAVESTATE_ID m_saveState = 0;
    bool m_active = false;
};

#endif // ARTFLOW_STUDIOADJUST_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_BRUSHPRESET_H
#define ARTFLOW_BRUSHPRESET_H

#include <QString>

#include "pencildef.h"

/**
 * One brush style: how a stroke turns into paint. The Brush and Pencil
 * tools each have their own set (see StudioBrushes::library()); the size
 * is only the starting size, each brush then remembers the last one used.
 */
struct BrushPreset
{
    QString id;
    QString name;
    QString family;              ///< "sketch", "ink", "paint" or "effects"
    ToolType tool = BRUSH;       ///< the tool whose library holds it

    qreal size = 10.0;           ///< starting diameter, in canvas pixels
    qreal hardness = 1.0;        ///< 1: crisp edge; 0: soft from the centre out
    qreal spacing = 0.06;        ///< distance between dabs, as a part of the diameter
    qreal flow = 1.0;            ///< paint laid by one dab
    bool buildUp = false;        ///< dabs add up (airbrush); otherwise a stroke never gets darker than one dab
    qreal pressureSize = 0.5;    ///< how much pressure changes the size, 0 to 1
    qreal pressureOpacity = 0.0; ///< how much pressure changes the opacity, 0 to 1
    qreal grain = 0.0;           ///< strength of the paper texture, 0 to 1
    qreal grainScale = 1.0;      ///< size of the texture, in canvas pixels per texel
    qreal roundness = 1.0;       ///< width of the tip across its angle; below 1 it is a flat nib
    qreal angle = 0.0;           ///< angle of a flat nib, in degrees
    qreal sizeJitter = 0.0;      ///< random change of size from dab to dab
    qreal scatter = 0.0;         ///< random offset of dabs, as a part of the diameter
    int particles = 0;           ///< spray: this many small dots per dab instead of a solid tip
    qreal taper = 0.0;           ///< length of the thin start of a stroke, in diameters
    bool speedThinning = false;  ///< with a mouse (no pressure), faster strokes come out thinner
};

#endif // ARTFLOW_BRUSHPRESET_H

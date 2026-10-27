/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_BRUSHSLIDERSPANEL_H
#define ARTFLOW_BRUSHSLIDERSPANEL_H

#include <QWidget>

class PillSlider;
class StudioCore;

/**
 * Two tall sliders beside the canvas: brush size and opacity. They follow
 * the engine too, so [ and ] or the tool options move them.
 */
class BrushSlidersPanel : public QWidget
{
    Q_OBJECT
public:
    BrushSlidersPanel(StudioCore *core, QWidget *parent = nullptr);

private Q_SLOTS:
    void slotBrushChanged();

private:
    StudioCore *m_core;
    PillSlider *m_size;
    PillSlider *m_opacity;
};

#endif // ARTFLOW_BRUSHSLIDERSPANEL_H

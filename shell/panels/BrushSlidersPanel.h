/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_BRUSHSLIDERSPANEL_H
#define ARTFLOW_BRUSHSLIDERSPANEL_H

#include <QWidget>

class QLabel;
class QSlider;
class StudioCore;

/**
 * Two tall sliders beside the canvas: brush size and opacity. They follow
 * the engine too, so [ and ] or picking a preset move them.
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
    QSlider *m_size;
    QSlider *m_opacity;
    QLabel *m_sizeLabel;
    QLabel *m_opacityLabel;
};

#endif // ARTFLOW_BRUSHSLIDERSPANEL_H

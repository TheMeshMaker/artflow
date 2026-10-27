/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_BRUSHSLIDERSPANEL_H
#define ARTFLOW_BRUSHSLIDERSPANEL_H

#include <QWidget>

class PillSlider;
class QToolButton;
class StudioCore;

/**
 * The rail beside the canvas: the current brush style (it opens the brush
 * picker) and two tall sliders, brush size and opacity. The sliders follow
 * the engine too, so [ and ] or the tool options move them.
 */
class BrushSlidersPanel : public QWidget
{
    Q_OBJECT
public:
    BrushSlidersPanel(StudioCore *core, QWidget *parent = nullptr);

Q_SIGNALS:
    /// The brush style button was clicked; the picker opens beside \p anchor.
    void brushStylesRequested(QWidget *anchor);

private Q_SLOTS:
    void slotBrushChanged();
    void slotStyleChanged();

private:
    StudioCore *m_core;
    QToolButton *m_style;
    PillSlider *m_size;
    PillSlider *m_opacity;
};

#endif // ARTFLOW_BRUSHSLIDERSPANEL_H

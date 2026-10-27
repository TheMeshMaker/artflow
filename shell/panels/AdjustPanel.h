/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_ADJUSTPANEL_H
#define ARTFLOW_ADJUSTPANEL_H

#include <QDialog>
#include <QTimer>

#include "StudioAdjust.h"

class QToolButton;
class StudioCore;
class ValueSlider;

/**
 * Hue, saturation, brightness, contrast, blur, invert and black & white
 * for the drawing on the current frame, previewed live on the canvas.
 * Apply keeps the change as one undo step; Cancel puts it back.
 */
class AdjustPanel : public QDialog
{
    Q_OBJECT
public:
    AdjustPanel(StudioCore *core, bool selectionOnly, QWidget *parent = nullptr);

    Adjustments adjustments() const;
    /// Sets every control without previewing (for tests).
    void setAdjustments(const Adjustments &adjustments);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void reset();

    StudioCore *m_core;
    ValueSlider *m_hue;
    ValueSlider *m_saturation;
    ValueSlider *m_brightness;
    ValueSlider *m_contrast;
    ValueSlider *m_blur;
    QToolButton *m_invert;
    QToolButton *m_grayscale;
    QTimer m_preview;
};

#endif // ARTFLOW_ADJUSTPANEL_H

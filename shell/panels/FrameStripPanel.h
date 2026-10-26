/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_FRAMESTRIPPANEL_H
#define ARTFLOW_FRAMESTRIPPANEL_H

#include <QWidget>

class QLabel;
class QScrollArea;
class QToolButton;
class StudioCore;
class StudioTimelineModel;

/**
 * The strip of cells for the active layer, painted from the timeline
 * model's active row: a filled cell starts a drawing, a faded cell holds
 * the previous drawing, an outlined cell is empty.
 */
class FrameCells : public QWidget
{
    Q_OBJECT
public:
    FrameCells(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    /// Rectangle of the cell showing engine frame \p frame (1-based).
    QRect cellRect(int frame) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    StudioCore *m_core;
    StudioTimelineModel *m_model;
};

/// Flip mode: frame-by-frame editing of one layer.
class FrameStripPanel : public QWidget
{
    Q_OBJECT
public:
    FrameStripPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

private Q_SLOTS:
    void slotModelChanged();
    void slotCurrentFrameChanged(int frame);

private:
    StudioCore *m_core;
    StudioTimelineModel *m_model;
    FrameCells *m_cells;
    QScrollArea *m_scroll;
    QLabel *m_layerLabel;
    QLabel *m_frameLabel;
    QToolButton *m_onion;
};

#endif // ARTFLOW_FRAMESTRIPPANEL_H

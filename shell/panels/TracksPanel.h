/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_TRACKSPANEL_H
#define ARTFLOW_TRACKSPANEL_H

#include <QWidget>

class QScrollArea;
class StudioCore;
class StudioTimelineModel;

/**
 * Layers as tracks, frames as columns, keyframes as diamonds, and a
 * playhead. The camera layer is a track like the others, so camera moves
 * (pan, zoom, rotate, with easing in the engine) are keyed here.
 */
class TrackGrid : public QWidget
{
    Q_OBJECT
public:
    TrackGrid(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    int playheadX() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void seekTo(const QPoint &pos, bool selectRow);

    StudioCore *m_core;
    StudioTimelineModel *m_model;
};

/// Motion mode: tracks and keyframes.
class TracksPanel : public QWidget
{
    Q_OBJECT
public:
    TracksPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

private:
    StudioCore *m_core;
    TrackGrid *m_grid;
    QScrollArea *m_scroll;
};

#endif // ARTFLOW_TRACKSPANEL_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_FRAMESTRIPPANEL_H
#define ARTFLOW_FRAMESTRIPPANEL_H

#include <QHash>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QScrollArea;
class QToolButton;
class StudioCore;
class StudioTimelineModel;

/**
 * The strip of frame tiles for the active layer, painted from the
 * timeline model's active row: a drawing shows its thumbnail, a held
 * frame continues the one before it, an empty frame is a faint outline.
 */
class FrameCells : public QWidget
{
    Q_OBJECT
public:
    FrameCells(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    /// Rectangle of the tile showing engine frame \p frame (1-based).
    QRect cellRect(int frame) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int columnCount() const;
    int frameAt(const QPoint &pos) const;
    QPixmap thumbnail(int frame);

    StudioCore *m_core;
    StudioTimelineModel *m_model;
    QHash<int, QPixmap> m_thumbnails;
    int m_hoveredFrame {-1};
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

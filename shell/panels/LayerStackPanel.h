/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_LAYERSTACKPANEL_H
#define ARTFLOW_LAYERSTACKPANEL_H

#include <QHash>
#include <QListView>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

class Layer;
class QLineEdit;
class StudioCore;
class StudioTimelineModel;
class ValueSlider;

/**
 * The list of layers: a thumbnail of each one's drawing on the current
 * frame, its name and visibility. Click to choose, click the eye to hide,
 * double-click the name to rename, drag up or down to reorder.
 */
class LayerList : public QListView
{
    Q_OBJECT
public:
    LayerList(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

    /// The thumbnail of \p layer's drawing on the current frame, kept until the next change.
    QPixmap thumbnail(Layer *layer, const QSize &size);
    /// Where a dragged layer would land, as a row gap (-1 while not dragging).
    int dropGap() const { return m_dropGap; }
    void rename(int row);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    int gapAt(int y) const;

    StudioCore *m_core;
    StudioTimelineModel *m_model;
    QHash<Layer *, QPixmap> m_thumbnails;
    QPoint m_pressPos;
    int m_pressRow {-1};
    int m_dropGap {-1};
    bool m_dragging {false};
    QPointer<QLineEdit> m_editor;
};

/// The layers panel: the list, buttons to add, copy and delete, and the opacity of the chosen layer.
class LayerStackPanel : public QWidget
{
    Q_OBJECT
public:
    LayerStackPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

Q_SIGNALS:
    /// The panel's maximum height changed with the number of layers.
    void heightLimitChanged();

private Q_SLOTS:
    void slotModelReset();

private:
    StudioCore *m_core;
    StudioTimelineModel *m_model;
    LayerList *m_list;
    ValueSlider *m_opacity;
};

#endif // ARTFLOW_LAYERSTACKPANEL_H

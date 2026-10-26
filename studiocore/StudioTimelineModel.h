/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOTIMELINEMODEL_H
#define STUDIOTIMELINEMODEL_H

#include <QAbstractTableModel>
#include <QTimer>
#include <QVector>

class Layer;
class StudioCore;

/**
 * One model, three views: rows are layers (top of the stack first),
 * columns are frames (column c is engine frame c + 1). The Flip frame
 * strip shows the active row, the Motion track view shows the whole grid
 * (camera layer included), and the Studio layer stack only uses column 0.
 */
class StudioTimelineModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Roles {
        LayerTypeRole = Qt::UserRole + 1,  ///< int, Layer::LAYER_TYPE
        IsActiveLayerRole,                 ///< bool
        IsVisibleRole,                     ///< bool
        HasKeyframeRole,                   ///< bool, a drawing (or camera key) starts here
        IsHoldRole,                        ///< bool, frame shows an earlier keyframe
        IsCurrentFrameRole,                ///< bool
    };

    explicit StudioTimelineModel(StudioCore *core, QObject *parent = nullptr);
    ~StudioTimelineModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    Layer *layerAt(int row) const;
    int activeRow() const;
    /// The engine's current frame (1-based).
    int currentFrame() const;
    int frameCount() const;

    static int frameOfColumn(int column) { return column + 1; }
    static int columnOfFrame(int frame) { return frame - 1; }

public Q_SLOTS:
    void refresh();

private Q_SLOTS:
    void scheduleRefresh();
    void slotCurrentFrameChanged(int frame);

private:
    struct Row {
        Layer *layer {nullptr};
        int type {0};
        QString name;
        bool visible {true};
        QVector<int> keys;  ///< sorted keyframe frames (1-based)
    };

    StudioCore *m_core;
    QVector<Row> m_rows;
    int m_frameCount {1};
    int m_sceneLength {1};  ///< last frame with a keyframe on any layer
    int m_currentFrame {1};
    Layer *m_activeLayer {nullptr};
    QTimer m_compressor;
};

#endif // STUDIOTIMELINEMODEL_H

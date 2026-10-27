/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioTimelineModel.h"

#include <algorithm>

#include "layer.h"

#include "StudioCore.h"
#include "StudioHistory.h"
#include "StudioLayers.h"
#include "StudioTimeline.h"

namespace {
/// Empty cells kept after the last frame so there is always room to draw on.
const int SpareFrames = 24;
}

StudioTimelineModel::StudioTimelineModel(StudioCore *core, QObject *parent)
    : QAbstractTableModel(parent)
    , m_core(core)
{
    m_compressor.setSingleShot(true);
    m_compressor.setInterval(20);
    connect(&m_compressor, &QTimer::timeout, this, &StudioTimelineModel::refresh);

    connect(core, &StudioCore::documentChanged, this, &StudioTimelineModel::scheduleRefresh);
    connect(core->layers(), &StudioLayers::layersChanged, this, &StudioTimelineModel::scheduleRefresh);
    connect(core->layers(), &StudioLayers::activeLayerChanged, this, &StudioTimelineModel::scheduleRefresh);
    connect(core->timeline(), &StudioTimeline::keyframesChanged, this, &StudioTimelineModel::scheduleRefresh);
    // Every undoable edit changes the undo state, which also covers undo/redo itself.
    connect(core->history(), &StudioHistory::changed, this, &StudioTimelineModel::scheduleRefresh);
    connect(core->timeline(), &StudioTimeline::currentFrameChanged,
            this, &StudioTimelineModel::slotCurrentFrameChanged);

    refresh();
}

StudioTimelineModel::~StudioTimelineModel()
{
}

int StudioTimelineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int StudioTimelineModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_frameCount;
}

QVariant StudioTimelineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size()) return QVariant();

    const Row &row = m_rows[index.row()];
    const int frame = frameOfColumn(index.column());

    switch (role) {
    case Qt::DisplayRole:
        return index.column() == 0 ? row.name : QVariant();
    case LayerTypeRole:
        return row.type;
    case IsActiveLayerRole:
        return row.layer == m_activeLayer;
    case IsVisibleRole:
        return row.visible;
    case HasKeyframeRole:
        return std::binary_search(row.keys.begin(), row.keys.end(), frame);
    case IsHoldRole:
        // Held frames run up to the end of the animation, not forever.
        return !row.keys.isEmpty() && frame > row.keys.first() && frame <= m_sceneLength
            && !std::binary_search(row.keys.begin(), row.keys.end(), frame);
    case IsCurrentFrameRole:
        return frame == m_currentFrame;
    case OpacityRole:
        return row.opacity;
    default:
        return QVariant();
    }
}

QVariant StudioTimelineModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Horizontal) return frameOfColumn(section);
    return (section >= 0 && section < m_rows.size()) ? m_rows[section].name : QVariant();
}

Layer *StudioTimelineModel::layerAt(int row) const
{
    return (row >= 0 && row < m_rows.size()) ? m_rows[row].layer : nullptr;
}

int StudioTimelineModel::activeRow() const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].layer == m_activeLayer) return i;
    }
    return -1;
}

int StudioTimelineModel::currentFrame() const
{
    return m_currentFrame;
}

int StudioTimelineModel::frameCount() const
{
    return m_frameCount;
}

void StudioTimelineModel::refresh()
{
    beginResetModel();

    m_rows.clear();
    for (Layer *layer : m_core->layers()->layers()) {
        Row row;
        row.layer = layer;
        row.type = layer->type();
        row.name = layer->name();
        row.visible = layer->visible();
        row.opacity = layer->opacity();
        row.keys = m_core->timeline()->keyframeTimes(layer).toVector();
        m_rows.append(row);
    }

    m_activeLayer = m_core->layers()->activeLayer();
    m_currentFrame = m_core->timeline()->currentFrame();
    m_sceneLength = m_core->timeline()->lastUsedFrame();
    m_frameCount = qMax(m_sceneLength, m_currentFrame) + SpareFrames;

    endResetModel();
}

void StudioTimelineModel::scheduleRefresh()
{
    m_compressor.start();
}

void StudioTimelineModel::slotCurrentFrameChanged(int frame)
{
    if (frame == m_currentFrame) return;

    const int previous = m_currentFrame;
    m_currentFrame = frame;

    if (columnOfFrame(frame) >= m_frameCount - 1) {
        scheduleRefresh();
        return;
    }

    if (!m_rows.isEmpty()) {
        const QVector<int> roles = {IsCurrentFrameRole};
        const int previousColumn = columnOfFrame(previous);
        if (previousColumn >= 0 && previousColumn < m_frameCount) {
            Q_EMIT dataChanged(index(0, previousColumn), index(m_rows.size() - 1, previousColumn), roles);
        }
        Q_EMIT dataChanged(index(0, columnOfFrame(frame)), index(m_rows.size() - 1, columnOfFrame(frame)), roles);
    }
}

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOLAYERS_H
#define STUDIOLAYERS_H

#include <QObject>
#include <QTimer>
#include <QVector>

class Layer;
class StudioCore;

/**
 * The layer stack of the document, top to bottom, plus the onion skin
 * switch (a document-wide setting in the engine, not a per-layer one).
 */
class StudioLayers : public QObject
{
    Q_OBJECT
public:
    explicit StudioLayers(StudioCore *core);
    ~StudioLayers() override;

    /// All layers, top of the stack first.
    QVector<Layer *> layers() const;

    Layer *activeLayer() const;
    void setActiveLayer(Layer *layer);

    /// Adds a drawing layer above the active one, without a name prompt.
    Layer *addDrawingLayer();
    /// Asks for confirmation (layer deletion cannot be undone in the engine).
    void removeActiveLayer();

    bool isVisible(Layer *layer) const;
    void setVisible(Layer *layer, bool visible);

    bool onionSkins() const;
    void setOnionSkins(bool enabled);

Q_SIGNALS:
    /// The set, order, names or visibility of the layers changed.
    void layersChanged();
    void activeLayerChanged();
    void onionSkinsChanged(bool enabled);

private:
    StudioCore *m_core;
    QTimer m_compressor;
};

#endif // STUDIOLAYERS_H

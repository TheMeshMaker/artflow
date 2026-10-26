/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioLayers.h"

#include "actioncommands.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "preferencemanager.h"

#include "StudioCore.h"

StudioLayers::StudioLayers(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
    m_compressor.setSingleShot(true);
    m_compressor.setInterval(20);
    connect(&m_compressor, &QTimer::timeout, this, &StudioLayers::layersChanged);

    Editor *editor = core->editor();
    auto compress = [this]() { m_compressor.start(); };
    connect(editor->layers(), &LayerManager::layerCountChanged, this, compress);
    connect(editor->layers(), &LayerManager::layerDeleted, this, compress);
    connect(editor, &Editor::updateTimeLine, this, compress);
    connect(editor, &Editor::objectLoaded, this, compress);
    connect(editor->layers(), &LayerManager::currentLayerChanged, this, &StudioLayers::activeLayerChanged);
}

StudioLayers::~StudioLayers()
{
}

QVector<Layer *> StudioLayers::layers() const
{
    LayerManager *manager = m_core->editor()->layers();
    QVector<Layer *> result;
    for (int i = manager->count() - 1; i >= 0; --i) {
        result << manager->getLayer(i);
    }
    return result;
}

Layer *StudioLayers::activeLayer() const
{
    return m_core->editor()->layers()->currentLayer();
}

void StudioLayers::setActiveLayer(Layer *layer)
{
    if (layer) m_core->editor()->layers()->setCurrentLayer(layer);
}

Layer *StudioLayers::addDrawingLayer()
{
    LayerManager *manager = m_core->editor()->layers();
    return manager->createBitmapLayer(manager->nameSuggestLayer(tr("Drawing")));
}

void StudioLayers::removeActiveLayer()
{
    m_core->commands()->deleteCurrentLayer();
}

bool StudioLayers::isVisible(Layer *layer) const
{
    return layer && layer->visible();
}

void StudioLayers::setVisible(Layer *layer, bool visible)
{
    if (!layer || layer->visible() == visible) return;

    LayerManager *manager = m_core->editor()->layers();
    for (int i = 0; i < manager->count(); ++i) {
        if (manager->getLayer(i) == layer) {
            m_core->editor()->switchVisibilityOfLayer(i);
            return;
        }
    }
}

bool StudioLayers::onionSkins() const
{
    PreferenceManager *preference = m_core->editor()->preference();
    return preference->isOn(SETTING::PREV_ONION) || preference->isOn(SETTING::NEXT_ONION);
}

void StudioLayers::setOnionSkins(bool enabled)
{
    PreferenceManager *preference = m_core->editor()->preference();
    preference->set(SETTING::PREV_ONION, enabled);
    preference->set(SETTING::NEXT_ONION, enabled);
    m_core->editor()->updateFrame();
    Q_EMIT onionSkinsChanged(enabled);
}

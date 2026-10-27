/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioLayers.h"

#include <QFileInfo>
#include <QImage>

#include "actioncommands.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "bitmapimage.h"
#include "layermanager.h"
#include "object.h"
#include "selectionmanager.h"
#include "undoredomanager.h"
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

bool StudioLayers::contains(Layer *layer) const
{
    if (!layer) return false;
    LayerManager *manager = m_core->editor()->layers();
    for (int i = 0; i < manager->count(); ++i) {
        if (manager->getLayer(i) == layer) return true;
    }
    return false;
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

Layer *StudioLayers::duplicateActiveLayer()
{
    Layer *source = activeLayer();
    if (!source || source->type() == Layer::CAMERA) return nullptr;
    const qreal opacity = source->opacity();
    m_core->commands()->duplicateLayer();
    Layer *copy = activeLayer();
    if (copy && copy != source) copy->setOpacity(opacity);
    m_core->editor()->object()->modification();
    m_compressor.start();
    return copy;
}

void StudioLayers::rename(Layer *layer, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (!layer || trimmed.isEmpty() || trimmed == layer->name()) return;
    m_core->editor()->layers()->renameLayer(layer, trimmed);
    m_core->editor()->object()->modification();
    m_compressor.start();
}

void StudioLayers::moveTo(Layer *layer, int row)
{
    LayerManager *manager = m_core->editor()->layers();
    const int count = manager->count();
    int from = -1;
    for (int i = 0; i < count; ++i) {
        if (manager->getLayer(i) == layer) from = i;
    }
    // Rows count from the top, the engine's indexes from the bottom. The
    // engine keeps a camera layer at the bottom where it is.
    const bool cameraBelow = count > 0 && manager->getLayer(0)->type() == Layer::CAMERA;
    const int lowest = cameraBelow ? 1 : 0;
    if (from < 0 || (cameraBelow && from == 0)) return;
    const int to = qBound(lowest, count - 1 - row, count - 1);
    if (from == to) return;

    // Step by step, the way the engine's timeline does it, so the moving
    // layer stays the active one.
    manager->setCurrentLayer(from);
    if (to < from) {
        for (int i = from - 1; i >= to; --i) m_core->editor()->swapLayers(i, i + 1);
    } else {
        for (int i = from + 1; i <= to; ++i) m_core->editor()->swapLayers(i, i - 1);
    }
    m_core->editor()->object()->modification();
    m_compressor.start();
}

Layer *StudioLayers::importImage(const QString &path)
{
    QImage picture(path);
    if (picture.isNull()) return nullptr;
    picture = picture.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // Fitted inside the page, centred on it; never enlarged.
    const QRect page = m_core->pageRect();
    if (!page.isEmpty() && (picture.width() > page.width() || picture.height() > page.height())) {
        picture = picture.scaled(page.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    const QPoint centre = page.isEmpty() ? QPoint(0, 0) : page.center();
    const QPoint topLeft = centre - QPoint(picture.width() / 2, picture.height() / 2);

    Editor *editor = m_core->editor();
    LayerManager *manager = editor->layers();
    Layer *layer = manager->createBitmapLayer(manager->nameSuggestLayer(QFileInfo(path).completeBaseName()));
    if (!layer) return nullptr;
    const int frame = editor->currentFrame();
    if (!layer->keyExists(frame)) layer->addNewKeyFrameAt(frame);

    const SAVESTATE_ID state = editor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    BitmapImage *bitmap = static_cast<LayerBitmap *>(layer)->getBitmapImageAtFrame(frame);
    if (!bitmap) return layer;
    BitmapImage placed(topLeft, picture);
    bitmap->paste(&placed);
    editor->setModified(manager->currentLayerIndex(), frame);
    editor->undoRedo()->record(state, tr("Import image"));
    editor->select()->setSelection(QRectF(topLeft, picture.size()), true);
    editor->updateFrame();
    m_compressor.start();
    return layer;
}

qreal StudioLayers::opacity(Layer *layer) const
{
    return layer ? layer->opacity() : 1.0;
}

void StudioLayers::setOpacity(Layer *layer, qreal opacity)
{
    if (!layer || qFuzzyCompare(layer->opacity(), qBound(0.0, opacity, 1.0))) return;
    layer->setOpacity(opacity);
    m_core->editor()->object()->modification();
    m_core->editor()->updateFrame();
    m_compressor.start();
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

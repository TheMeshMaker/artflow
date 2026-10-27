/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioTimeline.h"

#include <algorithm>

#include <QPainter>

#include "actioncommands.h"
#include "bitmapimage.h"
#include "editor.h"
#include "keyframe.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "layermanager.h"
#include "playbackmanager.h"

#include "StudioCore.h"

StudioTimeline::StudioTimeline(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
    Editor *editor = core->editor();
    connect(editor, &Editor::scrubbed, this, &StudioTimeline::currentFrameChanged);
    connect(editor, &Editor::frameModified, this, &StudioTimeline::keyframesChanged);
    connect(editor, &Editor::framesModified, this, &StudioTimeline::keyframesChanged);
    connect(editor, &Editor::updateTimeLine, this, &StudioTimeline::keyframesChanged);
    connect(editor, &Editor::objectLoaded, this, &StudioTimeline::keyframesChanged);
    connect(editor->layers(), &LayerManager::animationLengthChanged, this, &StudioTimeline::keyframesChanged);
    connect(editor->playback(), &PlaybackManager::fpsChanged, this, &StudioTimeline::frameRateChanged);
}

StudioTimeline::~StudioTimeline()
{
}

int StudioTimeline::currentFrame() const
{
    return m_core->editor()->currentFrame();
}

void StudioTimeline::setCurrentFrame(int frame)
{
    m_core->editor()->scrubTo(qMax(1, frame));
}

int StudioTimeline::frameRate() const
{
    return m_core->editor()->playback()->fps();
}

void StudioTimeline::setFrameRate(int fps)
{
    m_core->editor()->playback()->setFps(qBound(1, fps, 90));
}

int StudioTimeline::lastUsedFrame() const
{
    return qMax(1, m_core->editor()->layers()->lastKeyFrameIndex());
}

bool StudioTimeline::hasKeyframe(Layer *layer, int frame) const
{
    return layer && layer->keyExists(frame);
}

QImage StudioTimeline::thumbnail(Layer *layer, int frame, const QSize &size) const
{
    if (!layer || layer->type() != Layer::BITMAP || size.isEmpty()) return QImage();

    BitmapImage *bitmap = static_cast<LayerBitmap *>(layer)->getBitmapImageAtFrame(frame);
    if (!bitmap) return QImage();

    auto *camera = static_cast<LayerCamera *>(m_core->editor()->layers()->getLastCameraLayer());
    const QRectF paper = camera ? QRectF(camera->getViewRect()) : QRectF(bitmap->bounds());
    if (paper.isEmpty()) return QImage();

    QImage thumb(size, QImage::Format_ARGB32_Premultiplied);
    thumb.fill(Qt::white);

    QPainter painter(&thumb);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal scale = qMax(size.width() / paper.width(), size.height() / paper.height());
    painter.translate(size.width() / 2.0, size.height() / 2.0);
    painter.scale(scale, scale);
    painter.translate(-paper.center());

    const QImage *image = bitmap->image();
    if (image && !image->isNull()) {
        painter.drawImage(bitmap->bounds().topLeft(), *image);
    }
    return thumb;
}

QList<int> StudioTimeline::keyframeTimes(Layer *layer) const
{
    QList<int> times;
    if (!layer) return times;
    layer->foreachKeyFrame([&times](KeyFrame *key) { times << key->pos(); });
    std::sort(times.begin(), times.end());
    return times;
}

void StudioTimeline::focus(Layer *layer, int frame)
{
    if (layer && layer != m_core->editor()->layers()->currentLayer()) {
        m_core->editor()->layers()->setCurrentLayer(layer);
    }
    if (frame != currentFrame()) setCurrentFrame(frame);
}

void StudioTimeline::addFrameAfter(Layer *layer, int frame)
{
    if (!layer) return;
    focus(layer, frame + 1);
    m_core->commands()->addNewKey();  // takes the first free frame, records undo
}

void StudioTimeline::addKeyAt(Layer *layer, int frame)
{
    if (!layer) return;
    focus(layer, frame);
    m_core->commands()->addNewKey();
}

namespace {
/// A held frame stands for the keyframe it shows.
int keyFor(Layer *layer, int frame)
{
    if (layer->keyExists(frame)) return frame;
    KeyFrame *key = layer->getKeyFrameWhichCovers(frame);
    return key ? key->pos() : -1;
}
}

void StudioTimeline::duplicateFrame(Layer *layer, int frame)
{
    if (!layer) return;
    const int key = keyFor(layer, frame);
    if (key < 0) return;
    focus(layer, key);
    m_core->commands()->duplicateKey();
}

void StudioTimeline::removeFrame(Layer *layer, int frame)
{
    if (!layer) return;
    const int key = keyFor(layer, frame);
    if (key < 0) return;
    focus(layer, key);
    m_core->commands()->removeKey();  // records undo
}

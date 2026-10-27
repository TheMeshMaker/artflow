/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ArtTool.h"

#include <QPainter>

#include "editor.h"
#include "layer.h"
#include "layermanager.h"
#include "scribblearea.h"

ArtTool::ArtTool(QObject *parent)
    : BaseTool(parent)
{
}

bool ArtTool::leavingThisTool()
{
    if (m_editing) cancel();
    return BaseTool::leavingThisTool();
}

bool ArtTool::canDraw() const
{
    const Layer *layer = mEditor->layers()->currentLayer();
    return layer && layer->type() == Layer::BITMAP && layer->visible();
}

void ArtTool::beginEdit()
{
    mScribbleArea->handleDrawingOnEmptyFrame();
    m_saveState = mEditor->undoRedo()->createState(UndoRedoRecordType::KEYFRAME_MODIFY);
    m_previewBounds = QRect();
    m_editing = true;
}

void ArtTool::showPreview(const QImage &image, const QPoint &topLeft)
{
    if (!m_editing) return;

    // One patch over the new and the old preview, so whatever the old one
    // covered and the new one does not is cleared too.
    const QRect bounds(topLeft, image.size());
    const QRect area = bounds.united(m_previewBounds);
    if (area.isEmpty()) return;

    QImage patch(area.size(), QImage::Format_ARGB32_Premultiplied);
    patch.fill(Qt::transparent);
    if (!image.isNull()) {
        QPainter painter(&patch);
        painter.drawImage(bounds.topLeft() - area.topLeft(), image);
    }
    mScribbleArea->drawStrokePatch(patch, area.topLeft(), QPainter::CompositionMode_Source);
    m_previewBounds = bounds;
}

void ArtTool::commit()
{
    if (!m_editing) return;
    m_editing = false;

    mEditor->backup(typeName());
    mEditor->setModified(mEditor->currentLayerIndex(), mEditor->currentFrame());
    mScribbleArea->endStroke();
    mEditor->undoRedo()->record(m_saveState, typeName());
}

void ArtTool::cancel()
{
    if (!m_editing) return;
    m_editing = false;

    mScribbleArea->clearDrawingBuffer();
    mScribbleArea->update();
}

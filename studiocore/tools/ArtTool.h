/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_ARTTOOL_H
#define ARTFLOW_ARTTOOL_H

#include <QImage>
#include <QRect>

#include "basetool.h"
#include "toolproperties.h"
#include "undoredomanager.h"

/**
 * The common part of ArtFlow's own tools (shapes, lasso fill, text), added
 * to the engine with ToolManager::addTool(). They draw a live preview into
 * the engine's stroke buffer and lay it into the drawing as one undo step,
 * the way the engine's brushes do. They work on drawing (bitmap) layers.
 */
class ArtTool : public BaseTool
{
    Q_OBJECT
public:
    explicit ArtTool(QObject *parent = nullptr);

    ToolProperties &toolProperties() override { return m_properties; }
    void loadSettings() override {}
    bool isActive() const override { return m_editing; }
    bool leavingThisTool() override;

protected:
    /// The current layer is a visible drawing layer.
    bool canDraw() const;
    /// Starts an edit: a drawing to paint on and an undo snapshot.
    void beginEdit();
    /// Shows \p image (canvas pixels at \p topLeft) instead of the last preview.
    void showPreview(const QImage &image, const QPoint &topLeft);
    /// Lays the preview into the drawing as one undo step.
    void commit();
    void cancel();
    bool editing() const { return m_editing; }

private:
    ToolProperties m_properties;
    QRect m_previewBounds;
    SAVESTATE_ID m_saveState = 0;
    bool m_editing = false;
};

#endif // ARTFLOW_ARTTOOL_H

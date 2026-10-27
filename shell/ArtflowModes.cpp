/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QCoreApplication>

#include "ModeManager.h"
#include "panels/FrameStripPanel.h"
#include "panels/TracksPanel.h"

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("ArtflowModes", text);
}
}

void registerDefaultModes(ModeManager *manager)
{
    ModeDescriptor flip;
    flip.id = QStringLiteral("flip");
    flip.title = tr("Flip");
    flip.tagline = tr("Frame-by-frame animation: draw a frame, add the next one, flip through them");
    flip.tools = {ToolIntent::Brush, ToolIntent::Pencil, ToolIntent::Eraser, ToolIntent::Fill,
                  ToolIntent::LassoFill, ToolIntent::Picker, ToolIntent::Select};
    flip.showLayerStack = true;
    flip.defaultTool = ToolIntent::Brush;
    flip.engineDocks = {QStringLiteral("ColorWheel"), QStringLiteral("ColorPalette")};
    flip.createTimelinePanel = [](StudioCore *core, StudioTimelineModel *model, QWidget *parent) -> QWidget * {
        return new FrameStripPanel(core, model, parent);
    };
    manager->registerMode(flip);

    ModeDescriptor motion;
    motion.id = QStringLiteral("motion");
    motion.title = tr("Motion");
    motion.tagline = tr("Tracks and keyframes: move the camera and the drawings over time");
    motion.tools = {ToolIntent::Camera, ToolIntent::Move, ToolIntent::Select, ToolIntent::Brush, ToolIntent::Eraser};
    motion.defaultTool = ToolIntent::Camera;
    motion.engineDocks = {QStringLiteral("ColorWheel"), QStringLiteral("ColorPalette"), QStringLiteral("ToolOption")};
    motion.showLayerStack = true;
    motion.createTimelinePanel = [](StudioCore *core, StudioTimelineModel *model, QWidget *parent) -> QWidget * {
        return new TracksPanel(core, model, parent);
    };
    manager->registerMode(motion);

    ModeDescriptor studio;
    studio.id = QStringLiteral("studio");
    studio.title = tr("Studio");
    studio.tagline = tr("Drawing and painting: layers, brushes and colours");
    studio.tools = {ToolIntent::Brush, ToolIntent::Pencil, ToolIntent::Eraser, ToolIntent::Smudge,
                    ToolIntent::Fill, ToolIntent::LassoFill, ToolIntent::Picker, ToolIntent::Select,
                    ToolIntent::Move, ToolIntent::Shapes, ToolIntent::Text};
    studio.defaultTool = ToolIntent::Brush;
    studio.engineDocks = {QStringLiteral("ColorWheel"), QStringLiteral("ColorPalette"), QStringLiteral("ToolOption")};
    studio.showLayerStack = true;
    manager->registerMode(studio);
}

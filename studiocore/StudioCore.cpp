/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioCore.h"

#include <QEvent>
#include <QSettings>
#include <QTimer>

#include "actioncommands.h"
#include "editor.h"
#include "layercamera.h"
#include "mainwindow2.h"
#include "object.h"
#include "scribblearea.h"
#include "toolmanager.h"
#include "viewmanager.h"

#include "tools/LassoFillTool.h"
#include "tools/ShapeTool.h"
#include "tools/TextTool.h"

#include "StudioAdjust.h"
#include "StudioBrushes.h"
#include "StudioHistory.h"
#include "StudioLayers.h"
#include "StudioPenButtons.h"
#include "StudioPlayback.h"
#include "StudioTimeline.h"
#include "StudioTools.h"

StudioCore::StudioCore(MainWindow2 *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_editor(window->mEditor)
{
    // ActionCommands is the engine's own command layer (frames, layers,
    // import/export); the main window has a private one, the facade has its own.
    m_commands = new ActionCommands(window);
    m_commands->setCore(m_editor);

    m_history = new StudioHistory(this);
    m_tools = new StudioTools(this);
    m_layers = new StudioLayers(this);
    m_timeline = new StudioTimeline(this);
    m_playback = new StudioPlayback(this);
    m_penButtons = new StudioPenButtons(this);
    m_brushes = new StudioBrushes(this);
    m_adjust = new StudioAdjust(this);

    // ArtFlow's own tools join the engine's.
    m_shapeTool = new ShapeTool(m_brushes);
    m_textTool = new TextTool();
    m_editor->tools()->addTool(SHAPES, m_shapeTool);
    m_editor->tools()->addTool(LASSOFILL, new LassoFillTool());
    m_editor->tools()->addTool(TEXT, m_textTool);

    connect(m_editor, &Editor::objectLoaded, this, &StudioCore::documentChanged);
    connect(m_editor, &Editor::objectLoaded, this, &StudioCore::fitView);
    if (canvas()) canvas()->installEventFilter(this);
}

bool StudioCore::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == canvas() && event->type() == QEvent::Resize) {
        ViewManager *view = m_editor->view();
        const bool untouched = !m_fitted
            || (qFuzzyCompare(view->scaling(), m_fitScale) && view->translation() == m_fitTranslation);
        if (untouched) QTimer::singleShot(0, this, &StudioCore::fitView);
    }
    return QObject::eventFilter(watched, event);
}

StudioCore::~StudioCore()
{
}

MainWindow2 *StudioCore::window() const
{
    return m_window;
}

Editor *StudioCore::editor() const
{
    return m_editor;
}

ScribbleArea *StudioCore::canvas() const
{
    return m_editor->getScribbleArea();
}

QRect StudioCore::pageRect() const
{
    return pageRect(m_editor);
}

QRect StudioCore::pageRect(Editor *editor)
{
    Object *object = editor->object();
    if (!object) return QRect();
    const std::vector<LayerCamera *> cameras = object->getLayersByType<LayerCamera>();
    if (cameras.empty()) return QRect();
    const QRectF frame = cameras.front()->getViewRect();
    const QTransform view = cameras.front()->getViewAtFrame(editor->currentFrame());
    return view.inverted().mapRect(frame).toAlignedRect();
}

bool StudioCore::keepOnPage()
{
    return QSettings(PENCIL2D, PENCIL2D).value(QStringLiteral("ArtFlowLook/keepOnPage"), true).toBool();
}

void StudioCore::setKeepOnPage(bool keep)
{
    QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowLook/keepOnPage"), keep);
}

StudioBrushes *StudioCore::brushes() const
{
    return m_brushes;
}

StudioAdjust *StudioCore::adjust() const
{
    return m_adjust;
}

ShapeTool *StudioCore::shapeTool() const
{
    return m_shapeTool;
}

TextTool *StudioCore::textTool() const
{
    return m_textTool;
}

void StudioCore::fitView()
{
    ScribbleArea *area = canvas();
    Object *object = m_editor->object();
    if (!area || !object || area->width() < 10 || area->height() < 10) return;

    const std::vector<LayerCamera *> cameras = object->getLayersByType<LayerCamera>();
    if (cameras.empty()) return;
    const QRectF frame = cameras.front()->getViewRect();
    if (frame.isEmpty()) return;

    // A little room around the paper, so its edges read as edges.
    const qreal scale = 0.9 * qMin(area->width() / frame.width(), area->height() / frame.height());
    m_editor->view()->scale(scale);
    m_editor->view()->translate(-frame.center());

    m_fitted = true;
    m_fitScale = m_editor->view()->scaling();
    m_fitTranslation = m_editor->view()->translation();
}

ActionCommands *StudioCore::commands() const
{
    return m_commands;
}

StudioHistory *StudioCore::history() const
{
    return m_history;
}

StudioTools *StudioCore::tools() const
{
    return m_tools;
}

StudioLayers *StudioCore::layers() const
{
    return m_layers;
}

StudioTimeline *StudioCore::timeline() const
{
    return m_timeline;
}

StudioPlayback *StudioCore::playback() const
{
    return m_playback;
}

StudioPenButtons *StudioCore::penButtons() const
{
    return m_penButtons;
}

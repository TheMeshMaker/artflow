/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioTools.h"

#include "colormanager.h"
#include "editor.h"
#include "stroketool.h"
#include "toolmanager.h"

#include "StudioCore.h"

namespace {

QString toolIcon(const char *name)
{
    // Names of the shell's own line icons (shell/icons).
    return QLatin1String(name);
}

}

StudioTools::StudioTools(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
    connect(core->editor()->tools(), &ToolManager::toolChanged, this, &StudioTools::slotToolChanged);
    connect(core->editor()->color(), &ColorManager::colorChanged, this, &StudioTools::brushChanged);
    slotToolChanged(core->editor()->tools()->currentTool() ? core->editor()->tools()->currentTool()->type()
                                                           : INVALID_TOOL);
}

StudioTools::~StudioTools()
{
}

QVector<ToolIntentInfo> StudioTools::allIntents()
{
    // The beginner set. Letters follow what people know from other painting
    // apps (B brush, E eraser, G fill, I picker, V move, ...).
    return {
        {ToolIntent::Brush, QStringLiteral("brush"), tr("Brush"), QStringLiteral("B"), toolIcon("brush"), BRUSH},
        {ToolIntent::Pencil, QStringLiteral("pencil"), tr("Pencil"), QStringLiteral("N"), toolIcon("pencil"), PENCIL},
        {ToolIntent::Eraser, QStringLiteral("eraser"), tr("Eraser"), QStringLiteral("E"), toolIcon("eraser"), ERASER},
        {ToolIntent::Smudge, QStringLiteral("smudge"), tr("Smudge"), QStringLiteral("S"), toolIcon("smudge"), SMUDGE},
        {ToolIntent::Fill, QStringLiteral("fill"), tr("Fill"), QStringLiteral("G"), toolIcon("fill"), BUCKET},
        {ToolIntent::LassoFill, QStringLiteral("lasso"), tr("Lasso Fill"), QStringLiteral("L"), toolIcon("lasso"), LASSOFILL},
        {ToolIntent::Picker, QStringLiteral("picker"), tr("Picker"), QStringLiteral("I"), toolIcon("picker"), EYEDROPPER},
        {ToolIntent::Select, QStringLiteral("select"), tr("Select"), QStringLiteral("M"), toolIcon("select"), SELECT},
        {ToolIntent::Move, QStringLiteral("move"), tr("Move"), QStringLiteral("V"), toolIcon("move"), MOVE},
        {ToolIntent::Shapes, QStringLiteral("shapes"), tr("Shapes"), QStringLiteral("U"), toolIcon("shapes"), SHAPES},
        {ToolIntent::Text, QStringLiteral("text"), tr("Text"), QStringLiteral("T"), toolIcon("text"), TEXT},
        {ToolIntent::Camera, QStringLiteral("camera"), tr("Camera"), QStringLiteral("C"), toolIcon("camera"), CAMERA},
    };
}

ToolIntentInfo StudioTools::info(ToolIntent intent)
{
    for (const ToolIntentInfo &info : allIntents()) {
        if (info.intent == intent) return info;
    }
    return allIntents().first();
}

ToolIntent StudioTools::currentIntent() const
{
    return m_current;
}

bool StudioTools::activate(ToolIntent intent)
{
    if (intent == ToolIntent::Eraser && m_current != ToolIntent::Eraser) {
        m_beforeEraser = m_current;
    }

    m_switching = true;
    m_core->editor()->tools()->setCurrentTool(info(intent).tool);
    m_switching = false;

    setCurrent(intent);
    slotToolChanged(info(intent).tool);
    return true;
}

void StudioTools::toggleEraser()
{
    activate(m_current == ToolIntent::Eraser ? m_beforeEraser : ToolIntent::Eraser);
}

StrokeTool *StudioTools::strokeTool() const
{
    BaseTool *tool = m_core->editor()->tools()->currentTool();
    // Shapes are outlined with the brush, at the brush's size.
    if (tool && tool->type() == SHAPES) tool = m_core->editor()->tools()->getTool(BRUSH);
    return qobject_cast<StrokeTool *>(tool);
}

qreal StudioTools::brushSize() const
{
    StrokeTool *tool = strokeTool();
    return tool ? tool->strokeToolProperties().width() : 0.0;
}

void StudioTools::setBrushSize(qreal size)
{
    if (StrokeTool *tool = strokeTool()) tool->setWidth(qMax(1.0, size));
}

qreal StudioTools::brushOpacity() const
{
    return m_core->editor()->color()->frontColor().alphaF();
}

void StudioTools::setBrushOpacity(qreal opacity)
{
    QColor color = m_core->editor()->color()->frontColor();
    color.setAlphaF(qBound(0.01, opacity, 1.0));
    m_core->editor()->color()->setFrontColor(color);
}

void StudioTools::slotToolChanged(ToolType tool)
{
    // Follow width changes of whichever stroke tool is current.
    StrokeTool *stroke = strokeTool();
    if (stroke != m_watchedTool) {
        if (m_watchedTool) disconnect(m_watchedTool, nullptr, this, nullptr);
        m_watchedTool = stroke;
        if (m_watchedTool) connect(m_watchedTool, &StrokeTool::widthChanged, this, &StudioTools::brushChanged);
    }
    Q_EMIT brushChanged();

    if (m_switching || tool == INVALID_TOOL) return;

    // The tool changed from elsewhere (an engine shortcut, the eraser end of a pen...).
    for (const ToolIntentInfo &info : allIntents()) {
        if (info.tool == tool) {
            setCurrent(info.intent);
            return;
        }
    }
}

void StudioTools::setCurrent(ToolIntent intent)
{
    if (intent == m_current) return;
    m_current = intent;
    Q_EMIT intentChanged(intent);
}

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioCore.h"

#include "actioncommands.h"
#include "editor.h"
#include "mainwindow2.h"

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

    connect(m_editor, &Editor::objectLoaded, this, &StudioCore::documentChanged);
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

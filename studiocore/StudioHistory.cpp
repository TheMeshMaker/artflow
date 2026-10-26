/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioHistory.h"

#include <QAction>
#include <QIcon>

#include "editor.h"
#include "undoredomanager.h"

#include "StudioCore.h"

StudioHistory::StudioHistory(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
    UndoRedoManager *manager = core->editor()->undoRedo();
    m_undo = manager->createUndoAction(this, QIcon());
    m_redo = manager->createRedoAction(this, QIcon());

    connect(m_undo, &QAction::changed, this, &StudioHistory::changed);
    connect(m_redo, &QAction::changed, this, &StudioHistory::changed);
    // The legacy backup system only updates actions on request.
    connect(manager, &UndoRedoManager::didUpdateUndoStack, this, &StudioHistory::refresh);
    refresh();
}

StudioHistory::~StudioHistory()
{
}

bool StudioHistory::canUndo() const
{
    return m_undo->isEnabled();
}

bool StudioHistory::canRedo() const
{
    return m_redo->isEnabled();
}

QString StudioHistory::undoText() const
{
    return m_undo->text();
}

QString StudioHistory::redoText() const
{
    return m_redo->text();
}

void StudioHistory::undo()
{
    if (m_undo->isEnabled()) m_undo->trigger();
}

void StudioHistory::redo()
{
    if (m_redo->isEnabled()) m_redo->trigger();
}

void StudioHistory::refresh()
{
    UndoRedoManager *manager = m_core->editor()->undoRedo();
    manager->updateUndoAction(m_undo);
    manager->updateRedoAction(m_redo);
    Q_EMIT changed();
}

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ModeManager.h"

#include <StudioCore.h>

ModeManager::ModeManager(StudioCore *core, QObject *parent)
    : QObject(parent)
    , m_core(core)
{
}

ModeManager::~ModeManager()
{
}

void ModeManager::registerMode(const ModeDescriptor &mode)
{
    for (ModeDescriptor &existing : m_modes) {
        if (existing.id == mode.id) {
            existing = mode;
            return;
        }
    }
    m_modes.append(mode);
}

const QVector<ModeDescriptor> &ModeManager::modes() const
{
    return m_modes;
}

const ModeDescriptor *ModeManager::mode(const QString &id) const
{
    for (const ModeDescriptor &mode : m_modes) {
        if (mode.id == id) return &mode;
    }
    return nullptr;
}

QString ModeManager::currentModeId() const
{
    return m_current;
}

const ModeDescriptor *ModeManager::currentMode() const
{
    return mode(m_current);
}

bool ModeManager::switchTo(const QString &id)
{
    const ModeDescriptor *next = mode(id);
    if (!next) return false;
    if (id == m_current) return true;

    StudioTools *tools = m_core->tools();
    if (!m_current.isEmpty()) {
        m_lastTool[m_current] = tools->currentIntent();
    }

    const QString previous = m_current;
    m_current = id;

    ToolIntent tool = m_lastTool.value(id, next->defaultTool);
    if (!next->tools.contains(tool)) {
        tool = next->defaultTool;
    }
    tools->activate(tool);

    Q_EMIT modeChanged(id, previous);
    return true;
}

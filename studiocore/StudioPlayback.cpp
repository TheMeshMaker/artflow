/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioPlayback.h"

#include "actioncommands.h"
#include "editor.h"
#include "layermanager.h"
#include "playbackmanager.h"

#include "StudioCore.h"

StudioPlayback::StudioPlayback(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
    connect(core->editor()->playback(), &PlaybackManager::playStateChanged, this, &StudioPlayback::playingChanged);
}

StudioPlayback::~StudioPlayback()
{
}

bool StudioPlayback::isPlaying() const
{
    return m_core->editor()->playback()->isPlaying();
}

void StudioPlayback::play()
{
    if (!isPlaying()) m_core->editor()->playback()->play();
}

void StudioPlayback::stop()
{
    if (isPlaying()) m_core->editor()->playback()->stop();
}

void StudioPlayback::togglePlay()
{
    isPlaying() ? stop() : play();
}

void StudioPlayback::nextFrame()
{
    m_core->commands()->GotoNextFrame();
}

void StudioPlayback::previousFrame()
{
    m_core->commands()->GotoPrevFrame();
}

void StudioPlayback::firstFrame()
{
    m_core->editor()->scrubTo(1);
}

void StudioPlayback::lastFrame()
{
    m_core->editor()->scrubTo(qMax(1, m_core->editor()->layers()->lastKeyFrameIndex()));
}

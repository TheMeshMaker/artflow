/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOPLAYBACK_H
#define STUDIOPLAYBACK_H

#include <QObject>

class StudioCore;

/// Transport controls over the engine's PlaybackManager.
class StudioPlayback : public QObject
{
    Q_OBJECT
public:
    explicit StudioPlayback(StudioCore *core);
    ~StudioPlayback() override;

    bool isPlaying() const;

public Q_SLOTS:
    void play();
    void stop();
    void togglePlay();
    void nextFrame();
    void previousFrame();
    void firstFrame();
    void lastFrame();

Q_SIGNALS:
    void playingChanged(bool playing);

private:
    StudioCore *m_core;
};

#endif // STUDIOPLAYBACK_H

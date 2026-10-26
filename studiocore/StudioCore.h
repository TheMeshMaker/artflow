/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOCORE_H
#define STUDIOCORE_H

#include <QObject>
#include <QPointer>

class ActionCommands;
class Editor;
class MainWindow2;
class ScribbleArea;

class StudioHistory;
class StudioLayers;
class StudioPenButtons;
class StudioPlayback;
class StudioTimeline;
class StudioTools;

/**
 * The single entry point of the facade over the Pencil2D core.
 *
 * The shell never reaches into the engine directly: it asks StudioCore
 * (and the objects it owns) to do things, and they route the requests
 * through the engine's own Editor, managers and ActionCommands, so undo
 * records and change notifications happen exactly as in Pencil2D.
 */
class StudioCore : public QObject
{
    Q_OBJECT
public:
    explicit StudioCore(MainWindow2 *window, QObject *parent = nullptr);
    ~StudioCore() override;

    MainWindow2 *window() const;
    Editor *editor() const;
    ScribbleArea *canvas() const;
    ActionCommands *commands() const;

    StudioHistory *history() const;
    StudioTools *tools() const;
    StudioLayers *layers() const;
    StudioTimeline *timeline() const;
    StudioPlayback *playback() const;
    StudioPenButtons *penButtons() const;

Q_SIGNALS:
    /// A new or opened document replaced the current one.
    void documentChanged();

private:
    QPointer<MainWindow2> m_window;
    Editor *m_editor;
    ActionCommands *m_commands;

    StudioHistory *m_history;
    StudioTools *m_tools;
    StudioLayers *m_layers;
    StudioTimeline *m_timeline;
    StudioPlayback *m_playback;
    StudioPenButtons *m_penButtons;
};

#endif // STUDIOCORE_H

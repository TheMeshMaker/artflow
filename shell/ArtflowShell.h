/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_SHELL_H
#define ARTFLOW_SHELL_H

#include <QHash>
#include <QObject>
#include <QPointer>

#include "StudioTools.h"

class QButtonGroup;
class QDockWidget;
class QHBoxLayout;
class QStackedWidget;
class QToolBar;
class QToolButton;
class QWidget;
class MainWindow2;
class ModeManager;
class ModeSwitch;
class StudioCore;
class StudioTimelineModel;

/**
 * The ArtFlow chrome around the engine's main window: a top bar (modes,
 * tools, undo/redo, pen settings, help), a panel under the canvas, a
 * layer panel and brush sliders beside it. The canvas (ScribbleArea) and
 * the Editor are never re-created, which keeps the drawing, selection,
 * undo history and view intact across modes.
 */
class ArtflowShell : public QObject
{
    Q_OBJECT
public:
    explicit ArtflowShell(MainWindow2 *window, QObject *parent = nullptr);
    ~ArtflowShell() override;

    StudioCore *core() const;
    ModeManager *modes() const;
    MainWindow2 *window() const;

    /// Builds the chrome and enters \p modeId (or the last used mode).
    void start(const QString &modeId = QString());

public Q_SLOTS:
    void showPenButtonsDialog();
    void showCheatSheet();

private Q_SLOTS:
    void slotModeChanged(const QString &id, const QString &previous);
    void slotIntentChanged(ToolIntent intent);
    void slotHistoryChanged();
    void applyChrome();
    void fitToolOptions();
    void resizeRightColumn();
    void fixWindowTitle();

private:
    void buildTopBar();
    void buildDocks();
    void buildActions();
    void rebuildToolStrip();
    void populateMenu();
    void arrangeRightColumn(int width);
    QList<QDockWidget *> rightColumn() const;

private:
    QPointer<MainWindow2> m_window;
    StudioCore *m_core;
    ModeManager *m_modes;
    StudioTimelineModel *m_model;

    QToolBar *m_topBar {nullptr};
    ModeSwitch *m_modeSwitch {nullptr};
    QWidget *m_toolStrip {nullptr};
    QHBoxLayout *m_toolStripLayout {nullptr};
    QButtonGroup *m_toolButtons {nullptr};
    QToolButton *m_undo {nullptr};
    QToolButton *m_redo {nullptr};
    QToolButton *m_menuButton {nullptr};

    QDockWidget *m_timelineDock {nullptr};
    QStackedWidget *m_timelineStack {nullptr};
    QHash<QString, QWidget *> m_timelinePanels;
    QDockWidget *m_layersDock {nullptr};
    QDockWidget *m_brushDock {nullptr};
};

#endif // ARTFLOW_SHELL_H

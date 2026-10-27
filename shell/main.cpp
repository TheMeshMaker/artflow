/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Start-up of ArtFlow: the Pencil2D core and its main window, with the
 *  ArtFlow shell (modes, beginner tools and keys, pen buttons) attached.
 *  Follows app/src/main.cpp and Pencil2D::prepareGuiStartup().
 */

#include <QApplication>
#include <QIcon>
#include <QSettings>

#include "log.h"
#include "mainwindow2.h"
#include "pencildef.h"
#include "platformhandler.h"

#include "ArtflowSelfTest.h"
#include "ArtflowTheme.h"
#include "ArtflowShell.h"
#include "BeginnerShortcuts.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(core_lib);
    PlatformHandler::initialise();
#ifdef Q_OS_WIN
    // Group ArtFlow's windows under its own name in the taskbar.
    SetCurrentProcessExplicitAppUserModelID(L"ArtFlow.ArtFlow");
#endif
    initCategoryLogging();

    QApplication app(argc, argv);
    // Settings, autosaves and the window layout are ArtFlow's own: the core's
    // QSettings(PENCIL2D, PENCIL2D) calls resolve to "ArtFlow" in this build.
    QApplication::setOrganizationName(QStringLiteral("ArtFlow"));
    QApplication::setOrganizationDomain(QStringLiteral("artflow.invalid"));
    QApplication::setApplicationName(QStringLiteral("ArtFlow"));
    QApplication::setApplicationDisplayName(QStringLiteral("ArtFlow"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/artflow/artflow.svg")));
    ArtflowTheme::apply(app);

    // The beginner key map has to be in the settings before the main
    // window reads its shortcuts.
    BeginnerShortcuts::writeKeyMap();

    {
        // ArtFlow's own defaults, applied once per version so a later change
        // in the preferences sticks.
        const int DefaultsVersion = 2;
        QSettings settings(PENCIL2D, PENCIL2D);
        const int applied = settings.value(QStringLiteral("ArtFlowDefaultsVersion")).toInt();
        if (applied < 1) {
            // The engine's newer undo system records strokes, fills, moves and
            // frame edits as real undo steps and redoes them correctly (the
            // legacy one cannot redo a deleted frame).
            settings.setValue(SETTING_NEW_UNDO_REDO_ON, true);
        }
        if (applied < 2) {
            // The palette as a grid of swatches rather than a list of names.
            settings.setValue(QStringLiteral("ColorPaletteViewMode"), QStringLiteral("GridMode"));
            settings.setValue(QStringLiteral("PreferredColorGridSize"), 28);
        }
        if (applied < DefaultsVersion) {
            settings.setValue(QStringLiteral("ArtFlowDefaultsVersion"), DefaultsVersion);
        }
    }

    PlatformHandler::configurePlatformSpecificSettings();

    MainWindow2 window;
    ArtflowShell shell(&window);
    shell.start(qEnvironmentVariable("ARTFLOW_MODE"));

    // First start: use the whole screen; afterwards the engine restores the
    // size the window had when it was closed.
    QSettings settings(PENCIL2D, PENCIL2D);
    if (!settings.value(QStringLiteral("ArtFlowStartedBefore")).toBool()) {
        settings.setValue(QStringLiteral("ArtFlowStartedBefore"), true);
        window.showMaximized();
    } else {
        window.show();
    }

    const QStringList args = QApplication::arguments();
    window.openStartupFile(args.size() > 1 ? args.at(1) : QString());

    if (qEnvironmentVariableIsSet("ARTFLOW_SELFTEST")) {
        ArtflowSelfTest::run(&shell, qEnvironmentVariable("ARTFLOW_SELFTEST"));
    }

    return QApplication::exec();
}

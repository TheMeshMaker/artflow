/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ArtflowShell.h"

#include <QAction>
#include <QButtonGroup>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include "mainwindow2.h"
#include "pencildef.h"

#include "StudioCore.h"
#include "StudioHistory.h"
#include "StudioTimelineModel.h"
#include "StudioTools.h"

#include "BeginnerShortcuts.h"
#include "ModeManager.h"
#include "PenButtonsDialog.h"
#include "panels/BrushSlidersPanel.h"
#include "panels/LayerStackPanel.h"
#include "panels/ShellWidgets.h"

namespace {

const QString DefaultMode = QStringLiteral("flip");
const char ShellAction[] = "artflowAction";

QString withKeys(const QString &text, const QString &keys)
{
    return keys.isEmpty() ? text : QStringLiteral("%1  (%2)").arg(text, keys);
}

QIcon themeIcon(const char *path)
{
    return QIcon(QStringLiteral(":/icons/themes/playful/%1.svg").arg(QLatin1String(path)));
}

}

ArtflowShell::ArtflowShell(MainWindow2 *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_core(new StudioCore(window, this))
    , m_modes(new ModeManager(m_core, this))
    , m_model(new StudioTimelineModel(m_core, this))
{
    registerDefaultModes(m_modes);

    connect(m_modes, &ModeManager::modeChanged, this, &ArtflowShell::slotModeChanged);
    connect(m_core->tools(), &StudioTools::intentChanged, this, &ArtflowShell::slotIntentChanged);
    connect(m_core->history(), &StudioHistory::changed, this, &ArtflowShell::slotHistoryChanged);
}

ArtflowShell::~ArtflowShell()
{
}

StudioCore *ArtflowShell::core() const
{
    return m_core;
}

ModeManager *ArtflowShell::modes() const
{
    return m_modes;
}

MainWindow2 *ArtflowShell::window() const
{
    return m_window;
}

void ArtflowShell::start(const QString &modeId)
{
    buildTopBar();
    buildDocks();
    buildActions();

    // The engine's actions live in its (hidden) menus; attach them to the
    // window as well so their shortcuts keep working without a menu bar.
    for (QAction *action : m_window->findChildren<QAction *>()) {
        if (!action->shortcut().isEmpty() && !action->property(ShellAction).toBool()) {
            m_window->addAction(action);
        }
    }

    // Beginners should not be able to tear panels off by accident.
    m_window->lockWidgets(true);

    QString mode = modeId;
    if (mode.isEmpty()) {
        mode = QSettings(PENCIL2D, PENCIL2D).value(QStringLiteral("ArtFlowLastMode"), DefaultMode).toString();
    }
    if (!m_modes->mode(mode)) mode = DefaultMode;

    m_modes->switchTo(mode);
    slotHistoryChanged();
}

void ArtflowShell::buildTopBar()
{
    m_topBar = new QToolBar(tr("ArtFlow"), m_window);
    m_topBar->setObjectName(QStringLiteral("ArtFlowTopBar"));
    m_topBar->setMovable(false);
    m_topBar->setFloatable(false);
    m_topBar->setIconSize(QSize(24, 24));
    m_topBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // Everything the engine offers stays reachable from one menu button.
    m_menuButton = new QToolButton(m_topBar);
    m_menuButton->setIcon(themeIcon("misc/more-options"));
    m_menuButton->setToolTip(tr("All menus"));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu = new QMenu(m_menuButton);
    connect(menu, &QMenu::aboutToShow, this, &ArtflowShell::populateMenu);
    m_menuButton->setMenu(menu);
    m_topBar->addWidget(m_menuButton);
    m_topBar->addSeparator();

    m_modeButtons = new QButtonGroup(this);
    m_modeButtons->setExclusive(true);
    int index = 1;
    for (const ModeDescriptor &mode : m_modes->modes()) {
        QToolButton *button = new QToolButton(m_topBar);
        button->setText(mode.title);
        button->setToolTip(withKeys(mode.tagline, QStringLiteral("Ctrl+Alt+%1").arg(index++)));
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setProperty("artflowMode", mode.id);
        QFont font = button->font();
        font.setBold(true);
        button->setFont(font);
        button->setMinimumWidth(64);
        m_modeButtons->addButton(button);
        m_topBar->addWidget(button);

        const QString id = mode.id;
        connect(button, &QToolButton::clicked, this, [this, id]() { m_modes->switchTo(id); });
    }
    m_topBar->addSeparator();

    m_toolStrip = new QWidget(m_topBar);
    m_toolStripLayout = new QHBoxLayout(m_toolStrip);
    m_toolStripLayout->setContentsMargins(0, 0, 0, 0);
    m_toolStripLayout->setSpacing(2);
    m_toolButtons = new QButtonGroup(this);
    m_toolButtons->setExclusive(true);
    m_topBar->addWidget(m_toolStrip);

    QWidget *spacer = new QWidget(m_topBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_topBar->addWidget(spacer);

    auto addButton = [this](const QIcon &icon, const QString &text, const QString &toolTip) {
        QToolButton *button = new QToolButton(m_topBar);
        button->setIcon(icon);
        button->setText(text);
        button->setToolTip(toolTip);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        m_topBar->addWidget(button);
        return button;
    };

    m_undo = addButton(themeIcon("menubar/undo"), tr("Undo"), QString());
    connect(m_undo, &QToolButton::clicked, m_core->history(), &StudioHistory::undo);
    m_redo = addButton(themeIcon("menubar/redo"), tr("Redo"), QString());
    connect(m_redo, &QToolButton::clicked, m_core->history(), &StudioHistory::redo);

    m_topBar->addSeparator();

    QToolButton *pen = addButton(themeIcon("tools/tool-pen"), tr("Pen"),
                                 tr("Pen buttons: choose what the side buttons of your pen do"));
    connect(pen, &QToolButton::clicked, this, &ArtflowShell::showPenButtonsDialog);

    QToolButton *help = addButton(QIcon(), QStringLiteral("?"), withKeys(tr("All keys and gestures"), QStringLiteral("F1")));
    help->setToolButtonStyle(Qt::ToolButtonTextOnly);
    QFont font = help->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.6);
    help->setFont(font);
    connect(help, &QToolButton::clicked, this, &ArtflowShell::showCheatSheet);

    m_window->addToolBar(Qt::TopToolBarArea, m_topBar);
}

void ArtflowShell::buildDocks()
{
    auto makeDock = [this](const QString &title, const char *name, QWidget *content, Qt::DockWidgetArea area) {
        QDockWidget *dock = new QDockWidget(title, m_window);
        dock->setObjectName(QLatin1String(name));
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        dock->setTitleBarWidget(new QWidget(dock));
        content->setParent(dock);
        dock->setWidget(content);
        m_window->addDockWidget(area, dock);
        return dock;
    };

    // Panels sit around the canvas, never on top of it.
    m_timelineStack = new QStackedWidget();
    m_timelineDock = makeDock(tr("Timeline"), "ArtFlowTimelineDock", m_timelineStack, Qt::BottomDockWidgetArea);
    m_layersDock = makeDock(tr("Layers"), "ArtFlowLayersDock", new LayerStackPanel(m_core, m_model), Qt::RightDockWidgetArea);
    m_brushDock = makeDock(tr("Brush"), "ArtFlowBrushDock", new BrushSlidersPanel(m_core), Qt::LeftDockWidgetArea);
}

void ArtflowShell::buildActions()
{
    auto addAction = [this](const QString &text, const QList<QKeySequence> &keys, std::function<void()> slot) {
        QAction *action = new QAction(text, m_window);
        action->setShortcuts(keys);
        action->setShortcutContext(Qt::WindowShortcut);
        action->setProperty(ShellAction, true);
        connect(action, &QAction::triggered, this, slot);
        m_window->addAction(action);
    };

    int index = 1;
    for (const ModeDescriptor &mode : m_modes->modes()) {
        const QString id = mode.id;
        addAction(mode.title, {QKeySequence(QStringLiteral("Ctrl+Alt+%1").arg(index++))},
                  [this, id]() { m_modes->switchTo(id); });
    }

    // The engine has no command for the camera tool, nor for brush size keys.
    addAction(tr("Camera"), {QKeySequence(Qt::Key_C)}, [this]() {
        const ModeDescriptor *mode = m_modes->currentMode();
        if (mode && mode->tools.contains(ToolIntent::Camera)) m_core->tools()->activate(ToolIntent::Camera);
    });
    addAction(tr("Smaller brush"), {QKeySequence(Qt::Key_BracketLeft)}, [this]() {
        m_core->tools()->setBrushSize(m_core->tools()->brushSize() / 1.25);
    });
    addAction(tr("Bigger brush"), {QKeySequence(Qt::Key_BracketRight)}, [this]() {
        m_core->tools()->setBrushSize(m_core->tools()->brushSize() * 1.25);
    });
    addAction(tr("Redo"), {QKeySequence(QStringLiteral("Ctrl+Shift+Z"))}, [this]() { m_core->history()->redo(); });
    addAction(tr("Keys and gestures"), {QKeySequence(Qt::Key_F1), QKeySequence(QStringLiteral("?"))},
              [this]() { showCheatSheet(); });
}

void ArtflowShell::rebuildToolStrip()
{
    for (QAbstractButton *button : m_toolButtons->buttons()) {
        m_toolButtons->removeButton(button);
        delete button;
    }

    const ModeDescriptor *mode = m_modes->currentMode();
    if (!mode) return;

    for (ToolIntent intent : mode->tools) {
        const ToolIntentInfo info = StudioTools::info(intent);

        QToolButton *button = new QToolButton(m_toolStrip);
        button->setIcon(QIcon(info.icon));
        button->setText(info.label);
        button->setToolTip(withKeys(info.label, QKeySequence(info.shortcut).toString(QKeySequence::NativeText)));
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(24, 24));
        button->setMinimumWidth(50);
        button->setProperty("artflowIntent", int(intent));
        connect(button, &QToolButton::clicked, this, [this, intent]() { m_core->tools()->activate(intent); });

        m_toolButtons->addButton(button);
        m_toolStripLayout->addWidget(button);
    }

    slotIntentChanged(m_core->tools()->currentIntent());
}

void ArtflowShell::populateMenu()
{
    QMenu *menu = m_menuButton->menu();
    menu->clear();
    if (m_window && m_window->menuBar()) {
        menu->addActions(m_window->menuBar()->actions());
    }
}

void ArtflowShell::slotModeChanged(const QString &id, const QString &previous)
{
    Q_UNUSED(previous);

    const ModeDescriptor *mode = m_modes->mode(id);
    if (!mode) return;

    for (QAbstractButton *button : m_modeButtons->buttons()) {
        button->setChecked(button->property("artflowMode").toString() == id);
    }

    // Each mode's timeline panel is built once and then kept, so its scroll
    // position survives a round trip through the other modes.
    if (mode->createTimelinePanel && !m_timelinePanels.contains(id)) {
        QWidget *panel = mode->createTimelinePanel(m_core, m_model, m_timelineStack);
        m_timelineStack->addWidget(panel);
        m_timelinePanels.insert(id, panel);
    }
    if (QWidget *panel = m_timelinePanels.value(id)) {
        m_timelineStack->setCurrentWidget(panel);
    }

    rebuildToolStrip();
    applyChrome();

    QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowLastMode"), id);
}

void ArtflowShell::slotIntentChanged(ToolIntent intent)
{
    if (!m_toolButtons) return;

    for (QAbstractButton *button : m_toolButtons->buttons()) {
        if (button->property("artflowIntent").toInt() == int(intent)) {
            button->setChecked(true);
            return;
        }
    }

    // The active tool is not part of this mode's strip: uncheck everything.
    m_toolButtons->setExclusive(false);
    for (QAbstractButton *button : m_toolButtons->buttons()) {
        button->setChecked(false);
    }
    m_toolButtons->setExclusive(true);
}

void ArtflowShell::slotHistoryChanged()
{
    if (!m_undo) return;

    StudioHistory *history = m_core->history();
    m_undo->setEnabled(history->canUndo());
    m_redo->setEnabled(history->canRedo());
    m_undo->setToolTip(withKeys(history->undoText(), BeginnerShortcuts::keysFor(QStringLiteral("CmdUndo"))));
    m_redo->setToolTip(withKeys(history->redoText(), BeginnerShortcuts::keysFor(QStringLiteral("CmdRedo"))));
}

void ArtflowShell::applyChrome()
{
    const ModeDescriptor *mode = m_modes->currentMode();
    if (!m_window || !mode || !m_topBar) return;

    m_window->menuBar()->setVisible(false);
    for (QToolBar *toolBar : m_window->findChildren<QToolBar *>()) {
        toolBar->setVisible(toolBar == m_topBar);
    }
    m_window->statusBar()->setVisible(mode->showStatusBar);

    const QList<QDockWidget *> ours = {m_timelineDock, m_layersDock, m_brushDock};
    for (QDockWidget *dock : m_window->findChildren<QDockWidget *>()) {
        if (ours.contains(dock)) continue;
        const bool wanted = mode->engineDocks.contains(dock->objectName());
        if (wanted && (dock->isFloating() || m_window->dockWidgetArea(dock) != Qt::RightDockWidgetArea)) {
            // Engine panels always sit to the right of the canvas, never as
            // separate floating windows.
            dock->setFloating(false);
            m_window->addDockWidget(Qt::RightDockWidgetArea, dock);
        }
        dock->setVisible(wanted);
    }

    m_timelineDock->setVisible(m_timelinePanels.contains(mode->id));
    m_layersDock->setVisible(mode->showLayerStack);
    m_brushDock->setVisible(true);
}

void ArtflowShell::showPenButtonsDialog()
{
    PenButtonsDialog dialog(m_core->penButtons(), m_window);
    dialog.exec();
}

void ArtflowShell::showCheatSheet()
{
    CheatSheetDialog dialog(m_core->penButtons(), m_window);
    dialog.exec();
}

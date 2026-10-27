/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ArtflowShell.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolTip>
#include <QButtonGroup>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include "backgroundwidget.h"
#include "basetool.h"
#include "bitmapbrushengine.h"
#include "camerapainter.h"
#include "canvaspainter.h"
#include "editor.h"
#include "layer.h"
#include "layermanager.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "toolmanager.h"
#include "mainwindow2.h"
#include "pencildef.h"

#include "StudioAdjust.h"
#include "StudioBrushes.h"
#include "StudioCore.h"
#include "StudioHistory.h"
#include "tools/ShapeTool.h"
#include "tools/TextTool.h"
#include "StudioTimelineModel.h"
#include "StudioTools.h"

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"
#include "BeginnerShortcuts.h"
#include "ModeManager.h"
#include "PenButtonsDialog.h"
#include "panels/AdjustPanel.h"
#include "panels/BrushLibrary.h"
#include "panels/BrushSlidersPanel.h"
#include "panels/LayerStackPanel.h"
#include "panels/ShellControls.h"
#include "panels/ShellWidgets.h"
#include "panels/SymmetryGuide.h"

namespace {

const QString DefaultMode = QStringLiteral("flip");
const char ShellAction[] = "artflowAction";

/// The engine panels' buttons that carry its glossy icons, and ours instead.
void replaceEngineIcons(QWidget *panel)
{
    static const QList<QPair<const char *, const char *>> icons = {
        {"addColorButton", "plus"},
        {"removeColorButton", "minus"},
        {"colorDialogButton", "palette"},
        {"palettePref", "more"},
        {"resetTranslationButton", "move"},
        {"resetRotationButton", "rotate"},
        {"resetScaleButton", "scale"},
    };
    for (const auto &entry : icons) {
        if (QAbstractButton *button = panel->findChild<QAbstractButton *>(QLatin1String(entry.first))) {
            button->setIcon(ArtflowIcons::icon(QLatin1String(entry.second)));
            button->setIconSize(QSize(18, 18));
        }
    }
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

/// Small fixes that make the engine's panels sit in the ArtFlow look.
void polishEnginePanel(QDockWidget *dock)
{
    QWidget *content = dock->widget();
    if (!content) return;

    if (dock->objectName() == QLatin1String("ToolOption")) {
        if (content->layout()) content->layout()->setContentsMargins(12, 10, 12, 10);
        // "Width:" reads as "Width", and the number boxes line up in one column.
        for (QLabel *label : content->findChildren<QLabel *>()) {
            QString text = label->text().trimmed();
            if (!text.endsWith(QLatin1Char(':'))) continue;
            text.chop(1);
            label->setText(text);
        }
        for (QAbstractSpinBox *box : content->findChildren<QAbstractSpinBox *>()) {
            box->setFixedWidth(64);
            box->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
    } else if (dock->objectName() == QLatin1String("ColorPalette")) {
        if (content->layout()) content->layout()->setContentsMargins(10, 8, 10, 6);
        if (QHBoxLayout *row = content->findChild<QHBoxLayout *>(QStringLiteral("horizontalLayout"))) {
            row->setSpacing(2);
        }
        for (const char *name : {"addColorButton", "removeColorButton", "colorDialogButton"}) {
            if (QPushButton *button = content->findChild<QPushButton *>(QLatin1String(name))) {
                button->setFixedSize(30, 30);
                button->setProperty("artflowFlat", true);
                repolish(button);
            }
        }
        if (QListWidget *swatches = content->findChild<QListWidget *>(QStringLiteral("colorListWidget"))) {
            if (!qobject_cast<SwatchDelegate *>(swatches->itemDelegate())) {
                swatches->setItemDelegate(new SwatchDelegate(swatches));
            }
            swatches->setMouseTracking(true);
            // The engine keeps the grid's scroll bar on even when every swatch fits.
            swatches->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
    }
}

QString withKeys(const QString &text, const QString &keys)
{
    return keys.isEmpty() ? text : QStringLiteral("%1  (%2)").arg(text, keys);
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

    // The engine names the window after itself; keep ArtFlow's name.
    connect(window, &QWidget::windowTitleChanged, this, &ArtflowShell::fixWindowTitle);
    window->installEventFilter(this);

    // The engine shows its option rows again for every tool and layer change.
    connect(m_core->editor()->layers(), &LayerManager::currentLayerChanged, this, [this]() {
        QTimer::singleShot(0, this, &ArtflowShell::fitToolOptions);
    });
}

ArtflowShell::~ArtflowShell()
{
}

bool ArtflowShell::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::Show) {
        QTimer::singleShot(0, this, &ArtflowShell::applyChrome);
    }
    return QObject::eventFilter(watched, event);
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

    // White paper on a dark desk: darken everything outside the camera frame
    // to the window colour, and drop the engine's grey border around it.
    QColor surround = ArtflowTheme::canvasSurround();
    surround.setAlpha(238);
    CameraPainter::setOutsideShade(surround);
    // Drawings scale smoothly instead of in square pixels.
    CanvasPainter::setSmoothScaling(true);
    if (QWidget *canvas = m_core->canvas()) new SymmetryGuide(m_core, canvas);
    if (BackgroundWidget *background = m_window->findChild<BackgroundWidget *>()) {
        background->setStyleSheet(QStringLiteral("BackgroundWidget { background-color: white; border: none; }"));
    }
    fixWindowTitle();

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
    m_topBar->setIconSize(QSize(22, 22));

    auto iconButton = [this](const QString &icon, const QString &toolTip) {
        QToolButton *button = new QToolButton(m_topBar);
        button->setObjectName(QStringLiteral("iconButton"));
        button->setIcon(ArtflowIcons::icon(icon));
        button->setIconSize(QSize(22, 22));
        button->setToolTip(toolTip);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };

    // Everything the engine offers stays reachable from one menu button.
    m_menuButton = iconButton(QStringLiteral("menu"), tr("All menus"));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu = new QMenu(m_menuButton);
    connect(menu, &QMenu::aboutToShow, this, &ArtflowShell::populateMenu);
    m_menuButton->setMenu(menu);
    m_topBar->addWidget(m_menuButton);

    QWidget *gap = new QWidget(m_topBar);
    gap->setFixedWidth(8);
    m_topBar->addWidget(gap);

    m_modeSwitch = new ModeSwitch(m_topBar);
    QVector<ModeSwitch::Segment> segments;
    int index = 1;
    for (const ModeDescriptor &mode : m_modes->modes()) {
        segments << ModeSwitch::Segment{mode.id, mode.title, withKeys(mode.tagline, QStringLiteral("Ctrl+Alt+%1").arg(index++))};
    }
    m_modeSwitch->setSegments(segments);
    connect(m_modeSwitch, &ModeSwitch::segmentClicked, this, [this](const QString &id) { m_modes->switchTo(id); });
    m_topBar->addWidget(m_modeSwitch);

    m_topBar->addSeparator();

    m_toolStrip = new QWidget(m_topBar);
    m_toolStripLayout = new QHBoxLayout(m_toolStrip);
    m_toolStripLayout->setContentsMargins(0, 0, 0, 0);
    m_toolStripLayout->setSpacing(2);
    m_toolButtons = new QButtonGroup(this);
    m_toolButtons->setExclusive(true);
    m_topBar->addWidget(m_toolStrip);

    // The chosen tool's own options (shape, font...) sit right after the tools.
    m_contextBar = new QWidget(m_topBar);
    m_contextLayout = new QHBoxLayout(m_contextBar);
    m_contextLayout->setContentsMargins(14, 0, 0, 0);
    m_contextLayout->setSpacing(4);
    m_contextAction = m_topBar->addWidget(m_contextBar);
    // Options can change from elsewhere too; the bar follows without being
    // rebuilt, so the letter size box keeps its focus while typing.
    connect(m_core->shapeTool(), &ShapeTool::optionsChanged, this, &ArtflowShell::syncContextBar);
    connect(m_core->textTool(), &TextTool::optionsChanged, this, &ArtflowShell::syncContextBar);

    QWidget *spacer = new QWidget(m_topBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_topBar->addWidget(spacer);

    m_symmetry = iconButton(QStringLiteral("symmetry"),
                            tr("Symmetry: what you draw on one side of the page appears mirrored on the other"));
    m_symmetry->setObjectName(QStringLiteral("symmetryButton"));
    m_symmetry->setCheckable(true);
    connect(m_symmetry, &QToolButton::toggled, m_core->brushes(), &StudioBrushes::setSymmetry);
    m_topBar->addWidget(m_symmetry);

    m_adjustButton = iconButton(QStringLiteral("adjust"), tr("Adjust colours and blur of this drawing"));
    m_adjustButton->setObjectName(QStringLiteral("adjustButton"));
    connect(m_adjustButton, &QToolButton::clicked, this, &ArtflowShell::showAdjustPanel);
    m_topBar->addWidget(m_adjustButton);

    m_topBar->addSeparator();

    m_undo = iconButton(QStringLiteral("undo"), QString());
    connect(m_undo, &QToolButton::clicked, m_core->history(), &StudioHistory::undo);
    m_topBar->addWidget(m_undo);
    m_redo = iconButton(QStringLiteral("redo"), QString());
    connect(m_redo, &QToolButton::clicked, m_core->history(), &StudioHistory::redo);
    m_topBar->addWidget(m_redo);

    m_topBar->addSeparator();

    QToolButton *pen = iconButton(QStringLiteral("pen"), tr("Pen buttons: choose what the side buttons of your pen do"));
    connect(pen, &QToolButton::clicked, this, &ArtflowShell::showPenButtonsDialog);
    m_topBar->addWidget(pen);

    QToolButton *help = iconButton(QStringLiteral("help"), withKeys(tr("All keys and gestures"), QStringLiteral("F1")));
    connect(help, &QToolButton::clicked, this, &ArtflowShell::showCheatSheet);
    m_topBar->addWidget(help);

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
    LayerStackPanel *layers = new LayerStackPanel(m_core, m_model);
    connect(layers, &LayerStackPanel::heightLimitChanged, this, &ArtflowShell::resizeRightColumn);
    m_layersDock = makeDock(tr("Layers"), "ArtFlowLayersDock", layers, Qt::RightDockWidgetArea);
    BrushSlidersPanel *sliders = new BrushSlidersPanel(m_core);
    connect(sliders, &BrushSlidersPanel::brushStylesRequested, this, [this](QWidget *anchor) {
        showBrushLibrary(anchor, false);
    });
    m_brushDock = makeDock(tr("Brush"), "ArtFlowBrushDock", sliders, Qt::LeftDockWidgetArea);
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
    // ArtFlow's own tools have no engine command to carry their keys.
    for (const auto &key : QList<QPair<ToolIntent, Qt::Key>>{
             {ToolIntent::Shapes, Qt::Key_U}, {ToolIntent::LassoFill, Qt::Key_L}, {ToolIntent::Text, Qt::Key_T}}) {
        const ToolIntent intent = key.first;
        addAction(StudioTools::info(intent).label, {QKeySequence(key.second)}, [this, intent]() {
            const ModeDescriptor *mode = m_modes->currentMode();
            if (mode && mode->tools.contains(intent)) m_core->tools()->activate(intent);
        });
    }
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

        const bool styled = intent == ToolIntent::Brush || intent == ToolIntent::Pencil || intent == ToolIntent::Shapes;
        QToolButton *button = new QToolButton(m_toolStrip);
        button->setObjectName(QStringLiteral("toolTile"));
        button->setIcon(ArtflowIcons::icon(info.icon));
        const QString tip = withKeys(info.label, QKeySequence(info.shortcut).toString(QKeySequence::NativeText));
        button->setToolTip(styled ? tr("%1\nClick again for brush styles").arg(tip) : tip);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(22, 22));
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("artflowIntent", int(intent));
        // A second click on the chosen brush or pencil opens its styles.
        connect(button, &QToolButton::pressed, this, [this, button, intent]() {
            button->setProperty("artflowWasCurrent", m_core->tools()->currentIntent() == intent);
        });
        connect(button, &QToolButton::clicked, this, [this, button, intent, styled]() {
            const bool again = button->property("artflowWasCurrent").toBool();
            m_core->tools()->activate(intent);
            if (styled && again) showBrushLibrary(button, true);
        });

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

    m_modeSwitch->setCurrent(id);

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

    // Each tool shows other options; refit once the engine has swapped them.
    QTimer::singleShot(0, this, &ArtflowShell::fitToolOptions);
    rebuildContextBar();

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
        if (wanted) {
            ArtflowTheme::restyleEnginePanel(dock);
            replaceEngineIcons(dock);
            polishEnginePanel(dock);
        }
        dock->setVisible(wanted);
    }

    QWidget *panel = m_timelinePanels.value(mode->id);
    m_timelineDock->setVisible(panel != nullptr);
    if (panel) {
        // The Flip strip has one height; the Motion tracks grow with the layers.
        const bool fixed = panel->sizePolicy().verticalPolicy() == QSizePolicy::Fixed;
        m_timelineDock->setMaximumHeight(fixed ? panel->sizeHint().height() + 2 : 360);
    }
    m_layersDock->setVisible(mode->showLayerStack);
    m_brushDock->setVisible(true);

    arrangeRightColumn(mode->showLayerStack ? 300 : 280);
}

QList<QDockWidget *> ArtflowShell::rightColumn() const
{
    // One order in every mode: layers, the colour wheel and its swatches,
    // then the options of the current tool.
    QList<QDockWidget *> column;
    for (const char *name : {"ArtFlowLayersDock", "ColorWheel", "ColorPalette", "ToolOption"}) {
        QDockWidget *dock = m_window->findChild<QDockWidget *>(QLatin1String(name));
        if (dock && !dock->isHidden()) column << dock;
    }
    return column;
}

void ArtflowShell::arrangeRightColumn(int width)
{
    const QList<QDockWidget *> column = rightColumn();
    if (column.isEmpty()) return;

    for (QDockWidget *dock : column) {
        // Adding a dock that is already there moves it to the end.
        m_window->addDockWidget(Qt::RightDockWidgetArea, dock, Qt::Vertical);
        dock->show();

        // The sections share one panel colour; the gaps between them read
        // as dividers.
        if (QWidget *content = dock->widget()) {
            content->setAttribute(Qt::WA_StyledBackground);
            content->setProperty("artflowSection", true);
            repolish(content);
        }
    }

    // The wheel stays about square, the layers and the tool options end
    // right below their content, and the swatches get the rest.
    if (QDockWidget *wheel = m_window->findChild<QDockWidget *>(QStringLiteral("ColorWheel"))) {
        if (QWidget *content = wheel->widget()) {
            content->setMaximumHeight(width - 10);
            if (content->layout()) content->layout()->setContentsMargins(14, 12, 14, 8);
        }
    }
    fitToolOptions();

    // Sizes only mean something once the window has its real size; it
    // arranges the column again when it first shows.
    if (!m_window->isVisible()) return;
    m_window->resizeDocks(column, QList<int>(column.size(), width), Qt::Horizontal);
    resizeRightColumn();
}

void ArtflowShell::resizeRightColumn()
{
    // A section does not grow back by itself when its limit goes up, so the
    // whole column is laid out again. Asking for more than fits would shrink
    // every section in proportion, so the swatches ask for a little and grow
    // into whatever is left.
    if (!m_window || !m_window->isVisible()) return;
    const QList<QDockWidget *> column = rightColumn();
    QList<int> heights;
    for (QDockWidget *dock : column) {
        const int fitted = dock->widget() ? dock->widget()->maximumHeight() : QWIDGETSIZE_MAX;
        heights << (fitted < QWIDGETSIZE_MAX ? fitted : 160);
    }
    if (!column.isEmpty()) m_window->resizeDocks(column, heights, Qt::Vertical);
}

void ArtflowShell::showBrushLibrary(QWidget *anchor, bool below)
{
    const ToolIntent intent = m_core->tools()->currentIntent();
    if (intent != ToolIntent::Brush && intent != ToolIntent::Pencil && intent != ToolIntent::Shapes) return;

    if (!m_brushLibrary) m_brushLibrary = new BrushLibrary(m_core, m_window);
    const QPoint pos = below ? anchor->mapToGlobal(QPoint(0, anchor->height() + 8))
                             : anchor->mapToGlobal(QPoint(anchor->width() + 10, 0));
    m_brushLibrary->popup(intent == ToolIntent::Pencil ? PENCIL : BRUSH, pos);
}

void ArtflowShell::rebuildContextBar()
{
    if (!m_contextLayout) return;
    while (QLayoutItem *item = m_contextLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    auto option = [this](const QString &icon, const QString &tip, bool checked) {
        QToolButton *button = new QToolButton(m_contextBar);
        button->setObjectName(QStringLiteral("optionTile"));
        button->setIcon(ArtflowIcons::icon(icon));
        button->setIconSize(QSize(20, 20));
        button->setToolTip(tip);
        button->setCheckable(true);
        button->setChecked(checked);
        button->setCursor(Qt::PointingHandCursor);
        m_contextLayout->addWidget(button);
        return button;
    };

    const ToolIntent intent = m_core->tools()->currentIntent();
    if (intent == ToolIntent::Shapes) {
        ShapeTool *shapes = m_core->shapeTool();
        QButtonGroup *group = new QButtonGroup(m_contextBar);
        const QList<QPair<ShapeTool::Shape, QPair<const char *, QString>>> kinds = {
            {ShapeTool::Line, {"shape-line", tr("Line  (Shift: straight at 45 degrees)")}},
            {ShapeTool::Rectangle, {"shape-rect", tr("Rectangle  (Shift: square, Alt: from the centre)")}},
            {ShapeTool::Ellipse, {"shape-ellipse", tr("Ellipse  (Shift: circle, Alt: from the centre)")}},
        };
        for (const auto &kind : kinds) {
            QToolButton *button = option(QLatin1String(kind.second.first), kind.second.second, shapes->shape() == kind.first);
            button->setProperty("artflowShape", int(kind.first));
            group->addButton(button);
            const ShapeTool::Shape shape = kind.first;
            connect(button, &QToolButton::clicked, shapes, [shapes, shape]() { shapes->setShape(shape); });
        }
        m_contextLayout->addSpacing(6);
        QToolButton *fill = option(QStringLiteral("shape-fill"), tr("Fill the inside with the colour"), shapes->filled());
        fill->setProperty("artflowShapeFill", true);
        connect(fill, &QToolButton::toggled, shapes, &ShapeTool::setFilled);
    } else if (intent == ToolIntent::Text) {
        TextTool *text = m_core->textTool();
        QComboBox *style = new QComboBox(m_contextBar);
        style->addItems(TextTool::styleNames());
        style->setCurrentIndex(text->fontStyle());
        style->setToolTip(tr("Lettering style"));
        connect(style, qOverload<int>(&QComboBox::currentIndexChanged), text, &TextTool::setFontStyle);
        m_contextLayout->addWidget(style);

        QSpinBox *size = new QSpinBox(m_contextBar);
        size->setRange(6, 600);
        size->setValue(text->size());
        size->setSuffix(tr(" px"));
        size->setToolTip(tr("Letter size"));
        size->setFixedWidth(78);
        connect(size, qOverload<int>(&QSpinBox::valueChanged), text, &TextTool::setSize);
        m_contextLayout->addWidget(size);

        QToolButton *bold = option(QStringLiteral("bold"), tr("Bold"), text->bold());
        bold->setProperty("artflowBold", true);
        connect(bold, &QToolButton::toggled, text, &TextTool::setBold);
    }
    // A widget in a tool bar is shown and hidden through its action.
    m_contextAction->setVisible(m_contextLayout->count() > 0);
}

void ArtflowShell::syncContextBar()
{
    if (!m_contextBar) return;
    ShapeTool *shapes = m_core->shapeTool();
    TextTool *text = m_core->textTool();
    for (QToolButton *button : m_contextBar->findChildren<QToolButton *>()) {
        const QSignalBlocker blocker(button);
        if (button->property("artflowShape").isValid()) button->setChecked(button->property("artflowShape").toInt() == int(shapes->shape()));
        if (button->property("artflowShapeFill").toBool()) button->setChecked(shapes->filled());
        if (button->property("artflowBold").toBool()) button->setChecked(text->bold());
    }
    if (QComboBox *style = m_contextBar->findChild<QComboBox *>()) {
        const QSignalBlocker blocker(style);
        style->setCurrentIndex(text->fontStyle());
    }
    if (QSpinBox *size = m_contextBar->findChild<QSpinBox *>()) {
        if (size->value() != text->size()) {
            const QSignalBlocker blocker(size);
            size->setValue(text->size());
        }
    }
}

void ArtflowShell::showAdjustPanel()
{
    StudioAdjust *adjust = m_core->adjust();
    if (!adjust->begin()) {
        QToolTip::showText(m_adjustButton->mapToGlobal(QPoint(0, m_adjustButton->height() + 4)),
                           tr("Draw something on this frame first; Adjust works on drawing layers."), m_adjustButton);
        return;
    }

    AdjustPanel panel(m_core, m_core->editor()->select()->somethingSelected(), m_window);
    panel.adjustSize();
    QPoint pos = m_adjustButton->mapToGlobal(QPoint(m_adjustButton->width(), m_adjustButton->height() + 6));
    pos.rx() -= panel.width() - 14;
    pos.ry() -= 14;
    panel.move(pos);
    if (panel.exec() == QDialog::Accepted) {
        adjust->preview(panel.adjustments());
        adjust->apply();
    } else {
        adjust->cancel();
    }
}

void ArtflowShell::fitToolOptions()
{
    // The tool options are as tall as the current tool needs, and no taller.
    QDockWidget *dock = m_window ? m_window->findChild<QDockWidget *>(QStringLiteral("ToolOption")) : nullptr;
    QWidget *content = dock ? dock->widget() : nullptr;
    QWidget *options = dock ? dock->findChild<QWidget *>(QStringLiteral("scrollAreaWidgetContents")) : nullptr;
    if (!content || !options) return;

    // ArtFlow's brush engine shapes the edges itself: the engine's feather
    // and anti-aliasing switches do nothing for it on drawing layers.
    BaseTool *tool = m_core->editor()->tools()->currentTool();
    Layer *layer = m_core->editor()->layers()->currentLayer();
    BitmapBrushEngine *engine = BitmapBrushEngine::engine();
    if (tool && layer && layer->type() == Layer::BITMAP && engine && engine->handles(tool->type())) {
        for (const char *name : {"featherSlider", "featherSpinBox", "useFeatherBox", "useAABox"}) {
            if (QWidget *row = dock->findChild<QWidget *>(QLatin1String(name))) row->hide();
        }
    }

    int height = options->sizeHint().height() + 6;
    if (content->layout()) {
        height += content->layout()->contentsMargins().top() + content->layout()->contentsMargins().bottom();
    }
    height = qBound(48, height, 360);
    if (content->maximumHeight() == height) return;
    content->setMaximumHeight(height);
    resizeRightColumn();
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

void ArtflowShell::fixWindowTitle()
{
    if (!m_window) return;
    // "<file> - Pencil2D Development Build ..." becomes "<file> - ArtFlow".
    QString title = m_window->windowTitle();
    const int engine = title.indexOf(QLatin1String("Pencil2D"));
    if (engine < 0) return;

    title = title.left(engine).trimmed();
    if (title.endsWith(QLatin1Char('-'))) title.chop(1);
    // "[*]" is Qt's marker for unsaved changes; a new drawing has no name yet.
    title = title.remove(QLatin1String("[*]")).trimmed();
    if (title.isEmpty()) title = tr("Untitled");
    m_window->setWindowTitle(QStringLiteral("%1[*] - ArtFlow").arg(title));
}

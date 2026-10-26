/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioPenButtons.h"

#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QStringList>
#include <QTabletEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <QSettings>

#include "mainwindow2.h"
#include "pencildef.h"

#include "StudioCore.h"
#include "StudioHistory.h"
#include "StudioTools.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

/// A press this soon after the same button's release is contact bounce.
const qint64 DebounceMs = 150;
/// Context menus and stray clicks this soon after a gesture are swallowed too.
const qint64 GestureTailMs = 400;

const char ConfigGroup[] = "PenButtons";

QString buttonKey(StudioPenButtons::Button button)
{
    return button == StudioPenButtons::UpperButton ? QStringLiteral("Upper") : QStringLiteral("Lower");
}

QString mouseButtonName(int button)
{
    switch (button) {
    case Qt::RightButton: return QObject::tr("right", "mouse button");
    case Qt::MiddleButton: return QObject::tr("middle", "mouse button");
    case Qt::BackButton: return QObject::tr("back", "mouse button");
    case Qt::ForwardButton: return QObject::tr("forward", "mouse button");
    default: return QString::number(button, 16).prepend(QStringLiteral("0x"));
    }
}

bool isModifierKey(int key)
{
    return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt
        || key == Qt::Key_Meta || key == Qt::Key_AltGr || key == Qt::Key_unknown;
}

int keyCode(const QKeyEvent *event)
{
    return event->key() | int((event->modifiers() & ~Qt::KeypadModifier).toInt());
}

}

// ---------------------------------------------------------------- Signature

bool StudioPenButtons::Signature::matches(const Signature &event) const
{
    if (kind != event.kind || kind == None) return false;

    switch (kind) {
    case InkBarrel:
    case InkEraser:
        return true;
    case MouseButton:
        return code == event.code && (!fromPen || event.fromPen);
    default:
        return code == event.code;
    }
}

bool StudioPenButtons::Signature::operator==(const Signature &other) const
{
    return kind == other.kind && code == other.code && fromPen == other.fromPen;
}

QString StudioPenButtons::Signature::toString() const
{
    switch (kind) {
    case InkBarrel: return QStringLiteral("ink-barrel");
    case InkEraser: return QStringLiteral("ink-eraser");
    case TabletButton: return QStringLiteral("tablet-button:%1").arg(code);
    case MouseButton: return QStringLiteral("mouse-button:%1:%2").arg(code).arg(fromPen ? "pen" : "any");
    case Key: return QStringLiteral("key:%1").arg(code);
    case None: break;
    }
    return QStringLiteral("none");
}

StudioPenButtons::Signature StudioPenButtons::Signature::fromString(const QString &text)
{
    Signature signature;
    const QStringList parts = text.split(QLatin1Char(':'));

    if (parts[0] == QLatin1String("ink-barrel")) {
        signature.kind = InkBarrel;
    } else if (parts[0] == QLatin1String("ink-eraser")) {
        signature.kind = InkEraser;
    } else if (parts[0] == QLatin1String("tablet-button") && parts.size() >= 2) {
        signature.kind = TabletButton;
        signature.code = parts[1].toInt();
    } else if (parts[0] == QLatin1String("mouse-button") && parts.size() >= 2) {
        signature.kind = MouseButton;
        signature.code = parts[1].toInt();
        signature.fromPen = parts.size() < 3 || parts[2] != QLatin1String("any");
    } else if (parts[0] == QLatin1String("key") && parts.size() >= 2) {
        signature.kind = Key;
        signature.code = parts[1].toInt();
    }
    return signature;
}

QString StudioPenButtons::Signature::describe() const
{
    switch (kind) {
    case InkBarrel:
        return QObject::tr("Barrel button (Windows Ink)");
    case InkEraser:
        return QObject::tr("Eraser button (Windows Ink)");
    case TabletButton:
        return QObject::tr("Tablet button: %1").arg(mouseButtonName(code));
    case MouseButton:
        return fromPen ? QObject::tr("Pen click: %1 button").arg(mouseButtonName(code))
                       : QObject::tr("Mouse button: %1").arg(mouseButtonName(code));
    case Key:
        return QObject::tr("Key: %1").arg(QKeySequence(QKeyCombination::fromCombined(code)).toString(QKeySequence::NativeText));
    case None:
        break;
    }
    return QObject::tr("Not set");
}

// ---------------------------------------------------------- StudioPenButtons

StudioPenButtons::StudioPenButtons(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
    m_clock.start();
    resetToDefaults();
    load();

    qApp->installEventFilter(this);
#ifdef Q_OS_WIN
    qApp->installNativeEventFilter(this);
#endif
}

StudioPenButtons::~StudioPenButtons()
{
#ifdef Q_OS_WIN
    qApp->removeNativeEventFilter(this);
#endif
    qApp->removeEventFilter(this);
}

bool StudioPenButtons::isEnabled() const
{
    return m_enabled;
}

void StudioPenButtons::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

StudioPenButtons::Signature StudioPenButtons::signature(Button button) const
{
    return m_signatures[button];
}

void StudioPenButtons::setSignature(Button button, const Signature &signature)
{
    // One physical button can only drive one binding.
    const Button other = button == UpperButton ? LowerButton : UpperButton;
    if (signature.isValid() && m_signatures[other] == signature) {
        m_signatures[other] = Signature();
    }
    m_signatures[button] = signature;
}

StudioPenButtons::Action StudioPenButtons::action(Button button) const
{
    return m_actions[button];
}

void StudioPenButtons::setAction(Button button, Action action)
{
    m_actions[button] = action;
}

QString StudioPenButtons::actionName(Action action)
{
    switch (action) {
    case UndoAction: return tr("Undo");
    case RedoAction: return tr("Redo");
    case EraserAction: return tr("Toggle eraser");
    case NoAction: break;
    }
    return tr("Do nothing");
}

void StudioPenButtons::resetToDefaults()
{
    // The usual MPP layout: the button nearer the tip is the barrel
    // ("right click"), the other one is the eraser button. The detection
    // step fixes this up for pens that do it the other way round.
    m_signatures[UpperButton] = Signature{Signature::InkEraser, 0, true};
    m_signatures[LowerButton] = Signature{Signature::InkBarrel, 0, true};
    m_actions[UpperButton] = RedoAction;
    m_actions[LowerButton] = UndoAction;
    m_enabled = true;
}

void StudioPenButtons::load()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup(ConfigGroup);
    if (!settings.contains(QStringLiteral("Enabled"))) return;

    m_enabled = settings.value(QStringLiteral("Enabled"), true).toBool();
    for (int i = 0; i < ButtonCount; ++i) {
        const Button button = Button(i);
        const QString sig = settings.value(buttonKey(button) + QStringLiteral("Signature")).toString();
        if (!sig.isEmpty()) m_signatures[i] = Signature::fromString(sig);

        const int action = settings.value(buttonKey(button) + QStringLiteral("Action"), int(m_actions[i])).toInt();
        m_actions[i] = Action(qBound(int(NoAction), action, int(EraserAction)));
    }
}

void StudioPenButtons::save() const
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup(ConfigGroup);
    settings.setValue(QStringLiteral("Enabled"), m_enabled);
    for (int i = 0; i < ButtonCount; ++i) {
        const Button button = Button(i);
        settings.setValue(buttonKey(button) + QStringLiteral("Signature"), m_signatures[i].toString());
        settings.setValue(buttonKey(button) + QStringLiteral("Action"), int(m_actions[i]));
    }
}

void StudioPenButtons::startCapture()
{
    m_capturing = true;
    m_captureHeld = Signature();
}

void StudioPenButtons::cancelCapture()
{
    m_capturing = false;
}

bool StudioPenButtons::isCapturing() const
{
    return m_capturing;
}

bool StudioPenButtons::isRelevantTarget(QObject *watched) const
{
    // While capturing, the detection dialog itself is the target.
    if (m_capturing || m_captureHeld.isValid()) return true;
    if (!m_enabled) return false;

    MainWindow2 *mainWindow = m_core->window();
    if (!mainWindow) return false;

    if (QWidget *widget = qobject_cast<QWidget *>(watched)) {
        return widget->window() == mainWindow;
    }
    if (QWindow *window = qobject_cast<QWindow *>(watched)) {
        return window == mainWindow->windowHandle();
    }
    return false;
}

bool StudioPenButtons::isBound(Signature::Kind kind) const
{
    for (int i = 0; i < ButtonCount; ++i) {
        if (m_signatures[i].kind == kind && m_actions[i] != NoAction) return true;
    }
    return false;
}

bool StudioPenButtons::anyHeld() const
{
    return m_held[UpperButton] || m_held[LowerButton] || m_captureHeld.isValid()
        || m_inkBarrelDown || m_inkEraserDown;
}

bool StudioPenButtons::inGestureWindow() const
{
    return m_clock.elapsed() - m_lastGestureAt < GestureTailMs;
}

bool StudioPenButtons::handleEdge(const Signature &event, bool pressed)
{
    const qint64 now = m_clock.elapsed();

    // Key presses say nothing about the pen tip, everything else does.
    const bool tipRelated = event.kind != Signature::Key;

    if (m_capturing && pressed) {
        m_capturing = false;
        m_captureHeld = event;
        m_lastGestureAt = now;
        if (tipRelated) m_suppressUntilLift = true;
        Q_EMIT captured(event);
        return true;
    }
    if (!pressed && m_captureHeld.isValid() && m_captureHeld.matches(event)) {
        m_captureHeld = Signature();
        m_lastGestureAt = now;
        return true;
    }

    if (!m_enabled) return false;

    for (int i = 0; i < ButtonCount; ++i) {
        if (m_actions[i] == NoAction || !m_signatures[i].matches(event)) continue;

        m_lastGestureAt = now;

        if (pressed) {
            if (m_held[i]) return true;  // auto-repeat or a second report of the same press
            m_held[i] = true;
            if (tipRelated) m_suppressUntilLift = true;
            if (now - m_releasedAt[i] >= DebounceMs) {
                fire(Button(i));
            }
        } else if (m_held[i]) {
            m_held[i] = false;
            m_releasedAt[i] = now;
        }
        return true;
    }
    return false;
}

void StudioPenButtons::fire(Button button)
{
    const Action action = m_actions[button];

    // Run the action from the event loop rather than from inside the
    // native/event filter: undo may process events while it waits for
    // the image to become idle.
    QTimer::singleShot(0, this, [this, button, action]() {
        switch (action) {
        case UndoAction:
            m_core->history()->undo();
            break;
        case RedoAction:
            m_core->history()->redo();
            break;
        case EraserAction:
            m_core->tools()->toggleEraser();
            break;
        case NoAction:
            return;
        }
        Q_EMIT triggered(button, action);
    });
}

#ifdef Q_OS_WIN
static_assert(StudioPenButtons::InkBarrelFlag == PEN_FLAG_BARREL
              && StudioPenButtons::InkInvertedFlag == PEN_FLAG_INVERTED
              && StudioPenButtons::InkEraserFlag == PEN_FLAG_ERASER,
              "InkFlag must mirror the Windows PEN_FLAG_* bits");
#endif

void StudioPenButtons::injectInkFlags(quint32 penFlags)
{
    m_sawNativeInk = true;

    const bool barrel = penFlags & InkBarrelFlag;
    const bool eraser = penFlags & (InkEraserFlag | InkInvertedFlag);

    if (barrel != m_inkBarrelDown) {
        m_inkBarrelDown = barrel;
        handleEdge(Signature{Signature::InkBarrel, 0, true}, barrel);
    }
    if (eraser != m_inkEraserDown) {
        m_inkEraserDown = eraser;
        handleEdge(Signature{Signature::InkEraser, 0, true}, eraser);
    }
}

bool StudioPenButtons::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG") return false;

    const MSG *msg = static_cast<const MSG *>(message);
    switch (msg->message) {
    case WM_POINTERENTER:
    case WM_POINTERLEAVE:
    case WM_POINTERDOWN:
    case WM_POINTERUP:
    case WM_POINTERUPDATE:
        break;
    default:
        return false;
    }

    const UINT32 pointerId = GET_POINTERID_WPARAM(msg->wParam);
    POINTER_INPUT_TYPE pointerType = PT_POINTER;
    if (!GetPointerType(pointerId, &pointerType) || pointerType != PT_PEN) return false;

    if (!m_capturing && !m_captureHeld.isValid()) {
        if (!m_enabled) return false;

        // Only pen input aimed at our own windows counts.
        MainWindow2 *mainWindow = m_core->window();
        if (!mainWindow) return false;
        const HWND root = GetAncestor(msg->hwnd, GA_ROOT);
        if (root != reinterpret_cast<HWND>(mainWindow->winId())) {
            return false;
        }
    }

    quint32 flags = 0;
    if (msg->message != WM_POINTERLEAVE) {
        POINTER_PEN_INFO penInfo;
        if (!GetPointerPenInfo(pointerId, &penInfo)) return false;
        flags = penInfo.penFlags;
    }
    injectInkFlags(flags);
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    // Never eat the native message: Qt still needs it for the cursor and
    // for tip input. The Qt-level filter below removes what must not
    // reach the canvas.
    return false;
}

bool StudioPenButtons::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease:
    case QEvent::TabletEnterProximity:
    case QEvent::TabletLeaveProximity:
        return filterTablet(watched, static_cast<QTabletEvent *>(event));

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
        return filterMouse(watched, static_cast<QMouseEvent *>(event));

    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::ShortcutOverride:
        return filterKey(watched, static_cast<QKeyEvent *>(event));

    case QEvent::ContextMenu:
        if (isRelevantTarget(watched) && (anyHeld() || inGestureWindow())) {
            event->accept();
            return true;
        }
        return false;

    default:
        return false;
    }
}

bool StudioPenButtons::filterTablet(QObject *watched, QTabletEvent *event)
{
    // Proximity events have no target widget; they are global.
    const bool proximity = event->type() == QEvent::TabletEnterProximity
        || event->type() == QEvent::TabletLeaveProximity;
    if (!proximity && !isRelevantTarget(watched)) return false;
    if (proximity && !m_enabled && !m_capturing) return false;

    const QEvent::Type type = event->type();

    // 1. The eraser bit of an MPP pen is a button here, not a tool: drop
    //    eraser-typed events so the canvas never switches to the eraser end.
    if (event->pointerType() == QPointingDevice::PointerType::Eraser && (isBound(Signature::InkEraser) || m_capturing
                                                         || m_captureHeld.kind == Signature::InkEraser)) {
        if (!m_sawNativeInk) {
            // No native pen flags (another platform or tablet API): the
            // pointer type flipping is the only sign of the button.
            const bool down = type != QEvent::TabletLeaveProximity;
            if (down != m_inkEraserDown) {
                m_inkEraserDown = down;
                handleEdge(Signature{Signature::InkEraser, 0, true}, down);
            }
        }
        event->accept();
        return true;
    }

    if (proximity) {
        // The pen left or (re)entered the screen: no stroke is in progress.
        if (type == QEvent::TabletLeaveProximity || !anyHeld()) {
            m_suppressUntilLift = false;
        }
        return false;
    }

    // 2. Real tablet buttons (WinTab, X11, ...), and the right button Qt
    //    reports for the barrel while the tip touches the screen.
    if ((type == QEvent::TabletPress || type == QEvent::TabletRelease)
        && event->button() != Qt::LeftButton && event->button() != Qt::NoButton) {

        if (event->button() == Qt::RightButton && m_inkBarrelDown && isBound(Signature::InkBarrel)) {
            event->accept();
            return true;
        }

        const Signature sig{Signature::TabletButton, int(event->button()), true};
        if (handleEdge(sig, type == QEvent::TabletPress)) {
            event->accept();
            return true;
        }
    }

    // 3. A gesture happened: the tip must not start a stroke until it lifts.
    if (m_suppressUntilLift) {
        const bool tipDown = event->buttons() != Qt::NoButton;

        if (type == QEvent::TabletRelease) {
            if (!tipDown) m_suppressUntilLift = anyHeld();
            event->accept();
            return true;
        }
        if (type == QEvent::TabletPress || tipDown) {
            event->accept();
            return true;
        }
        if (!anyHeld()) {
            // Hovering again with all buttons up: back to normal.
            m_suppressUntilLift = false;
        }
    }

    return false;
}

bool StudioPenButtons::filterMouse(QObject *watched, QMouseEvent *event)
{
    if (!isRelevantTarget(watched)) return false;

    const bool fromPen = event->source() != Qt::MouseEventNotSynthesized;

    if (event->button() != Qt::LeftButton && event->button() != Qt::NoButton) {
        if (event->type() != QEvent::MouseButtonDblClick) {
            const Signature sig{Signature::MouseButton, int(event->button()), fromPen};
            if (handleEdge(sig, event->type() == QEvent::MouseButtonPress)) {
                event->accept();
                return true;
            }
        }
        // Right/middle clicks that Windows synthesizes from a pen button
        // would otherwise open the popup palette or a context menu.
        if (fromPen && (anyHeld() || inGestureWindow())) {
            event->accept();
            return true;
        }
    }

    if (fromPen && m_suppressUntilLift && event->type() != QEvent::MouseButtonRelease) {
        event->accept();
        return true;
    }
    return false;
}

bool StudioPenButtons::filterKey(QObject *watched, QKeyEvent *event)
{
    if (event->isAutoRepeat() || isModifierKey(event->key())) return false;
    if (!isRelevantTarget(watched)) return false;

    // Escape always closes the detection dialog instead of being captured.
    if (m_capturing && event->key() == Qt::Key_Escape) return false;

    const Signature sig{Signature::Key, keyCode(event), true};

    if (event->type() == QEvent::ShortcutOverride) {
        // Claim the key so no engine shortcut runs for a bound pen key.
        bool claim = m_capturing;
        for (int i = 0; i < ButtonCount; ++i) {
            if (m_enabled && m_actions[i] != NoAction && m_signatures[i].matches(sig)) {
                claim = true;
            }
        }
        if (claim) event->accept();
        return claim;
    }

    if (handleEdge(sig, event->type() == QEvent::KeyPress)) {
        event->accept();
        return true;
    }
    return false;
}

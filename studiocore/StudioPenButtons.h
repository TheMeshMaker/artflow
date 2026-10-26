/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOPENBUTTONS_H
#define STUDIOPENBUTTONS_H

#include <QAbstractNativeEventFilter>
#include <QElapsedTimer>
#include <QMetaType>
#include <QObject>
#include <QString>

class QKeyEvent;
class QMouseEvent;
class QTabletEvent;
class StudioCore;

/**
 * Turns the side buttons of a pen into one-shot actions (undo, redo,
 * eraser toggle) instead of mouse buttons.
 *
 * Pens report side buttons in different ways, so each button is bound to
 * a *signature* that the "press each button" step records:
 *
 *  - InkBarrel / InkEraser: the PEN_FLAG_BARREL and PEN_FLAG_ERASER /
 *    PEN_FLAG_INVERTED bits of Windows Ink (MPP pens such as the ASUS
 *    Pen 2.0). Qt only turns the barrel bit into a right button while the
 *    tip touches the screen and turns the eraser bit into an Eraser
 *    pointer type, so these are read from the raw WM_POINTER messages.
 *  - TabletButton: a real QTabletEvent button (WinTab drivers, Linux).
 *  - MouseButton: a right/middle/extra mouse button, optionally only
 *    when it was synthesized from pen input.
 *  - Key: some Bluetooth pens send key presses.
 *
 * A press fires its action once (debounced). While a pen button is in
 * play, the events it causes are swallowed before the canvas sees them,
 * so it never starts a stroke, never switches the tool to the eraser
 * end and never opens a context menu or the popup palette. Pressure and
 * tilt are untouched because tip events pass through unchanged.
 */
class StudioPenButtons : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    enum Button {
        UpperButton = 0,
        LowerButton = 1,
        ButtonCount
    };
    Q_ENUM(Button)

    enum Action {
        NoAction = 0,
        UndoAction,
        RedoAction,
        EraserAction,
    };
    Q_ENUM(Action)

    struct Signature
    {
        enum Kind {
            None = 0,
            InkBarrel,
            InkEraser,
            TabletButton,
            MouseButton,
            Key,
        };

        Kind kind {None};
        int code {0};         ///< Qt::MouseButton for button kinds, key|modifiers for Key
        bool fromPen {true};  ///< MouseButton only: match pen-synthesized clicks only

        bool isValid() const { return kind != None; }
        bool matches(const Signature &event) const;
        bool operator==(const Signature &other) const;

        QString toString() const;
        static Signature fromString(const QString &text);
        QString describe() const;
    };

    explicit StudioPenButtons(StudioCore *core);
    ~StudioPenButtons() override;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    Signature signature(Button button) const;
    void setSignature(Button button, const Signature &signature);

    Action action(Button button) const;
    void setAction(Button button, Action action);

    static QString actionName(Action action);

    void load();
    void save() const;
    void resetToDefaults();

    /**
     * The next pen-button press anywhere in the application is reported
     * through captured() instead of firing an action.
     */
    void startCapture();
    void cancelCapture();
    bool isCapturing() const;

    /// Windows Ink pen flag bits (same values as PEN_FLAG_* in winuser.h).
    enum InkFlag : quint32 {
        InkBarrelFlag = 0x1,
        InkInvertedFlag = 0x2,
        InkEraserFlag = 0x4,
    };

    /**
     * Feeds a set of pen flags as if they came with a WM_POINTER message.
     * The native filter uses it; so do the self-test and any future
     * tablet backend that reports the same bits.
     */
    void injectInkFlags(quint32 penFlags);

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    void captured(const StudioPenButtons::Signature &signature);
    /// Emitted after a bound button fired; mostly for feedback in the UI.
    void triggered(StudioPenButtons::Button button, StudioPenButtons::Action action);

private:
    bool isRelevantTarget(QObject *watched) const;
    bool isBound(Signature::Kind kind) const;
    bool anyHeld() const;
    bool inGestureWindow() const;

    /**
     * Feeds one press/release edge of \p event. Returns true when the
     * edge belongs to a bound button (or was captured), i.e. the event
     * behind it must not reach the canvas.
     */
    bool handleEdge(const Signature &event, bool pressed);
    void fire(Button button);

    bool filterTablet(QObject *watched, QTabletEvent *event);
    bool filterMouse(QObject *watched, QMouseEvent *event);
    bool filterKey(QObject *watched, QKeyEvent *event);

private:
    StudioCore *m_core;
    bool m_enabled {true};
    bool m_capturing {false};
    Signature m_captureHeld;

    Signature m_signatures[ButtonCount];
    Action m_actions[ButtonCount] {NoAction, NoAction};

    bool m_held[ButtonCount] {false, false};
    qint64 m_releasedAt[ButtonCount] {-1000, -1000};

    bool m_inkBarrelDown {false};
    bool m_inkEraserDown {false};
    bool m_sawNativeInk {false};

    /// After a pen-button gesture, swallow tip presses until the pen lifts.
    bool m_suppressUntilLift {false};
    qint64 m_lastGestureAt {-1000};

    QElapsedTimer m_clock;
};

Q_DECLARE_METATYPE(StudioPenButtons::Signature)

#endif // STUDIOPENBUTTONS_H

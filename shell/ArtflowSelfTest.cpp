/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ArtflowSelfTest.h"

#include <functional>

#include <QApplication>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QMouseEvent>
#include <QPointer>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QTextStream>
#include <QTimer>
#include <QVector>

#include "bitmapimage.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layermanager.h"
#include "mainwindow2.h"
#include "object.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "toolmanager.h"
#include "undoredomanager.h"

#include "StudioCore.h"
#include "StudioHistory.h"
#include "StudioLayers.h"
#include "StudioPenButtons.h"
#include "StudioTimeline.h"
#include "StudioTools.h"

#include "ArtflowShell.h"
#include "BeginnerShortcuts.h"
#include "ModeManager.h"

namespace {

const QPointingDevice *testDevice(QPointingDevice::PointerType type)
{
    static const QPointingDevice *pen = nullptr;
    static const QPointingDevice *eraser = nullptr;
    const auto caps = QInputDevice::Capability::Position | QInputDevice::Capability::Pressure
        | QInputDevice::Capability::XTilt | QInputDevice::Capability::YTilt;
    if (!pen) {
        pen = new QPointingDevice(QStringLiteral("ArtFlow self-test pen"), 4711, QInputDevice::DeviceType::Stylus,
                                  QPointingDevice::PointerType::Pen, caps, 1, 3);
        eraser = new QPointingDevice(QStringLiteral("ArtFlow self-test pen eraser"), 4712, QInputDevice::DeviceType::Stylus,
                                     QPointingDevice::PointerType::Eraser, caps, 1, 3);
    }
    return type == QPointingDevice::PointerType::Eraser ? eraser : pen;
}

class SelfTest : public QObject
{
public:
    SelfTest(ArtflowShell *shell, const QString &outputDir)
        : QObject(shell)
        , m_shell(shell)
        , m_core(shell->core())
        , m_out(outputDir)
    {
        m_out.mkpath(QStringLiteral("."));
        buildSteps();
    }

    void start()
    {
        log(QStringLiteral("ArtFlow self-test (Pencil2D core), %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
        QTimer::singleShot(1500, this, [this]() { runNext(); });
    }

private:
    // ------------------------------------------------------------ plumbing

    void log(const QString &line)
    {
        m_report << line;
        qInfo().noquote() << "[selftest]" << line;
    }

    void check(const QString &name, bool ok, const QString &details = QString())
    {
        if (!ok) ++m_failures;
        log(QStringLiteral("%1  %2%3").arg(ok ? "PASS" : "FAIL", name,
                                             details.isEmpty() ? QString() : QStringLiteral("  (%1)").arg(details)));
    }

    void settle(int rounds = 5)
    {
        for (int i = 0; i < rounds; ++i) QApplication::processEvents(QEventLoop::AllEvents, 40);
    }

    void runNext()
    {
        if (m_step >= m_steps.size()) {
            finish();
            return;
        }
        settle();
        m_steps[m_step++]();
        settle();
        QTimer::singleShot(200, this, [this]() { runNext(); });
    }

    void finish()
    {
        log(QStringLiteral("RESULT  %1 failure(s)").arg(m_failures));
        QFile file(m_out.absoluteFilePath(QStringLiteral("report.txt")));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream(&file) << m_report.join(QLatin1Char('\n')) << '\n';
        }
        if (qEnvironmentVariableIntValue("ARTFLOW_SELFTEST_QUIT")) {
            // The test drawing is throw-away: drop the unsaved changes, then quit.
            m_core->editor()->undoRedo()->clearStack();
            QApplication::exit(m_failures == 0 ? 0 : 3);
        }
    }

    Editor *editor() const { return m_core->editor(); }
    ScribbleArea *canvas() const { return m_core->canvas(); }

    QPointF at(qreal dx, qreal dy) const
    {
        return QPointF(canvas()->rect().center()) + QPointF(dx, dy);
    }

    static QVector<QPointF> line(const QPointF &from, const QPointF &to, int steps = 24)
    {
        QVector<QPointF> points;
        for (int i = 0; i <= steps; ++i) points << from + (to - from) * (qreal(i) / steps);
        return points;
    }

    void sendTablet(QEvent::Type type, const QPointF &pos, qreal pressure, Qt::MouseButton button,
                    Qt::MouseButtons buttons, QPointingDevice::PointerType pointer = QPointingDevice::PointerType::Pen)
    {
        QTabletEvent event(type, testDevice(pointer), pos, canvas()->mapToGlobal(pos), pressure, 25.f, -15.f, 0.f,
                           0.0, 0.f, Qt::NoModifier, button, buttons);
        QApplication::sendEvent(canvas(), &event);
    }

    /// A pen stroke with real pressure and tilt, the path an MPP pen takes.
    void penStroke(const QVector<QPointF> &points, qreal pressure = 0.7)
    {
        sendTablet(QEvent::TabletPress, points.first(), pressure, Qt::LeftButton, Qt::LeftButton);
        for (int i = 1; i < points.size(); ++i) {
            sendTablet(QEvent::TabletMove, points[i], pressure, Qt::NoButton, Qt::LeftButton);
            QApplication::processEvents();
        }
        sendTablet(QEvent::TabletRelease, points.last(), 0.0, Qt::LeftButton, Qt::NoButton);
        settle();
    }

    void mouseStroke(const QVector<QPointF> &points)
    {
        auto send = [this](QEvent::Type type, const QPointF &pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
            QMouseEvent event(type, pos, canvas()->mapToGlobal(pos), button, buttons, Qt::NoModifier);
            QApplication::sendEvent(canvas(), &event);
        };
        send(QEvent::MouseButtonPress, points.first(), Qt::LeftButton, Qt::LeftButton);
        for (int i = 1; i < points.size(); ++i) {
            send(QEvent::MouseMove, points[i], Qt::NoButton, Qt::LeftButton);
            QApplication::processEvents();
        }
        send(QEvent::MouseButtonRelease, points.last(), Qt::LeftButton, Qt::NoButton);
        settle();
    }

    QImage frameImage() const
    {
        Layer *layer = editor()->layers()->currentLayer();
        if (!layer || layer->type() != Layer::BITMAP) return QImage();
        BitmapImage *bitmap = static_cast<LayerBitmap *>(layer)->getLastBitmapImageAtFrame(editor()->currentFrame());
        return bitmap && bitmap->image() ? *bitmap->image() : QImage();
    }

    /// Non-transparent pixels of the drawing shown on the active layer.
    int paintedPixels() const
    {
        const QImage image = frameImage().convertToFormat(QImage::Format_ARGB32);
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(row[x]) > 8) ++count;
            }
        }
        return count;
    }

    QByteArray layerHash() const
    {
        const QImage image = frameImage().convertToFormat(QImage::Format_ARGB32);
        return QCryptographicHash::hash(QByteArray(reinterpret_cast<const char *>(image.constBits()), int(image.sizeInBytes())),
                                        QCryptographicHash::Sha1).toHex().left(12);
    }

    void screenshot(const QString &name)
    {
        m_shell->window()->grab().save(m_out.absoluteFilePath(name + QStringLiteral(".png")));
    }

    QTabletEvent tabletEvent(QEvent::Type type, Qt::MouseButton button, Qt::MouseButtons buttons,
                             QPointingDevice::PointerType pointer = QPointingDevice::PointerType::Pen) const
    {
        return QTabletEvent(type, testDevice(pointer), at(0, 0), canvas()->mapToGlobal(at(0, 0)), 0.5, 0.f, 0.f, 0.f,
                            0.0, 0.f, Qt::NoModifier, button, buttons);
    }

    // --------------------------------------------------------------- steps

    void buildSteps()
    {
        m_steps << [this]() {
            check(QStringLiteral("canvas is up"), canvas() && editor()->object());
            m_object = editor()->object();
            m_canvas = canvas();
            check(QStringLiteral("starts in a mode"), !m_shell->modes()->currentModeId().isEmpty(),
                  m_shell->modes()->currentModeId());

            // Draw on a bitmap layer.
            Layer *current = editor()->layers()->currentLayer();
            if (!current || current->type() != Layer::BITMAP) {
                for (int i = 0; i < editor()->layers()->count(); ++i) {
                    if (editor()->layers()->getLayer(i)->type() == Layer::BITMAP) {
                        editor()->layers()->setCurrentLayer(i);
                        break;
                    }
                }
            }
            check(QStringLiteral("active layer is a drawing layer"),
                  editor()->layers()->currentLayer()->type() == Layer::BITMAP, editor()->layers()->currentLayer()->name());
            canvas()->setFocus(Qt::OtherFocusReason);
            m_shell->modes()->switchTo(QStringLiteral("flip"));
            m_core->tools()->activate(ToolIntent::Brush);
            m_before = paintedPixels();
        };

        m_steps << [this]() { penStroke(line(at(-250, -40), at(250, -40))); };
        m_steps << [this]() {
            m_afterBrush = paintedPixels();
            check(QStringLiteral("brush draws (pen stroke, pressure 0.7, tilt)"), m_afterBrush > m_before + 200,
                  QStringLiteral("%1 -> %2 px").arg(m_before).arg(m_afterBrush));
            m_core->tools()->activate(ToolIntent::Eraser);
            m_core->tools()->setBrushSize(40);
            check(QStringLiteral("eraser intent active"), m_core->tools()->currentIntent() == ToolIntent::Eraser);
        };
        m_steps << [this]() {
            penStroke(line(at(-60, -120), at(-60, 60), 30), 1.0);
            penStroke(line(at(-30, -120), at(-30, 60), 30), 1.0);
        };
        m_steps << [this]() {
            m_afterEraser = paintedPixels();
            check(QStringLiteral("eraser removes paint"), m_afterEraser < m_afterBrush && m_afterEraser > 0,
                  QStringLiteral("%1 -> %2 px").arg(m_afterBrush).arg(m_afterEraser));
            m_core->tools()->activate(ToolIntent::Fill);
            check(QStringLiteral("fill intent active"), m_core->tools()->currentIntent() == ToolIntent::Fill);
        };
        m_steps << [this]() { penStroke({at(0, 200), at(0, 200)}); };
        m_steps << [this]() {
            const int now = paintedPixels();
            check(QStringLiteral("fill floods the empty area"), now > m_afterEraser + 10000,
                  QStringLiteral("%1 -> %2 px").arg(m_afterEraser).arg(now));
            m_core->history()->undo();
        };
        m_steps << [this]() {
            check(QStringLiteral("undo takes the fill back"), paintedPixels() == m_afterEraser,
                  QStringLiteral("%1 px").arg(paintedPixels()));
            m_core->tools()->activate(ToolIntent::Select);
        };
        m_steps << [this]() { penStroke(line(at(-150, -150), at(150, 100), 20)); };
        m_steps << [this]() {
            check(QStringLiteral("select makes a selection"), editor()->select()->somethingSelected());
            editor()->deselectAll();
            m_core->tools()->activate(ToolIntent::Brush);
        };

        // Mode round trip keeps everything
        m_steps << [this]() {
            m_hash = layerHash();
            m_canUndo = m_core->history()->canUndo();
            screenshot(QStringLiteral("mode-flip"));
            m_shell->modes()->switchTo(QStringLiteral("motion"));
        };
        m_steps << [this]() {
            screenshot(QStringLiteral("mode-motion"));
            check(QStringLiteral("motion mode active"), m_shell->modes()->currentModeId() == QLatin1String("motion"));
            check(QStringLiteral("Motion opens with the camera tool"), m_core->tools()->currentIntent() == ToolIntent::Camera);
            m_shell->modes()->switchTo(QStringLiteral("studio"));
        };
        m_steps << [this]() {
            QStringList docks;
            for (QDockWidget *dock : m_shell->window()->findChildren<QDockWidget *>()) {
                docks << QStringLiteral("%1=%2%3").arg(dock->objectName()).arg(dock->isVisible())
                             .arg(dock->isFloating() ? QStringLiteral("(floating)") : QString());
            }
            log(QStringLiteral("INFO  docks in Studio: %1").arg(docks.join(QStringLiteral(", "))));
            screenshot(QStringLiteral("mode-studio"));
            check(QStringLiteral("studio mode active"), m_shell->modes()->currentModeId() == QLatin1String("studio"));
            m_shell->modes()->switchTo(QStringLiteral("flip"));
        };
        m_steps << [this]() {
            check(QStringLiteral("same document after Flip > Motion > Studio > Flip"), editor()->object() == m_object);
            check(QStringLiteral("same canvas after the round trip"), canvas() == m_canvas);
            check(QStringLiteral("drawing unchanged after the round trip"), layerHash() == m_hash, QString::fromLatin1(m_hash));
            check(QStringLiteral("undo history kept"), m_core->history()->canUndo() == m_canUndo);
            check(QStringLiteral("Flip restores its last tool (brush)"), m_core->tools()->currentIntent() == ToolIntent::Brush);
        };

        // Frames
        m_steps << [this]() {
            m_frame1Pixels = paintedPixels();
            m_core->timeline()->addFrameAfter(editor()->layers()->currentLayer(), 1);
        };
        m_steps << [this]() {
            const QList<int> keys = m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer());
            check(QStringLiteral("new frame added after frame 1"), keys == QList<int>({1, 2}),
                  QStringLiteral("%1 keys").arg(keys.size()));
            check(QStringLiteral("playhead moved to the new frame"), m_core->timeline()->currentFrame() == 2,
                  QString::number(m_core->timeline()->currentFrame()));
            check(QStringLiteral("new frame starts empty"), paintedPixels() == 0, QString::number(paintedPixels()));
            penStroke(line(at(-200, 80), at(200, 80)));
        };
        m_steps << [this]() {
            m_frame2Pixels = paintedPixels();
            check(QStringLiteral("drawing on frame 2"), m_frame2Pixels > 200, QString::number(m_frame2Pixels));
            m_core->timeline()->setCurrentFrame(1);
        };
        m_steps << [this]() {
            check(QStringLiteral("frame 1 kept its drawing"), paintedPixels() == m_frame1Pixels,
                  QStringLiteral("%1 px").arg(paintedPixels()));
            m_core->timeline()->duplicateFrame(editor()->layers()->currentLayer(), 2);
        };
        m_steps << [this]() {
            const QList<int> keys = m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer());
            check(QStringLiteral("duplicate copies frame 2 into frame 3"),
                  keys == QList<int>({1, 2, 3}) && m_core->timeline()->currentFrame() == 3 && paintedPixels() == m_frame2Pixels,
                  QStringLiteral("%1 keys, frame %2, %3 px").arg(keys.size()).arg(m_core->timeline()->currentFrame()).arg(paintedPixels()));
            m_core->timeline()->removeFrame(editor()->layers()->currentLayer(), 3);
        };
        m_steps << [this]() {
            const QList<int> keys = m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer());
            check(QStringLiteral("delete frame removes it"), keys == QList<int>({1, 2}), QStringLiteral("%1 keys").arg(keys.size()));
            m_core->timeline()->setCurrentFrame(2);
            screenshot(QStringLiteral("frames"));
        };

        // Pen buttons: lower = undo, upper = redo (Windows Ink flags)
        m_steps << [this]() {
            StudioPenButtons *pen = m_core->penButtons();
            pen->resetToDefaults();
            m_penUndoText = m_core->history()->undoText();
            m_penKeys = m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer()).size();
            pen->injectInkFlags(StudioPenButtons::InkBarrelFlag);  // lower button down
        };
        m_steps << [this]() {
            StudioPenButtons *pen = m_core->penButtons();

            QTabletEvent rightPress = tabletEvent(QEvent::TabletPress, Qt::RightButton, Qt::RightButton);
            check(QStringLiteral("barrel right-press is swallowed"), pen->eventFilter(canvas(), &rightPress));
            QTabletEvent tipPress = tabletEvent(QEvent::TabletPress, Qt::LeftButton, Qt::LeftButton);
            check(QStringLiteral("tip press during the gesture starts no stroke"), pen->eventFilter(canvas(), &tipPress));
            QContextMenuEvent menu(QContextMenuEvent::Mouse, at(0, 0).toPoint());
            check(QStringLiteral("no context menu from the pen button"), pen->eventFilter(canvas(), &menu));

            const int keys = m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer()).size();
            check(QStringLiteral("lower button undid the last edit (the frame delete)"), keys == m_penKeys + 1,
                  QStringLiteral("%1 -> %2 keys, was '%3'").arg(m_penKeys).arg(keys).arg(m_penUndoText));

            // Contact bounce: release and press again within the debounce window.
            pen->injectInkFlags(0);
            pen->injectInkFlags(StudioPenButtons::InkBarrelFlag);
            pen->injectInkFlags(0);
        };
        m_steps << [this]() {
            const int keys = m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer()).size();
            check(QStringLiteral("bounce does not undo twice"), keys == m_penKeys + 1, QStringLiteral("%1 keys").arg(keys));

            StudioPenButtons *pen = m_core->penButtons();
            QTabletEvent hover = tabletEvent(QEvent::TabletMove, Qt::NoButton, Qt::NoButton);
            pen->eventFilter(canvas(), &hover);
            QTabletEvent tipPress = tabletEvent(QEvent::TabletPress, Qt::LeftButton, Qt::LeftButton);
            check(QStringLiteral("tip works again after the gesture"), !pen->eventFilter(canvas(), &tipPress));

            m_redoState = QStringLiteral("canRedo=%1 '%2'").arg(m_core->history()->canRedo()).arg(m_core->history()->redoText());
            pen->injectInkFlags(StudioPenButtons::InkInvertedFlag);  // upper button down
        };
        m_steps << [this]() {
            StudioPenButtons *pen = m_core->penButtons();
            QTabletEvent eraserMove = tabletEvent(QEvent::TabletMove, Qt::NoButton, Qt::NoButton,
                                                  QPointingDevice::PointerType::Eraser);
            check(QStringLiteral("eraser-flag events never reach the canvas"), pen->eventFilter(canvas(), &eraserMove));
            const int keys = m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer()).size();
            QString direct;
            if (keys != m_penKeys) {
                // Tell "the button did not fire" apart from "the engine cannot redo this".
                const QString afterState = QStringLiteral("canRedo=%1 '%2'").arg(m_core->history()->canRedo())
                                               .arg(m_core->history()->redoText());
                m_core->history()->redo();
                settle();
                direct = QStringLiteral("; after press: %1; direct redo -> %2 keys")
                             .arg(afterState)
                             .arg(m_core->timeline()->keyframeTimes(editor()->layers()->currentLayer()).size());
            }
            check(QStringLiteral("upper button redid it"), keys == m_penKeys,
                  QStringLiteral("%1 keys; before: %2%3").arg(keys).arg(m_redoState, direct));
            pen->injectInkFlags(0);
            pen->load();
        };

        // Beginner keys, mouse drawing, brush size
        m_steps << [this]() {
            check(QStringLiteral("beginner key map: Ctrl+Z undoes, B is the brush"),
                  BeginnerShortcuts::keysFor(QStringLiteral("CmdUndo")) == QKeySequence(QStringLiteral("Ctrl+Z")).toString(QKeySequence::NativeText)
                      && BeginnerShortcuts::keysFor(QStringLiteral("CmdToolBrush")) == QLatin1String("B"));
            check(QStringLiteral("beginner key map: the engine's extra keys are gone (R rotate, G grid)"),
                  BeginnerShortcuts::keysFor(QStringLiteral("CmdRotateClockwise")).isEmpty()
                      && BeginnerShortcuts::keysFor(QStringLiteral("CmdGrid")).isEmpty());

            m_shell->modes()->switchTo(QStringLiteral("studio"));
        };
        m_steps << [this]() {
            const ModeDescriptor *studio = m_shell->modes()->currentMode();
            QStringList failed;
            for (ToolIntent intent : studio->tools) {
                const ToolIntentInfo info = StudioTools::info(intent);
                m_core->tools()->activate(intent);
                settle(2);
                if (!editor()->tools()->currentTool() || editor()->tools()->currentTool()->type() != info.tool) failed << info.key;
            }
            check(QStringLiteral("all %1 Studio tools activate").arg(studio->tools.size()), failed.isEmpty(),
                  failed.join(QLatin1Char(',')));
            m_core->tools()->activate(ToolIntent::Brush);
            m_mouseBefore = paintedPixels();
        };
        m_steps << [this]() { mouseStroke(line(at(-200, 150), at(200, 220))); };
        m_steps << [this]() {
            check(QStringLiteral("brush draws with a plain mouse too"), paintedPixels() > m_mouseBefore + 200,
                  QStringLiteral("%1 -> %2 px").arg(m_mouseBefore).arg(paintedPixels()));
            m_core->tools()->setBrushSize(42);
            m_core->tools()->setBrushOpacity(0.5);
        };
        m_steps << [this]() {
            check(QStringLiteral("size slider value reaches the brush"), qAbs(m_core->tools()->brushSize() - 42) < 0.5,
                  QString::number(m_core->tools()->brushSize()));
            check(QStringLiteral("opacity slider value reaches the colour"), qAbs(m_core->tools()->brushOpacity() - 0.5) < 0.01,
                  QString::number(m_core->tools()->brushOpacity()));
            m_core->tools()->setBrushOpacity(1.0);
            screenshot(QStringLiteral("studio-final"));
            m_shell->modes()->switchTo(QStringLiteral("flip"));
        };
        m_steps << [this]() { screenshot(QStringLiteral("final")); };
    }

private:
    ArtflowShell *m_shell;
    StudioCore *m_core;
    QDir m_out;
    QStringList m_report;
    int m_failures {0};
    QVector<std::function<void()>> m_steps;
    int m_step {0};

    Object *m_object {nullptr};
    QPointer<ScribbleArea> m_canvas;
    QByteArray m_hash;
    bool m_canUndo {false};
    int m_before {0};
    int m_afterBrush {0};
    int m_afterEraser {0};
    int m_frame1Pixels {0};
    int m_frame2Pixels {0};
    int m_penKeys {0};
    int m_mouseBefore {0};
    QString m_penUndoText;
    QString m_redoState;
};

}

void ArtflowSelfTest::run(ArtflowShell *shell, const QString &outputDir)
{
    (new SelfTest(shell, outputDir))->start();
}

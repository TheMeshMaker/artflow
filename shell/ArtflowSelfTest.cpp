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
#include <QElapsedTimer>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QMouseEvent>
#include <QPointer>
#include <QPointingDevice>
#include <QProcess>
#include <QScrollArea>
#include <QSettings>
#include <QToolButton>
#include <QtMath>
#include <QTabletEvent>
#include <QTextStream>
#include <QTimer>
#include <QVector>

#include "bitmapimage.h"
#include "colormanager.h"
#include "editor.h"
#include "layer.h"
#include "layerbitmap.h"
#include "layercamera.h"
#include "movieexporter.h"
#include "util.h"
#include "layermanager.h"
#include "mainwindow2.h"
#include "object.h"
#include "scribblearea.h"
#include "selectionmanager.h"
#include "toolmanager.h"
#include "undoredomanager.h"
#include "viewmanager.h"

#include "StudioAdjust.h"
#include "StudioBrushes.h"
#include "StudioCore.h"
#include "StudioHistory.h"
#include "StudioLayers.h"
#include "StudioPenButtons.h"
#include "StudioTimeline.h"
#include "StudioTools.h"

#include "ArtflowShell.h"
#include "BeginnerShortcuts.h"
#include "ModeManager.h"
#include "panels/AdjustPanel.h"
#include "panels/HomeScreen.h"
#include "panels/LookDialog.h"
#include "ArtflowTheme.h"
#include "TouchController.h"
#include "panels/BrushLibrary.h"
#include "panels/LayerStackPanel.h"
#include "tools/ShapeTool.h"
#include "tools/TextTool.h"

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
        // The test picks brush styles and sizes; the person's own come back at the end.
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.beginGroup(QStringLiteral("ArtFlowBrushes"));
        for (const QString &key : settings.allKeys()) m_brushSettings.insert(key, settings.value(key));
        settings.endGroup();
        settings.beginGroup(QStringLiteral("ArtFlowLook"));
        for (const QString &key : settings.allKeys()) m_lookSettings.insert(key, settings.value(key));
        settings.endGroup();

        // Judge the layout the way people see it: maximized.
        m_shell->window()->showMaximized();
        waitForStableWindow(0, QSize());
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

    void checkToolOptionsFit(const QString &name)
    {
        QDockWidget *options = m_shell->window()->findChild<QDockWidget *>(QStringLiteral("ToolOption"));
        QScrollArea *scroll = options ? options->findChild<QScrollArea *>(QStringLiteral("scrollArea")) : nullptr;
        const int needed = scroll && scroll->widget() ? scroll->widget()->sizeHint().height() : -1;
        const int shown = scroll ? scroll->viewport()->height() : -1;
        check(name, scroll && shown >= needed,
              QStringLiteral("needs %1 px, shows %2 px, section %3 px").arg(needed).arg(shown).arg(options ? options->height() : -1));
    }

    void settle(int rounds = 5)
    {
        for (int i = 0; i < rounds; ++i) QApplication::processEvents(QEventLoop::AllEvents, 40);
    }

    // Starts the steps once the maximized window has stopped changing size:
    // strokes drawn while the view still re-centres land in different places.
    void waitForStableWindow(int tries, const QSize &last)
    {
        const QSize size = m_shell->window()->size();
        if (tries >= 30 || (tries >= 5 && size == last && m_shell->window()->isMaximized())) {
            log(QStringLiteral("INFO  window %1 x %2 after %3 ms").arg(size.width()).arg(size.height()).arg(tries * 200));
            runNext();
            return;
        }
        QTimer::singleShot(200, this, [this, tries, size]() { waitForStableWindow(tries + 1, size); });
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
        QSettings settings(PENCIL2D, PENCIL2D);
        settings.remove(QStringLiteral("ArtFlowBrushes"));
        settings.beginGroup(QStringLiteral("ArtFlowBrushes"));
        for (auto it = m_brushSettings.constBegin(); it != m_brushSettings.constEnd(); ++it) settings.setValue(it.key(), it.value());
        settings.endGroup();
        settings.remove(QStringLiteral("ArtFlowLook"));
        settings.beginGroup(QStringLiteral("ArtFlowLook"));
        for (auto it = m_lookSettings.constBegin(); it != m_lookSettings.constEnd(); ++it) settings.setValue(it.key(), it.value());
        settings.endGroup();

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

    /// A pen stroke whose pressure changes along the way. Like a real pen
    /// it hovers over the start first (after a side-button gesture the tip
    /// only draws again once the pen has hovered).
    void pressureStroke(const QVector<QPointF> &points, const QVector<qreal> &pressures)
    {
        sendTablet(QEvent::TabletMove, points.first(), 0.0, Qt::NoButton, Qt::NoButton);
        sendTablet(QEvent::TabletPress, points.first(), pressures.first(), Qt::LeftButton, Qt::LeftButton);
        for (int i = 1; i < points.size(); ++i) {
            sendTablet(QEvent::TabletMove, points[i], pressures[i], Qt::NoButton, Qt::LeftButton);
            QApplication::processEvents();
        }
        sendTablet(QEvent::TabletRelease, points.last(), 0.0, Qt::LeftButton, Qt::NoButton);
        settle();
    }

    /// Back and forth over the same place, \p passes times.
    QVector<QPointF> zigzag(const QPointF &from, const QPointF &to, int passes) const
    {
        QVector<QPointF> points;
        for (int pass = 0; pass < passes; ++pass) {
            const QVector<QPointF> leg = pass % 2 ? line(to, from) : line(from, to);
            points << (pass ? leg.mid(1) : leg);
        }
        return points;
    }

    /// Goes to a new, empty drawing after the current one.
    void freshFrame()
    {
        Layer *layer = editor()->layers()->currentLayer();
        const int current = editor()->currentFrame();
        m_core->timeline()->addFrameAfter(layer, current);
        int next = -1;
        for (int key : m_core->timeline()->keyframeTimes(layer)) {
            if (key > current && (next < 0 || key < next)) next = key;
        }
        if (next > 0) m_core->timeline()->setCurrentFrame(next);
    }

    void useStyle(const QString &id)
    {
        const BrushPreset preset = m_core->brushes()->preset(id);
        m_core->tools()->activate(preset.tool == PENCIL ? ToolIntent::Pencil : ToolIntent::Brush);
        m_core->brushes()->select(id);
    }

    /// Alpha statistics of the drawing on the current frame.
    struct AlphaStats {
        int solid = 0;   ///< alpha 240 and up
        int edge = 0;    ///< alpha 16 to 239
        int highest = 0;
    };

    AlphaStats alphaStats() const
    {
        AlphaStats stats;
        const QImage image = frameImage().convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y) {
            const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const int alpha = qAlpha(row[x]);
                if (alpha >= 240) ++stats.solid;
                else if (alpha >= 16) ++stats.edge;
                stats.highest = qMax(stats.highest, alpha);
            }
        }
        return stats;
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

    /// The pixel of the current drawing under a point of the canvas widget.
    QRgb pixelAt(const QPointF &screenPoint) const
    {
        Layer *layer = editor()->layers()->currentLayer();
        if (!layer || layer->type() != Layer::BITMAP) return 0;
        BitmapImage *bitmap = static_cast<LayerBitmap *>(layer)->getLastBitmapImageAtFrame(editor()->currentFrame());
        if (!bitmap || !bitmap->image()) return 0;
        const QPoint p = editor()->view()->mapScreenToCanvas(screenPoint).toPoint() - bitmap->topLeft();
        const QImage image = bitmap->image()->convertToFormat(QImage::Format_ARGB32);
        return image.rect().contains(p) ? image.pixel(p) : 0;
    }

    /// A press, a drag and a release with the pen, for click-and-drag tools.
    void penDrag(const QPointF &from, const QPointF &to, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        auto send = [this, modifiers](QEvent::Type type, const QPointF &pos, qreal pressure, Qt::MouseButton button,
                                      Qt::MouseButtons buttons) {
            QTabletEvent event(type, testDevice(QPointingDevice::PointerType::Pen), pos, canvas()->mapToGlobal(pos), pressure,
                               0.f, 0.f, 0.f, 0.0, 0.f, modifiers, button, buttons);
            QApplication::sendEvent(canvas(), &event);
        };
        send(QEvent::TabletMove, from, 0.0, Qt::NoButton, Qt::NoButton);
        send(QEvent::TabletPress, from, 0.8, Qt::LeftButton, Qt::LeftButton);
        for (const QPointF &point : line(from, to, 16).mid(1)) {
            send(QEvent::TabletMove, point, 0.8, Qt::NoButton, Qt::LeftButton);
            QApplication::processEvents();
        }
        send(QEvent::TabletRelease, to, 0.0, Qt::LeftButton, Qt::NoButton);
        settle();
    }

    static QPointingDevice *touchScreen()
    {
        static QPointingDevice *device = new QPointingDevice(QStringLiteral("ArtFlow test touch screen"), 4242,
                                                             QInputDevice::DeviceType::TouchScreen,
                                                             QPointingDevice::PointerType::Finger,
                                                             QInputDevice::Capability::Position, 10, 0);
        return device;
    }

    /// Fingers on the canvas: each list holds the fingers down at that moment.
    void touch(QEvent::Type type, const QList<QPointF> &fingers, QEventPoint::State state)
    {
        QList<QEventPoint> points;
        for (int i = 0; i < fingers.size(); ++i) {
            points << QEventPoint(i + 1, state, fingers[i], canvas()->mapToGlobal(fingers[i]));
        }
        QTouchEvent event(type, touchScreen(), Qt::NoModifier, points);
        // Sent like a real one, so Qt fills in the positions on the canvas.
        QApplication::sendEvent(canvas(), &event);
        QApplication::processEvents();
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
            const QString title = m_shell->window()->windowTitle();
            check(QStringLiteral("window title names ArtFlow, not the engine"),
                  title.endsWith(QLatin1String(" - ArtFlow")) && !title.contains(QLatin1String("Pencil2D"))
                      && !title.startsWith(QLatin1String("[*] -")),
                  title);
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
            checkToolOptionsFit(QStringLiteral("tool options fit without scrolling"));
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
            QContextMenuEvent menu(QContextMenuEvent::Mouse, at(0, 0).toPoint(), canvas()->mapToGlobal(at(0, 0).toPoint()));
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
            checkToolOptionsFit(QStringLiteral("tool options fit again after trying every tool"));
            screenshot(QStringLiteral("studio-final"));
        };

        // Brush styles
        m_steps << [this]() { freshFrame(); };
        m_steps << [this]() {
            const std::vector<LayerCamera *> cameras = editor()->object()->getLayersByType<LayerCamera>();
            const QRect frame = cameras.empty() ? QRect() : cameras.front()->getViewRect();
            check(QStringLiteral("new drawings are 1920 x 1080"), frame.size() == QSize(1920, 1080),
                  QStringLiteral("%1 x %2").arg(frame.width()).arg(frame.height()));
            const QRectF page = editor()->view()->getView().mapRect(QRectF(frame));
            const QRectF area = QRectF(canvas()->rect());
            check(QStringLiteral("the whole page fits the window"),
                  area.contains(page) && (page.width() > 0.8 * area.width() || page.height() > 0.8 * area.height()),
                  QStringLiteral("page %1 x %2 in a %3 x %4 canvas").arg(qRound(page.width())).arg(qRound(page.height()))
                      .arg(area.width()).arg(area.height()));
            editor()->color()->setFrontColor(QColor(Qt::black));
            m_sampler = 0;
            m_samplerFailures.clear();
        };
        for (const BrushPreset &preset : StudioBrushes::library()) {
            m_steps << [this, preset]() {
                useStyle(preset.id);
                const int index = m_sampler++;
                const QPointF centre = at(-600 + (index % 4) * 400, -330 + (index / 4) * 220);
                QVector<QPointF> points;
                QVector<qreal> pressures;
                for (int i = 0; i <= 40; ++i) {
                    const qreal t = i / 40.0;
                    points << centre + QPointF(-140 + 280 * t, -qSin(t * 2 * M_PI) * 55);
                    pressures << 0.25 + 0.75 * qSin(t * M_PI);
                }
                const int before = paintedPixels();
                pressureStroke(points, pressures);
                const int added = paintedPixels() - before;
                if (added < 100) m_samplerFailures << QStringLiteral("%1 (%2 px)").arg(preset.id).arg(added);
            };
        }
        m_steps << [this]() {
            check(QStringLiteral("all %1 brush styles paint").arg(StudioBrushes::library().size()),
                  m_samplerFailures.isEmpty(), m_samplerFailures.join(QStringLiteral(", ")));
            screenshot(QStringLiteral("brush-sampler"));
            frameImage().save(m_out.absoluteFilePath(QStringLiteral("brush-sampler-pixels.png")));
            freshFrame();
        };
        m_steps << [this]() {
            // A thin line, where stair-stepped edges would show the most.
            useStyle(QStringLiteral("ink-pen"));
            m_core->tools()->setBrushSize(6);
            const QVector<QPointF> points = line(at(-300, -100), at(300, 60), 40);
            pressureStroke(points, QVector<qreal>(points.size(), 0.8));
        };
        m_steps << [this]() {
            const AlphaStats stats = alphaStats();
            check(QStringLiteral("ink lines have smooth edges, not stairs"), stats.edge > 400 && stats.edge > stats.solid / 3,
                  QStringLiteral("%1 edge px, %2 solid px").arg(stats.edge).arg(stats.solid));
            freshFrame();
        };
        m_steps << [this]() {
            useStyle(QStringLiteral("marker"));
            const QVector<QPointF> points = zigzag(at(-200, 0), at(200, 6), 6);
            pressureStroke(points, QVector<qreal>(points.size(), 1.0));
        };
        m_steps << [this]() {
            const AlphaStats stats = alphaStats();
            check(QStringLiteral("a marker stroke does not get darker where it crosses itself"),
                  stats.highest >= 120 && stats.highest <= 150, QStringLiteral("highest alpha %1").arg(stats.highest));
            freshFrame();
        };
        m_steps << [this]() {
            useStyle(QStringLiteral("airbrush"));
            const QVector<QPointF> points = zigzag(at(-150, 0), at(150, 0), 8);
            pressureStroke(points, QVector<qreal>(points.size(), 1.0));
        };
        m_steps << [this]() {
            const AlphaStats stats = alphaStats();
            check(QStringLiteral("the airbrush builds up where it passes again"), stats.highest > 60,
                  QStringLiteral("highest alpha %1").arg(stats.highest));

            QStringList blank;
            for (const BrushPreset &preset : StudioBrushes::library()) {
                const QImage sample = StudioBrushes::preview(preset, QSize(180, 48), 1.0, Qt::white);
                int inked = 0;
                for (int y = 0; y < sample.height(); ++y) {
                    for (int x = 0; x < sample.width(); ++x) {
                        if (qAlpha(sample.pixel(x, y)) > 16) ++inked;
                    }
                }
                if (inked < 40) blank << preset.id;
            }
            check(QStringLiteral("every style has a sample stroke in the picker"), blank.isEmpty(), blank.join(QLatin1Char(',')));

            m_core->tools()->activate(ToolIntent::Brush);
            if (QToolButton *style = m_shell->window()->findChild<QToolButton *>(QStringLiteral("brushStyle"))) style->click();
        };
        m_steps << [this]() {
            BrushLibrary *library = m_shell->window()->findChild<BrushLibrary *>();
            const int cards = library ? int(library->findChildren<BrushCard *>().size()) : 0;
            check(QStringLiteral("the brush style button opens the brush picker"), library && library->isVisible() && cards == 11,
                  QStringLiteral("%1 styles").arg(cards));
            if (library) {
                library->grab().save(m_out.absoluteFilePath(QStringLiteral("brush-library.png")));
                library->close();
            }
            m_core->tools()->activate(ToolIntent::Pencil);
            if (QToolButton *style = m_shell->window()->findChild<QToolButton *>(QStringLiteral("brushStyle"))) style->click();
        };
        m_steps << [this]() {
            BrushLibrary *library = m_shell->window()->findChild<BrushLibrary *>();
            const int cards = library ? int(library->findChildren<BrushCard *>().size()) : 0;
            check(QStringLiteral("the Pencil has its own styles"), library && library->isVisible() && cards == 5,
                  QStringLiteral("%1 styles").arg(cards));
            if (library) {
                library->grab().save(m_out.absoluteFilePath(QStringLiteral("pencil-library.png")));
                library->close();
            }
            m_core->tools()->activate(ToolIntent::Brush);
            freshFrame();
        };

        // Low-opacity black must look grey, while drawing and after.
        m_steps << [this]() {
            useStyle(QStringLiteral("ink-pen"));
            m_core->tools()->setBrushSize(24);
            editor()->color()->setFrontColor(QColor(Qt::black));
            settle(2);
            m_core->tools()->setBrushOpacity(0.3);
            settle(2);
            const QColor front = editor()->color()->frontColor();
            log(QStringLiteral("INFO  front colour after opacity 30%: rgba(%1,%2,%3,%4)").arg(front.red()).arg(front.green())
                    .arg(front.blue()).arg(front.alpha()));
            const QVector<QPointF> points = line(at(-300, 40), at(300, 40), 30);
            sendTablet(QEvent::TabletMove, points.first(), 0.0, Qt::NoButton, Qt::NoButton);
            sendTablet(QEvent::TabletPress, points.first(), 0.8, Qt::LeftButton, Qt::LeftButton);
            for (int i = 1; i < points.size(); ++i) {
                sendTablet(QEvent::TabletMove, points[i], 0.8, Qt::NoButton, Qt::LeftButton);
                QApplication::processEvents();
            }
            settle(2);
            auto sample = [this](const QPointF &p) {
                const QImage shot = canvas()->grab().toImage();
                return QColor(shot.pixel((p * canvas()->devicePixelRatioF()).toPoint()));
            };
            canvas()->grab().save(m_out.absoluteFilePath(QStringLiteral("opacity-during.png")));
            m_shell->window()->grab().save(m_out.absoluteFilePath(QStringLiteral("opacity-during-window.png")));
            const QColor during = sample(at(-100, 40));
            sendTablet(QEvent::TabletRelease, points.last(), 0.0, Qt::LeftButton, Qt::NoButton);
            settle();
            canvas()->grab().save(m_out.absoluteFilePath(QStringLiteral("opacity-after.png")));
            const QColor after = sample(at(-100, 40));
            auto neutral = [](const QColor &c) {
                return qAbs(c.red() - c.green()) < 10 && qAbs(c.green() - c.blue()) < 10 && c.value() < 235;
            };
            check(QStringLiteral("30% black is grey while drawing and after"), neutral(during) && neutral(after),
                  QStringLiteral("while drawing rgb(%1,%2,%3), after rgb(%4,%5,%6)").arg(during.red()).arg(during.green())
                      .arg(during.blue()).arg(after.red()).arg(after.green()).arg(after.blue()));
            m_core->tools()->setBrushOpacity(1.0);
        };

        // Shapes, lasso fill, text, symmetry
        m_steps << [this]() {
            editor()->color()->setFrontColor(QColor(40, 90, 200));
            useStyle(QStringLiteral("ink-pen"));
            m_core->tools()->setBrushSize(6);
            m_core->tools()->activate(ToolIntent::Shapes);
            m_core->shapeTool()->setShape(ShapeTool::Rectangle);
            m_core->shapeTool()->setFilled(true);
            m_before = paintedPixels();
            penDrag(at(-520, -300), at(-300, -120));
            m_shell->window()->grab(QRect(0, 0, m_shell->window()->width(), 60))
                .save(m_out.absoluteFilePath(QStringLiteral("shapes-bar.png")));
        };
        m_steps << [this]() {
            check(QStringLiteral("Shapes draws a filled rectangle"),
                  paintedPixels() > m_before + 2000 && qAlpha(pixelAt(at(-410, -210))) > 200,
                  QStringLiteral("%1 -> %2 px").arg(m_before).arg(paintedPixels()));
            check(QStringLiteral("a shape is one undo step"), m_core->history()->canUndo());
            m_core->shapeTool()->setShape(ShapeTool::Ellipse);
            m_core->shapeTool()->setFilled(false);
            m_before = paintedPixels();
            penDrag(at(-220, -300), at(0, -120));
        };
        m_steps << [this]() {
            check(QStringLiteral("an unfilled ellipse is only its outline"),
                  paintedPixels() > m_before + 300 && qAlpha(pixelAt(at(-110, -210))) == 0,
                  QStringLiteral("%1 -> %2 px, centre alpha %3").arg(m_before).arg(paintedPixels()).arg(qAlpha(pixelAt(at(-110, -210)))));
            m_core->shapeTool()->setShape(ShapeTool::Line);
            m_before = paintedPixels();
            penDrag(at(60, -300), at(300, -150), Qt::ShiftModifier);
        };
        m_steps << [this]() {
            check(QStringLiteral("Shift keeps a line at 45 degrees"), paintedPixels() > m_before + 200
                      && qAlpha(pixelAt(at(60, -300) + QPointF(100, 100))) > 100,
                  QStringLiteral("%1 -> %2 px").arg(m_before).arg(paintedPixels()));

            m_core->tools()->activate(ToolIntent::LassoFill);
            editor()->color()->setFrontColor(QColor(230, 120, 60));
            m_before = paintedPixels();
            QVector<QPointF> loop;
            for (int i = 0; i <= 40; ++i) {
                const qreal t = i / 40.0 * 2 * M_PI;
                loop << at(450 + 120 * qCos(t), -210 + 80 * qSin(t));
            }
            pressureStroke(loop, QVector<qreal>(loop.size(), 0.8));
        };
        m_steps << [this]() {
            check(QStringLiteral("Lasso Fill fills the area drawn around"),
                  paintedPixels() > m_before + 5000 && qAlpha(pixelAt(at(450, -210))) > 200,
                  QStringLiteral("%1 -> %2 px").arg(m_before).arg(paintedPixels()));

            m_core->tools()->activate(ToolIntent::Text);
            editor()->color()->setFrontColor(QColor(20, 20, 30));
            m_core->textTool()->setSize(64);
            m_before = paintedPixels();
            m_core->textTool()->placeText(editor()->view()->mapScreenToCanvas(at(-520, -40)), QStringLiteral("Hello ArtFlow"));
        };
        m_steps << [this]() {
            check(QStringLiteral("Text writes into the drawing"), paintedPixels() > m_before + 800,
                  QStringLiteral("%1 -> %2 px").arg(m_before).arg(paintedPixels()));
            const int withText = paintedPixels();
            m_core->history()->undo();
            settle();
            check(QStringLiteral("undo takes the text back"), paintedPixels() < withText - 500,
                  QStringLiteral("%1 -> %2 px").arg(withText).arg(paintedPixels()));
            m_core->history()->redo();
            settle();

            // Symmetry: a stroke on the left of the page shows up on the right.
            m_core->tools()->activate(ToolIntent::Brush);
            editor()->color()->setFrontColor(QColor(120, 40, 160));
            m_core->tools()->setBrushSize(14);
            if (QToolButton *mirror = m_shell->window()->findChild<QToolButton *>(QStringLiteral("symmetryButton"))) {
                mirror->setChecked(true);
            }
            const QPointF axis = editor()->view()->mapCanvasToScreen(QPointF(m_core->brushes()->symmetryAxis(), 0));
            m_mirrorLeft = QPointF(axis.x() - 380, at(0, 120).y());
            pressureStroke(line(m_mirrorLeft, m_mirrorLeft + QPointF(-120, 80), 20), QVector<qreal>(21, 0.8));
            m_mirrorRight = QPointF(2 * axis.x() - m_mirrorLeft.x(), m_mirrorLeft.y());
        };
        m_steps << [this]() {
            check(QStringLiteral("symmetry mirrors the stroke across the page"),
                  qAlpha(pixelAt(m_mirrorLeft)) > 150 && qAlpha(pixelAt(m_mirrorRight)) > 150,
                  QStringLiteral("left alpha %1, mirrored alpha %2").arg(qAlpha(pixelAt(m_mirrorLeft))).arg(qAlpha(pixelAt(m_mirrorRight))));
            if (QToolButton *mirror = m_shell->window()->findChild<QToolButton *>(QStringLiteral("symmetryButton"))) {
                mirror->setChecked(false);
            }
            screenshot(QStringLiteral("new-tools"));
            frameImage().save(m_out.absoluteFilePath(QStringLiteral("new-tools-pixels.png")));

            // Adjust: turn the blue rectangle around the colour wheel.
            m_adjustBefore = pixelAt(at(-410, -210));
            check(QStringLiteral("Adjust starts on a drawing"), m_core->adjust()->begin());
            {
                AdjustPanel panel(m_core, false, m_shell->window());
                Adjustments shown;
                shown.hue = 40;
                shown.saturation = -20;
                shown.blur = 6;
                panel.setAdjustments(shown);
                panel.adjustSize();
                panel.grab().save(m_out.absoluteFilePath(QStringLiteral("adjust-panel.png")));
            }
            Adjustments shift;
            shift.hue = 180;
            m_core->adjust()->preview(shift);
            m_core->adjust()->apply();
            settle();
        };
        m_steps << [this]() {
            const QRgb after = pixelAt(at(-410, -210));
            check(QStringLiteral("Adjust turns the colours"), qBlue(m_adjustBefore) > qRed(m_adjustBefore) && qRed(after) > qBlue(after),
                  QStringLiteral("rgb(%1,%2,%3) -> rgb(%4,%5,%6)").arg(qRed(m_adjustBefore)).arg(qGreen(m_adjustBefore))
                      .arg(qBlue(m_adjustBefore)).arg(qRed(after)).arg(qGreen(after)).arg(qBlue(after)));
            m_core->history()->undo();
            settle();
            const QRgb undone = pixelAt(at(-410, -210));
            check(QStringLiteral("undo takes the adjustment back"), qBlue(undone) > qRed(undone),
                  QStringLiteral("rgb(%1,%2,%3)").arg(qRed(undone)).arg(qGreen(undone)).arg(qBlue(undone)));

            Adjustments blur;
            blur.blur = 12;
            const QImage source = frameImage();
            const QImage blurred = StudioAdjust::adjusted(source, blur);
            int soft = 0;
            int softBefore = 0;
            for (int y = 0; y < blurred.height(); y += 2) {
                for (int x = 0; x < blurred.width(); x += 2) {
                    const int a = qAlpha(blurred.pixel(x, y));
                    const int b = qAlpha(source.pixel(x, y));
                    if (a > 10 && a < 230) ++soft;
                    if (b > 10 && b < 230) ++softBefore;
                }
            }
            check(QStringLiteral("blur softens the edges"), soft > softBefore * 2, QStringLiteral("%1 -> %2 soft px").arg(softBefore).arg(soft));
        };

        // Layers
        m_steps << [this]() {
            m_layersBefore = editor()->layers()->count();
            m_core->layers()->addDrawingLayer();
            settle();
            Layer *added = m_core->layers()->activeLayer();
            m_core->layers()->rename(added, QStringLiteral("Ink"));
            m_core->layers()->setOpacity(added, 0.5);
            settle();
            check(QStringLiteral("a new layer can be renamed and faded"),
                  added && added->name() == QLatin1String("Ink") && qAbs(added->opacity() - 0.5) < 0.01
                      && editor()->layers()->count() == m_layersBefore + 1,
                  added ? added->name() : QStringLiteral("no layer"));
            const QVector<Layer *> stack = m_core->layers()->layers();
            m_core->layers()->moveTo(added, stack.size() - 1);
            settle();
            const QVector<Layer *> moved = m_core->layers()->layers();
            check(QStringLiteral("a layer moves down to just above the camera"),
                  moved.size() >= 2 && moved[moved.size() - 2] == added && moved.last()->type() == Layer::CAMERA,
                  QStringLiteral("second from the bottom: %1").arg(moved.size() >= 2 ? moved[moved.size() - 2]->name() : QString()));
            m_core->layers()->moveTo(added, 0);
            settle();
            check(QStringLiteral("and back to the top"), m_core->layers()->layers().first() == added);
            Layer *copy = m_core->layers()->duplicateActiveLayer();
            settle();
            check(QStringLiteral("duplicating keeps name and opacity"),
                  copy && copy != added && copy->name().contains(QLatin1String("Ink")) && qAbs(copy->opacity() - 0.5) < 0.01
                      && editor()->layers()->count() == m_layersBefore + 2,
                  copy ? copy->name() : QStringLiteral("no copy"));
            if (LayerStackPanel *panel = m_shell->window()->findChild<LayerStackPanel *>()) {
                panel->grab().save(m_out.absoluteFilePath(QStringLiteral("layers-panel.png")));
            }
            screenshot(QStringLiteral("layers"));
            m_shell->modes()->switchTo(QStringLiteral("flip"));
        };
        m_steps << [this]() {
            QDockWidget *layers = m_shell->window()->findChild<QDockWidget *>(QStringLiteral("ArtFlowLayersDock"));
            check(QStringLiteral("Flip mode shows the layers too"), layers && layers->isVisible());
            screenshot(QStringLiteral("final"));
        };

        // Drawing stays on the page
        m_steps << [this]() {
            m_shell->modes()->switchTo(QStringLiteral("studio"));
            m_core->tools()->activate(ToolIntent::Brush);
            freshFrame();
        };
        m_steps << [this]() {
            useStyle(QStringLiteral("round-brush"));
            m_core->tools()->setBrushSize(30);
            editor()->color()->setFrontColor(QColor(Qt::black));
            const QRect page = m_core->pageRect();
            const QPointF outside = editor()->view()->mapCanvasToScreen(QPointF(page.left() - 150, page.center().y()));
            const QPointF inside = editor()->view()->mapCanvasToScreen(QPointF(page.left() + 200, page.center().y()));
            const QVector<QPointF> points = line(outside, inside, 30);
            pressureStroke(points, QVector<qreal>(points.size(), 0.9));
        };
        m_steps << [this]() {
            Layer *layer = editor()->layers()->currentLayer();
            BitmapImage *bitmap = static_cast<LayerBitmap *>(layer)->getLastBitmapImageAtFrame(editor()->currentFrame());
            const QRect page = m_core->pageRect();
            int off = 0;
            int on = 0;
            if (bitmap && bitmap->image()) {
                const QImage image = bitmap->image()->convertToFormat(QImage::Format_ARGB32);
                const QPoint origin = bitmap->topLeft();
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        if (qAlpha(image.pixel(x, y)) < 8) continue;
                        if (page.contains(origin + QPoint(x, y))) ++on;
                        else ++off;
                    }
                }
            }
            check(QStringLiteral("a stroke started off the page leaves nothing outside it"), on > 500 && off == 0,
                  QStringLiteral("%1 px on the page, %2 off it").arg(on).arg(off));

            // Touch: one finger draws, two fingers pinch, a two-finger tap undoes.
            // Fingers right after the pen are a resting palm, so wait first.
            QElapsedTimer pause;
            pause.start();
            while (pause.elapsed() < 1700) QApplication::processEvents(QEventLoop::AllEvents, 50);
            m_before = paintedPixels();
            TouchController::setFingersDraw(true);
            touch(QEvent::TouchBegin, {at(-200, 150)}, QEventPoint::Pressed);
            for (int i = 1; i <= 12; ++i) touch(QEvent::TouchUpdate, {at(-200 + i * 25, 150 + i * 4)}, QEventPoint::Updated);
            touch(QEvent::TouchEnd, {at(100, 198)}, QEventPoint::Released);
            settle();
        };
        m_steps << [this]() {
            check(QStringLiteral("one finger draws"), paintedPixels() > m_before + 300,
                  QStringLiteral("%1 -> %2 px").arg(m_before).arg(paintedPixels()));
            m_scaleBefore = editor()->view()->scaling();
            touch(QEvent::TouchBegin, {at(-100, 0), at(100, 0)}, QEventPoint::Pressed);
            for (int i = 1; i <= 10; ++i) {
                touch(QEvent::TouchUpdate, {at(-100 - i * 10, 0), at(100 + i * 10, 0)}, QEventPoint::Updated);
            }
            touch(QEvent::TouchEnd, {at(-200, 0), at(200, 0)}, QEventPoint::Released);
            settle();
            const qreal ratio = editor()->view()->scaling() / m_scaleBefore;
            check(QStringLiteral("two fingers pinch to zoom"), ratio > 1.8 && ratio < 2.2, QStringLiteral("zoom x%1").arg(ratio, 0, 'f', 2));
            m_before = paintedPixels();
            touch(QEvent::TouchBegin, {at(-50, 0), at(50, 0)}, QEventPoint::Pressed);
            touch(QEvent::TouchEnd, {at(-50, 0), at(50, 0)}, QEventPoint::Released);
            settle();
        };
        m_steps << [this]() {
            check(QStringLiteral("a two-finger tap undoes"), paintedPixels() < m_before - 300,
                  QStringLiteral("%1 -> %2 px").arg(m_before).arg(paintedPixels()));
            m_core->history()->redo();
            QToolButton *fit = nullptr;
            for (QToolButton *button : canvas()->findChildren<QToolButton *>()) {
                if (button->toolTip().startsWith(QLatin1String("Fit the page"))) fit = button;
            }
            if (fit) fit->click();
            settle();
            const QRectF page = editor()->view()->getView().mapRect(QRectF(m_core->pageRect()));
            check(QStringLiteral("the fit button shows the whole page again"),
                  fit && QRectF(canvas()->rect()).contains(page) && page.width() > 0.8 * canvas()->width(),
                  QStringLiteral("page %1 x %2").arg(qRound(page.width())).arg(qRound(page.height())));
            screenshot(QStringLiteral("view-controls"));

            // A picture becomes a new layer, fitted to the page.
            QImage picture(2400, 900, QImage::Format_ARGB32);
            picture.fill(QColor(30, 160, 120));
            const QString path = m_out.absoluteFilePath(QStringLiteral("import-test.png"));
            picture.save(path);
            m_layersBefore = editor()->layers()->count();
            Layer *imported = m_core->layers()->importImage(path);
            settle();
            const QImage layerImage = frameImage();
            check(QStringLiteral("a picture imports as a new layer, fitted to the page"),
                  imported && editor()->layers()->count() == m_layersBefore + 1 && imported->name().contains(QLatin1String("import-test"))
                      && layerImage.width() <= m_core->pageRect().width() && layerImage.width() > 1000,
                  QStringLiteral("%1 x %2 px on \"%3\"").arg(layerImage.width()).arg(layerImage.height())
                      .arg(imported ? imported->name() : QString()));
            screenshot(QStringLiteral("imported"));
        };

        // Looks, placement and the start screen
        m_steps << [this]() {
            ArtflowTheme::setLook(QStringLiteral("paper"), QStringLiteral("teal"));
            ArtflowTheme::apply(*qApp);
            for (QWidget *widget : QApplication::allWidgets()) widget->update();
            settle();
            const QColor bar = m_shell->window()->grab(QRect(0, 0, 40, 40)).toImage().pixelColor(30, 30);
            check(QStringLiteral("the Paper style turns the interface light"), bar.lightness() > 180,
                  QStringLiteral("top bar %1").arg(bar.name()));
            screenshot(QStringLiteral("look-paper"));
            {
                LookDialog dialog(m_shell->window());
                dialog.adjustSize();
                dialog.grab().save(m_out.absoluteFilePath(QStringLiteral("look-dialog.png")));
            }
            ArtflowTheme::setLook(QStringLiteral("midnight"), QStringLiteral("violet"));
            ArtflowTheme::apply(*qApp);
            for (QWidget *widget : QApplication::allWidgets()) widget->update();

            QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowLook/panelSide"), QStringLiteral("left"));
            m_shell->togglePanels();
            m_shell->togglePanels();
            settle();
        };
        m_steps << [this]() {
            QDockWidget *layers = m_shell->window()->findChild<QDockWidget *>(QStringLiteral("ArtFlowLayersDock"));
            check(QStringLiteral("panels can sit on the left"),
                  layers && m_shell->window()->dockWidgetArea(layers) == Qt::LeftDockWidgetArea && layers->isVisible());
            screenshot(QStringLiteral("panels-left"));
            QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowLook/panelSide"), QStringLiteral("right"));
            m_shell->togglePanels();
            settle();
        };
        m_steps << [this]() {
            QDockWidget *layers = m_shell->window()->findChild<QDockWidget *>(QStringLiteral("ArtFlowLayersDock"));
            check(QStringLiteral("Tab hides the side panels for more room"), layers && !layers->isVisible());
            screenshot(QStringLiteral("panels-hidden"));
            m_shell->togglePanels();
            m_shell->showHome();
            settle();
        };
        m_steps << [this]() {
            HomeScreen *home = m_shell->window()->findChild<HomeScreen *>();
            check(QStringLiteral("the start screen opens"), home && home->isVisible() && home->findChildren<PageCard *>().size() == 4);
            screenshot(QStringLiteral("home"));
            if (home) home->hide();
            settle();
        };

        // GIF and MP4 export, when ffmpeg is next to the program
        m_steps << [this]() {
            const QString ffmpeg = ffmpegLocation();
            if (!QFile::exists(ffmpeg)) {
                log(QStringLiteral("INFO  export skipped: no %1").arg(QDir::toNativeSeparators(ffmpeg)));
                return;
            }
            const std::vector<LayerCamera *> cameras = editor()->object()->getLayersByType<LayerCamera>();
            ExportMovieDesc desc;
            desc.startFrame = 1;
            desc.endFrame = qMax(2, m_core->timeline()->lastUsedFrame());
            desc.fps = 12;
            desc.exportSize = QSize(640, 360);
            desc.strCameraName = cameras.empty() ? QString() : cameras.front()->name();

            for (const QString &format : {QStringLiteral("gif"), QStringLiteral("mp4")}) {
                desc.strFileName = m_out.absoluteFilePath(QStringLiteral("export.%1").arg(format));
                desc.loop = format == QLatin1String("gif");
                MovieExporter exporter;
                const Status status = exporter.run(editor()->object(), desc, [](float, float) {}, [](float) {}, [](QString) {});
                QFile file(desc.strFileName);
                QByteArray head;
                if (file.open(QIODevice::ReadOnly)) head = file.read(12);
                const bool looksRight = format == QLatin1String("gif") ? head.startsWith("GIF89a") : head.mid(4, 4) == "ftyp";
                check(QStringLiteral("exports a %1").arg(format.toUpper()), status.ok() && file.size() > 1000 && looksRight,
                      QStringLiteral("%1 bytes, %2 frames%3").arg(file.size()).arg(desc.endFrame)
                          .arg(status.ok() ? QString() : QStringLiteral(", ") + exporter.error()));
            }

            // ffmpeg reads the whole film back: it plays, and it is H.264.
            QProcess probe;
            probe.start(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-i"), m_out.absoluteFilePath(QStringLiteral("export.mp4")),
                                 QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-")});
            probe.waitForFinished(60000);
            const QString report = QString::fromLocal8Bit(probe.readAllStandardError());
            check(QStringLiteral("the MP4 plays back and is H.264"),
                  probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0 && report.contains(QLatin1String("Video: h264")),
                  report.section(QLatin1String("Stream #0:0"), 1, 1).section(QLatin1Char('\n'), 0, 0).trimmed().left(120));
        };
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
    int m_sampler {0};
    int m_layersBefore {0};
    QRgb m_adjustBefore {0};
    QPointF m_mirrorLeft;
    QPointF m_mirrorRight;
    QStringList m_samplerFailures;
    QVariantMap m_brushSettings;
    QVariantMap m_lookSettings;
    qreal m_scaleBefore {1.0};
    QString m_penUndoText;
    QString m_redoState;
};

}

void ArtflowSelfTest::run(ArtflowShell *shell, const QString &outputDir)
{
    (new SelfTest(shell, outputDir))->start();
}

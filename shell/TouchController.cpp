/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TouchController.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QLineF>
#include <QMouseEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QTouchEvent>
#include <QWidget>
#include <QtMath>

#include "editor.h"
#include "pencildef.h"
#include "viewmanager.h"

#include "StudioCore.h"
#include "StudioHistory.h"

namespace {
/// A finger that has not moved this far yet may still become a gesture.
/// Two hands rarely land together, so a lone finger waits a moment for the
/// other one (or a clear stroke) before it draws.
const qreal StartDistance = 40.0;
const int WaitMs = 170;
/// A second finger this soon after the first turns the first one's line
/// into part of the gesture: the line goes away.
const int CancelMs = 1000;
/// Fingers this soon after the pen are a resting palm.
const int PalmMs = 1500;
const int TapMs = 280;
const qreal TapTravel = 12.0;
/// Turning snaps to the nearest quarter turn within this many degrees.
const qreal SnapDegrees = 4.0;

/// The canvas as it was when the fingers came down, drawn moved, scaled and
/// turned with them: one picture to move instead of every layer to redraw.
class LiveView : public QWidget
{
public:
    LiveView(QWidget *canvas, const QImage &snapshot, const QColor &surround)
        : QWidget(canvas)
        , m_snapshot(QPixmap::fromImage(snapshot))
        , m_surround(surround)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setGeometry(canvas->rect());
    }
    void setTransform(const QTransform &transform)
    {
        m_transform = transform;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), m_surround);
        painter.setTransform(m_transform);
        painter.drawPixmap(QRectF(rect()), m_snapshot, QRectF(m_snapshot.rect()));
    }

private:
    QPixmap m_snapshot;
    QColor m_surround;
    QTransform m_transform;
};

bool fromTouch(const QMouseEvent *event)
{
    // Mouse clicks Windows or Qt made up from a finger.
    if (event->source() == Qt::MouseEventSynthesizedBySystem) return true;
    return event->source() == Qt::MouseEventSynthesizedByQt && event->pointingDevice()
        && event->pointingDevice()->type() == QInputDevice::DeviceType::TouchScreen;
}
}

TouchController::TouchController(StudioCore *core, QWidget *canvas)
    : QObject(canvas)
    , m_core(core)
    , m_canvas(canvas)
{
    canvas->setAttribute(Qt::WA_AcceptTouchEvents);
    canvas->installEventFilter(this);
    m_frame.setSingleShot(true);
    m_frame.setInterval(8);
    connect(&m_frame, &QTimer::timeout, this, [this]() {
        if (m_state == State::Gesture && m_pending.size() >= 2) updateGesture(m_pending);
    });
    m_wait.setSingleShot(true);
    connect(&m_wait, &QTimer::timeout, this, [this]() {
        if (m_state == State::Waiting) startDrawing();
    });
}

bool TouchController::fingersDraw()
{
    return QSettings(PENCIL2D, PENCIL2D).value(QStringLiteral("ArtFlowLook/fingersDraw"), true).toBool();
}

void TouchController::setFingersDraw(bool draw)
{
    QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowLook/fingersDraw"), draw);
}

bool TouchController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_canvas) {
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
            m_penClock.restart();
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
            // Those would draw behind the gesture's back: fingers come through
            // touch events only, and this controller draws for them itself.
            if (fromTouch(static_cast<QMouseEvent *>(event))) return true;
            break;
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel: {
            QTouchEvent *touch = static_cast<QTouchEvent *>(event);
            if (touch->device() && touch->device()->type() == QInputDevice::DeviceType::TouchScreen) {
                return handle(touch);
            }
            break;
        }
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool TouchController::handle(QTouchEvent *event)
{
    QList<QPointF> points;
    for (const QEventPoint &point : event->points()) {
        // From the screen position, which every touch carries.
        if (point.state() != QEventPoint::Released) points << m_canvas->mapFromGlobal(point.globalPosition());
    }
    event->accept();

    switch (event->type()) {
    case QEvent::TouchBegin:
        m_since.restart();
        m_maxPoints = int(points.size());
        m_travel = 0.0;
        m_start = m_last = points.value(0);
        m_held.clear();
        if (points.size() >= 2) {
            beginGesture(points);
        } else {
            m_state = State::Waiting;
            m_wait.start(WaitMs);
        }
        return true;

    case QEvent::TouchUpdate:
        m_maxPoints = qMax(m_maxPoints, int(points.size()));
        if (points.size() >= 2) {
            if (m_state == State::Drawing) {
                // A second finger soon after the first: it was a gesture,
                // not a line, so the dot the first finger began goes away.
                sendMouse(QEvent::MouseButtonRelease, m_last);
                if (m_since.elapsed() < CancelMs) m_core->history()->undo();
            }
            m_wait.stop();
            if (m_state == State::Gesture && int(points.size()) == m_gesturePoints) {
                // Many touch updates come per screen frame; only the latest counts.
                m_pending = points;
                if (!m_frame.isActive()) m_frame.start();
            } else {
                // New fingers (or one lifted): carry on from where they are now.
                beginGesture(points);
            }
            return true;
        }
        if (points.size() == 1) {
            const QPointF point = points.first();
            m_travel += QLineF(point, m_last).length();
            if (m_state == State::Waiting) {
                m_held << point;
                m_last = point;
                if (QLineF(point, m_start).length() > StartDistance) startDrawing();
                return true;
            }
            if (m_state == State::Drawing) {
                sendMouse(QEvent::MouseMove, point);
            } else if (m_state == State::Panning) {
                ViewManager *view = m_core->editor()->view();
                const QPointF from = view->mapScreenToCanvas(m_last);
                const QPointF to = view->mapScreenToCanvas(point);
                {
                    const QSignalBlocker quiet(view);
                    view->translate(view->translation() + (to - from));
                }
                updateLiveView();
            }
            m_last = point;
        }
        return true;

    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        m_wait.stop();
        if (m_state == State::Gesture) {
            m_frame.stop();
            if (m_pending.size() >= 2) updateGesture(m_pending);
        }
        if (m_state == State::Gesture || m_state == State::Panning) endLiveView();
        if (m_state == State::Waiting && event->type() == QEvent::TouchEnd) {
            // A quick tap leaves a dot.
            startDrawing();
        }
        if (m_state == State::Drawing) {
            sendMouse(QEvent::MouseButtonRelease, m_last);
        } else if (m_state == State::Gesture && m_since.elapsed() < TapMs && m_travel < TapTravel) {
            if (m_maxPoints == 2) m_core->history()->undo();
            if (m_maxPoints >= 3) m_core->history()->redo();
        }
        m_state = State::Idle;
        return true;

    default:
        return false;
    }
}

void TouchController::startDrawing()
{
    m_wait.stop();
    if (m_penClock.isValid() && m_penClock.elapsed() < PalmMs) {
        // The pen was just here: this is the hand resting on the screen.
        m_state = State::Idle;
        return;
    }
    if (!fingersDraw()) {
        m_state = State::Panning;
        startLiveView();
        return;
    }
    m_state = State::Drawing;
    // The finger's path while it waited is drawn too.
    sendMouse(QEvent::MouseButtonPress, m_start);
    for (const QPointF &point : m_held) sendMouse(QEvent::MouseMove, point);
    m_held.clear();
}

void TouchController::sendMouse(int type, const QPointF &pos)
{
    const bool release = type == QEvent::MouseButtonRelease;
    const bool press = type == QEvent::MouseButtonPress;
    QMouseEvent event(QEvent::Type(type), pos, m_canvas->mapToGlobal(pos), press || release ? Qt::LeftButton : Qt::NoButton,
                      release ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(m_canvas, &event);
}

void TouchController::beginGesture(const QList<QPointF> &points)
{
    ViewManager *view = m_core->editor()->view();
    const QPointF a = points.at(0);
    const QPointF b = points.at(1);
    const QPointF centre = (a + b) / 2.0;
    m_startDistance = qMax(1.0, QLineF(a, b).length());
    m_startAngle = qAtan2(b.y() - a.y(), b.x() - a.x());
    m_startScale = view->scaling();
    m_startRotation = view->rotation();
    m_canvasAnchor = view->mapScreenToCanvas(centre);
    m_last = centre;
    m_pending.clear();
    m_held.clear();
    m_gesturePoints = int(points.size());
    m_state = State::Gesture;
    startLiveView();
}

void TouchController::startLiveView()
{
    if (m_live) return;
    m_liveStart = m_core->editor()->view()->getView();

    // The canvas alone, without the controls that sit on it.
    QList<QWidget *> hidden;
    for (QWidget *child : m_canvas->findChildren<QWidget *>(Qt::FindDirectChildrenOnly)) {
        if (child->isVisible()) {
            child->hide();
            hidden << child;
        }
    }
    const QImage snapshot = m_canvas->grab().toImage();
    for (QWidget *child : hidden) child->show();

    LiveView *live = new LiveView(m_canvas, snapshot, snapshot.isNull() ? QColor(Qt::black) : snapshot.pixelColor(1, 1));
    live->show();
    live->raise();
    m_live = live;
}

void TouchController::updateLiveView()
{
    if (!m_live) return;
    // From where things were on screen when the fingers came down to where
    // they are now.
    static_cast<LiveView *>(m_live.data())->setTransform(m_liveStart.inverted() * m_core->editor()->view()->getView());
}

void TouchController::endLiveView()
{
    if (!m_live) return;
    delete m_live;
    // Now the canvas draws the page once, properly, where the fingers left it.
    ViewManager *view = m_core->editor()->view();
    view->translate(view->translation());
}

void TouchController::updateGesture(const QList<QPointF> &points)
{
    ViewManager *view = m_core->editor()->view();
    const QPointF a = points.at(0);
    const QPointF b = points.at(1);
    const QPointF centre = (a + b) / 2.0;

    {
        // The canvas is not told until the gesture ends; the live view
        // follows the fingers meanwhile.
        const QSignalBlocker quiet(view);
        view->scale(m_startScale * QLineF(a, b).length() / m_startDistance);
        qreal rotation = m_startRotation + qRadiansToDegrees(qAtan2(b.y() - a.y(), b.x() - a.x()) - m_startAngle);
        const qreal quarter = qRound(rotation / 90.0) * 90.0;
        if (qAbs(rotation - quarter) < SnapDegrees) rotation = quarter;
        view->rotate(float(rotation));
        // The point of the drawing that was under the fingers stays under them.
        const QPointF under = view->mapScreenToCanvas(centre);
        view->translate(view->translation() + (under - m_canvasAnchor));
    }
    updateLiveView();

    // A pinch is not a tap even when its middle stays put.
    m_travel += QLineF(centre, m_last).length();
    m_travel = qMax(m_travel, qAbs(QLineF(a, b).length() - m_startDistance));
    m_last = centre;
}

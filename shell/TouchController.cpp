/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TouchController.h"

#include <QApplication>
#include <QLineF>
#include <QMouseEvent>
#include <QSettings>
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
const qreal StartDistance = 6.0;
const int WaitMs = 90;
/// Fingers this soon after the pen are a resting palm.
const int PalmMs = 1500;
const int TapMs = 280;
const qreal TapTravel = 12.0;
/// Turning snaps to the nearest quarter turn within this many degrees.
const qreal SnapDegrees = 4.0;
}

TouchController::TouchController(StudioCore *core, QWidget *canvas)
    : QObject(canvas)
    , m_core(core)
    , m_canvas(canvas)
{
    canvas->setAttribute(Qt::WA_AcceptTouchEvents);
    canvas->installEventFilter(this);
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
                if (m_since.elapsed() < 300) m_core->history()->undo();
            }
            if (m_state == State::Gesture) {
                updateGesture(points);
            } else {
                beginGesture(points);
            }
            return true;
        }
        if (points.size() == 1) {
            const QPointF point = points.first();
            m_travel += QLineF(point, m_last).length();
            if (m_state == State::Waiting && QLineF(point, m_start).length() > StartDistance) {
                m_last = point;
                startDrawing();
            }
            if (m_state == State::Drawing) {
                sendMouse(QEvent::MouseMove, point);
            } else if (m_state == State::Panning) {
                ViewManager *view = m_core->editor()->view();
                const QPointF from = view->mapScreenToCanvas(m_last);
                const QPointF to = view->mapScreenToCanvas(point);
                view->translate(view->translation() + (to - from));
            }
            m_last = point;
        }
        return true;

    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        m_wait.stop();
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
        return;
    }
    m_state = State::Drawing;
    sendMouse(QEvent::MouseButtonPress, m_start);
    if (m_last != m_start) sendMouse(QEvent::MouseMove, m_last);
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
    m_state = State::Gesture;
}

void TouchController::updateGesture(const QList<QPointF> &points)
{
    ViewManager *view = m_core->editor()->view();
    const QPointF a = points.at(0);
    const QPointF b = points.at(1);
    const QPointF centre = (a + b) / 2.0;

    view->scale(m_startScale * QLineF(a, b).length() / m_startDistance);
    qreal rotation = m_startRotation + qRadiansToDegrees(qAtan2(b.y() - a.y(), b.x() - a.x()) - m_startAngle);
    const qreal quarter = qRound(rotation / 90.0) * 90.0;
    if (qAbs(rotation - quarter) < SnapDegrees) rotation = quarter;
    view->rotate(float(rotation));

    // The point of the drawing that was under the fingers stays under them.
    const QPointF under = view->mapScreenToCanvas(centre);
    view->translate(view->translation() + (under - m_canvasAnchor));

    // A pinch is not a tap even when its middle stays put.
    m_travel += QLineF(centre, m_last).length();
    m_travel = qMax(m_travel, qAbs(QLineF(a, b).length() - m_startDistance));
    m_last = centre;
}

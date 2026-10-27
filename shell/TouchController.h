/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_TOUCHCONTROLLER_H
#define ARTFLOW_TOUCHCONTROLLER_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QTimer>

class QTouchEvent;
class QWidget;
class StudioCore;

/**
 * Fingers on the canvas (touch screens such as the ASUS ProArt PX13):
 * one finger draws (or moves the page, if fingers are set not to draw),
 * two fingers pinch to zoom, drag to move and twist to turn the page; a
 * two-finger tap undoes and a three-finger tap redoes. Right after the
 * pen touches, a resting palm is ignored.
 */
class TouchController : public QObject
{
    Q_OBJECT
public:
    TouchController(StudioCore *core, QWidget *canvas);

    static bool fingersDraw();
    static void setFingersDraw(bool draw);

    /// Handles a touch event as if it came from the screen (for tests too).
    bool handle(QTouchEvent *event);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class State { Idle, Waiting, Drawing, Panning, Gesture };

    void startDrawing();
    void sendMouse(int type, const QPointF &pos);
    void beginGesture(const QList<QPointF> &points);
    void updateGesture(const QList<QPointF> &points);

    StudioCore *m_core;
    QWidget *m_canvas;
    State m_state = State::Idle;
    QTimer m_wait;
    QElapsedTimer m_since;
    QElapsedTimer m_penClock;
    QPointF m_start;
    QPointF m_last;
    int m_maxPoints = 0;
    qreal m_travel = 0.0;

    // The gesture as it began.
    QPointF m_canvasAnchor;
    qreal m_startDistance = 1.0;
    qreal m_startAngle = 0.0;
    qreal m_startScale = 1.0;
    qreal m_startRotation = 0.0;
};

#endif // ARTFLOW_TOUCHCONTROLLER_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_VIEWCONTROLS_H
#define ARTFLOW_VIEWCONTROLS_H

#include <QWidget>

class QToolButton;
class StudioCore;

/**
 * A small pill in the corner of the canvas: zoom out, the zoom (click for
 * 100 %), zoom in, fit the page, and straighten the page once it is turned.
 */
class ViewControls : public QWidget
{
    Q_OBJECT
public:
    ViewControls(StudioCore *core, QWidget *canvas);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void refresh();
    void place();

    StudioCore *m_core;
    QToolButton *m_zoom;
    QToolButton *m_straighten;
};

#endif // ARTFLOW_VIEWCONTROLS_H

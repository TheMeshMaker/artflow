/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_SYMMETRYGUIDE_H
#define ARTFLOW_SYMMETRYGUIDE_H

#include <QWidget>

class StudioCore;

/// The mirror line over the canvas while symmetry is on. It lets every click through.
class SymmetryGuide : public QWidget
{
    Q_OBJECT
public:
    explicit SymmetryGuide(StudioCore *core, QWidget *canvas);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    StudioCore *m_core;
};

#endif // ARTFLOW_SYMMETRYGUIDE_H

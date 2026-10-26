/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_SHELLWIDGETS_H
#define ARTFLOW_SHELLWIDGETS_H

#include <QWidget>

class QSpinBox;
class QToolButton;
class StudioCore;

namespace ShellWidgets {

/// A flat tool button with an icon from the engine's "playful" theme.
QToolButton *makeButton(const char *themeIcon, const QString &toolTip, QWidget *parent);

}

/**
 * First / previous / play-stop / next / last and the frame rate, shared by
 * the Flip and Motion panels.
 */
class TransportBar : public QWidget
{
    Q_OBJECT
public:
    TransportBar(StudioCore *core, QWidget *parent = nullptr);

private Q_SLOTS:
    void slotPlayingChanged(bool playing);
    void slotFrameRateChanged();

private:
    StudioCore *m_core;
    QToolButton *m_play;
    QSpinBox *m_fps;
};

#endif // ARTFLOW_SHELLWIDGETS_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_THEME_H
#define ARTFLOW_THEME_H

#include <QColor>
#include <QFont>

class QApplication;
class QWidget;

/**
 * ArtFlow's look: one dark palette, one accent, rounded surfaces. Every
 * custom-painted widget takes its colours from here, and apply() styles
 * the stock Qt widgets (including the engine's own panels) to match.
 */
namespace ArtflowTheme {

// Surfaces, from the window background up to hovered controls.
inline QColor window() { return QColor(0x0f, 0x11, 0x16); }
inline QColor panel() { return QColor(0x15, 0x18, 0x1e); }
inline QColor raised() { return QColor(0x1d, 0x21, 0x2a); }
inline QColor hover() { return QColor(0x26, 0x2b, 0x36); }
inline QColor line() { return QColor(0x2a, 0x2f, 0x3b); }
inline QColor canvasSurround() { return QColor(0x0b, 0x0d, 0x11); }

// Text, from primary to faint.
inline QColor text() { return QColor(0xe8, 0xea, 0xf0); }
inline QColor textMuted() { return QColor(0x95, 0x9c, 0xab); }
inline QColor textFaint() { return QColor(0x5d, 0x64, 0x72); }

// The one accent, and the camera colour of Motion mode.
inline QColor accent() { return QColor(0x8b, 0x7c, 0xff); }
inline QColor accentHover() { return QColor(0x9e, 0x92, 0xff); }
inline QColor accentSoft() { return QColor(0x8b, 0x7c, 0xff, 0x2e); }
inline QColor camera() { return QColor(0xff, 0xb4, 0x54); }
inline QColor danger() { return QColor(0xff, 0x6b, 0x6b); }

QFont uiFont(qreal pointSize = 9.5, int weight = QFont::Normal);

/// Fusion style, the dark palette, the UI font and the app-wide stylesheet.
void apply(QApplication &app);

/// Strips the light-grey inline styles the engine's panels set on themselves.
void restyleEnginePanel(QWidget *panel);

}

#endif // ARTFLOW_THEME_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_THEME_H
#define ARTFLOW_THEME_H

#include <QColor>
#include <QFont>
#include <QStringList>

class QApplication;
class QWidget;

/**
 * ArtFlow's look: a style (Midnight, Graphite or Paper), one accent, rounded
 * surfaces. Every custom-painted widget takes its colours from here, and
 * apply() styles the stock Qt widgets (including the engine's own panels) to
 * match. The look can change while the program runs.
 */
namespace ArtflowTheme {

struct Colors
{
    QColor window, panel, raised, hover, line, canvasSurround;
    QColor text, textMuted, textFaint;
    QColor accent;
    bool dark = true;
};

/// The colours of the current look.
const Colors &colors();

// Surfaces, from the window background up to hovered controls.
inline QColor window() { return colors().window; }
inline QColor panel() { return colors().panel; }
inline QColor raised() { return colors().raised; }
inline QColor hover() { return colors().hover; }
inline QColor line() { return colors().line; }
inline QColor canvasSurround() { return colors().canvasSurround; }

// Text, from primary to faint.
inline QColor text() { return colors().text; }
inline QColor textMuted() { return colors().textMuted; }
inline QColor textFaint() { return colors().textFaint; }

// The one accent, and the camera colour of Motion mode.
inline QColor accent() { return colors().accent; }
inline QColor accentHover() { return colors().accent.lighter(colors().dark ? 112 : 108); }
inline QColor accentSoft() { QColor c = colors().accent; c.setAlpha(colors().dark ? 0x2e : 0x33); return c; }
inline QColor camera() { return QColor(0xff, 0xb4, 0x54); }
inline QColor danger() { return QColor(0xff, 0x6b, 0x6b); }

/// The looks people can choose, by stable id and shown name.
QStringList styleIds();
QString styleName(const QString &id);
QStringList accentIds();
QColor accentColor(const QString &id);

QString currentStyle();
QString currentAccent();
/// Switches the look (saved for next time); call apply() to show it.
void setLook(const QString &style, const QString &accent);

QFont uiFont(qreal pointSize = 9.5, int weight = QFont::Normal);

/// Fusion style, the palette, the UI font and the app-wide stylesheet.
void apply(QApplication &app);

/// Strips the light-grey inline styles the engine's panels set on themselves.
void restyleEnginePanel(QWidget *panel);

}

#endif // ARTFLOW_THEME_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_ICONS_H
#define ARTFLOW_ICONS_H

#include <QColor>
#include <QIcon>
#include <QString>

/**
 * ArtFlow's own line icons (shell/icons/<name>.svg), recoloured per state:
 * muted when idle, bright on hover, white when checked, faint when
 * disabled. Rendered from the SVG at the exact size and pixel ratio, so
 * they stay crisp on high-DPI screens.
 */
namespace ArtflowIcons {

QIcon icon(const QString &name);

/// An icon that always uses \p color (e.g. the camera colour).
QIcon icon(const QString &name, const QColor &color);

}

#endif // ARTFLOW_ICONS_H

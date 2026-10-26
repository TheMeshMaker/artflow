/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_BEGINNERSHORTCUTS_H
#define ARTFLOW_BEGINNERSHORTCUTS_H

#include <QDialog>
#include <QString>

class StudioPenButtons;

/**
 * A short, familiar key map instead of the engine's ~110 shortcuts. The
 * engine reads its keys from the "shortcuts" settings group, so the map
 * is simply written there (once, so later changes by the user stick).
 * Every engine command not listed here gets no key.
 */
namespace BeginnerShortcuts {

/// Writes the beginner map into the settings unless it is already there.
void writeKeyMap();

/// The key of an engine command ("CmdUndo"...), as shown to people.
QString keysFor(const QString &command);

}

/// The "?" sheet: every key, gesture and pen button on one page.
class CheatSheetDialog : public QDialog
{
    Q_OBJECT
public:
    CheatSheetDialog(StudioPenButtons *pen, QWidget *parent = nullptr);
};

#endif // ARTFLOW_BEGINNERSHORTCUTS_H

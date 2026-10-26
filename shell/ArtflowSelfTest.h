/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_SELFTEST_H
#define ARTFLOW_SELFTEST_H

#include <QString>

class ArtflowShell;

/**
 * Developer self-test, enabled with ARTFLOW_SELFTEST=<output folder>.
 *
 * It drives the real canvas with synthetic tablet and mouse events (sent
 * to the canvas widget, so the system cursor never moves), checks the
 * results on the document, switches modes, edits frames, exercises the
 * pen-button path and writes report.txt plus one screenshot per mode into
 * the output folder. ARTFLOW_SELFTEST_QUIT=1 closes the app afterwards.
 */
namespace ArtflowSelfTest {
void run(ArtflowShell *shell, const QString &outputDir);
}

#endif // ARTFLOW_SELFTEST_H

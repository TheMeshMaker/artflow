/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "BeginnerShortcuts.h"

#include <QDialogButtonBox>
#include <QHash>
#include <QKeySequence>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

#include "pencildef.h"

#include "StudioPenButtons.h"
#include "StudioTools.h"

namespace {

/// Bump when the map changes, so existing installs pick it up once.
const int KeyMapVersion = 2;

const char ShortcutsGroup[] = "shortcuts";

/// Engine command -> the only key it keeps.
const QHash<QString, QString> &keyMap()
{
    static const QHash<QString, QString> keys = {
        {QStringLiteral("CmdNewFile"), QStringLiteral("Ctrl+N")},
        {QStringLiteral("CmdOpenFile"), QStringLiteral("Ctrl+O")},
        {QStringLiteral("CmdSaveFile"), QStringLiteral("Ctrl+S")},
        {QStringLiteral("CmdSaveAs"), QStringLiteral("Ctrl+Shift+S")},
        {QStringLiteral("CmdUndo"), QStringLiteral("Ctrl+Z")},
        {QStringLiteral("CmdRedo"), QStringLiteral("Ctrl+Y")},  // Ctrl+Shift+Z is added by the shell
        {QStringLiteral("CmdCut"), QStringLiteral("Ctrl+X")},
        {QStringLiteral("CmdCopy"), QStringLiteral("Ctrl+C")},
        {QStringLiteral("CmdPaste"), QStringLiteral("Ctrl+V")},
        {QStringLiteral("CmdSelectAll"), QStringLiteral("Ctrl+A")},
        {QStringLiteral("CmdDeselectAll"), QStringLiteral("Ctrl+D")},
        {QStringLiteral("CmdClearFrame"), QStringLiteral("Del")},
        {QStringLiteral("CmdZoomIn"), QStringLiteral("Ctrl++")},
        {QStringLiteral("CmdZoomOut"), QStringLiteral("Ctrl+-")},
        {QStringLiteral("CmdResetZoomRotate"), QStringLiteral("Ctrl+0")},
        {QStringLiteral("CmdPlay"), QStringLiteral("Ctrl+Return")},
        {QStringLiteral("CmdGotoNextFrame"), QStringLiteral("Right")},
        {QStringLiteral("CmdGotoPreviousFrame"), QStringLiteral("Left")},
        {QStringLiteral("CmdToolBrush"), QStringLiteral("B")},
        {QStringLiteral("CmdToolPencil"), QStringLiteral("N")},
        {QStringLiteral("CmdToolEraser"), QStringLiteral("E")},
        {QStringLiteral("CmdToolSmudge"), QStringLiteral("S")},
        {QStringLiteral("CmdToolBucket"), QStringLiteral("G")},
        {QStringLiteral("CmdToolEyedropper"), QStringLiteral("I")},
        {QStringLiteral("CmdToolSelect"), QStringLiteral("M")},
        {QStringLiteral("CmdToolMove"), QStringLiteral("V")},
        {QStringLiteral("CmdToolHand"), QStringLiteral("H")},
    };
    return keys;
}

QString shown(const QString &portable)
{
    return QKeySequence(portable, QKeySequence::PortableText).toString(QKeySequence::NativeText);
}

}

void BeginnerShortcuts::writeKeyMap()
{
    QSettings settings(PENCIL2D, PENCIL2D);
    if (settings.value(QStringLiteral("ArtFlowKeyMapVersion")).toInt() >= KeyMapVersion) return;

    QSettings defaults(QStringLiteral(":resources/kb.ini"), QSettings::IniFormat);
    defaults.beginGroup(QLatin1String(ShortcutsGroup));
    const QStringList commands = defaults.childKeys();
    defaults.endGroup();

    settings.beginGroup(QLatin1String(ShortcutsGroup));
    for (const QString &command : commands) {
        settings.setValue(command, keyMap().value(command));
    }
    settings.endGroup();
    settings.setValue(QStringLiteral("ArtFlowKeyMapVersion"), KeyMapVersion);
}

QString BeginnerShortcuts::keysFor(const QString &command)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.beginGroup(QLatin1String(ShortcutsGroup));
    return shown(settings.value(command, keyMap().value(command)).toString());
}

CheatSheetDialog::CheatSheetDialog(StudioPenButtons *pen, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keys and gestures"));

    auto row = [](const QString &keys, const QString &what) {
        return QStringLiteral("<tr><td style='padding:2px 18px 2px 0'><b>%1</b></td><td>%2</td></tr>")
            .arg(keys.toHtmlEscaped(), what.toHtmlEscaped());
    };
    auto section = [](const QString &title) {
        return QStringLiteral("<tr><td colspan='2' style='padding-top:10px'><big><b>%1</b></big></td></tr>")
            .arg(title.toHtmlEscaped());
    };

    QString html = QStringLiteral("<table>");

    html += section(tr("Tools"));
    for (const ToolIntentInfo &info : StudioTools::allIntents()) {
        html += row(shown(info.shortcut), info.label);
    }

    html += section(tr("Editing"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdUndo")), tr("Undo"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdRedo")) + QStringLiteral(" / ") + shown(QStringLiteral("Ctrl+Shift+Z")),
                tr("Redo"));
    html += row(QStringLiteral("[  ]"), tr("Smaller / bigger brush"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdCopy")) + QStringLiteral(" / ")
                    + BeginnerShortcuts::keysFor(QStringLiteral("CmdPaste")), tr("Copy / paste"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdSelectAll")), tr("Select everything"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdDeselectAll")), tr("Select nothing"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdClearFrame")), tr("Clear the frame"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdSaveFile")), tr("Save"));

    html += section(tr("Canvas"));
    html += row(tr("Space + drag"), tr("Move around"));
    html += row(tr("Mouse wheel"), tr("Zoom"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdResetZoomRotate")), tr("Reset the view"));

    html += section(tr("Frames"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdGotoPreviousFrame")) + QStringLiteral(" / ")
                    + BeginnerShortcuts::keysFor(QStringLiteral("CmdGotoNextFrame")), tr("Previous / next frame"));
    html += row(BeginnerShortcuts::keysFor(QStringLiteral("CmdPlay")), tr("Play / stop"));

    html += section(tr("Pen"));
    html += row(tr("Upper side button"), StudioPenButtons::actionName(pen->action(StudioPenButtons::UpperButton)));
    html += row(tr("Lower side button"), StudioPenButtons::actionName(pen->action(StudioPenButtons::LowerButton)));

    html += section(tr("Modes"));
    html += row(tr("Ctrl+Alt+1 / 2 / 3"), tr("Flip / Motion / Studio"));
    html += row(tr("F1 or ?"), tr("This sheet"));

    html += QStringLiteral("</table>");

    QLabel *label = new QLabel(html, this);
    label->setTextFormat(Qt::RichText);
    label->setContentsMargins(12, 12, 12, 12);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidget(label);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(scroll);
    layout->addWidget(box);
    resize(440, 640);
}

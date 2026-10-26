/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "PenButtonsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>



namespace {
const int DetectTimeoutMs = 10000;

QString buttonTitle(StudioPenButtons::Button button)
{
    return button == StudioPenButtons::UpperButton ? QObject::tr("Upper side button")
                                                   : QObject::tr("Lower side button (nearer the tip)");
}
}

PenButtonsDialog::PenButtonsDialog(StudioPenButtons *buttons, QWidget *parent)
    : QDialog(parent)
    , m_buttons(buttons)
{
    setWindowTitle(tr("Pen buttons"));
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr("Pens report their side buttons in different ways. Press <b>Detect</b>, "
                                    "then press that button on your pen (hovering over the screen is enough)."),
                               this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_enabled = new QCheckBox(tr("Use the pen buttons for these actions"), this);
    layout->addWidget(m_enabled);
    m_savedEnabled = m_buttons->isEnabled();

    QGridLayout *grid = new QGridLayout();
    grid->setColumnStretch(2, 1);

    for (int i = 0; i < StudioPenButtons::ButtonCount; ++i) {
        const StudioPenButtons::Button button = StudioPenButtons::Button(i);

        QLabel *title = new QLabel(QStringLiteral("<b>%1</b>").arg(buttonTitle(button)), this);

        m_action[i] = new QComboBox(this);
        for (StudioPenButtons::Action action : {StudioPenButtons::UndoAction, StudioPenButtons::RedoAction,
                                                StudioPenButtons::EraserAction, StudioPenButtons::NoAction}) {
            m_action[i]->addItem(StudioPenButtons::actionName(action), int(action));
        }

        m_savedSignature[i] = m_buttons->signature(button);
        m_savedAction[i] = m_buttons->action(button);

        m_signature[i] = new QLabel(this);
        m_signature[i]->setEnabled(false);

        m_detect[i] = new QPushButton(tr("Detect…"), this);
        connect(m_detect[i], &QPushButton::clicked, this, [this, button]() { detect(button); });

        grid->addWidget(title, i * 2, 0, 1, 4);
        grid->addWidget(m_action[i], i * 2 + 1, 0);
        grid->addWidget(m_detect[i], i * 2 + 1, 1);
        grid->addWidget(m_signature[i], i * 2 + 1, 2, 1, 2);
    }
    layout->addLayout(grid);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(m_status->fontMetrics().height() * 2);
    layout->addWidget(m_status);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                                 | QDialogButtonBox::RestoreDefaults, this);
    connect(box, &QDialogButtonBox::accepted, this, &PenButtonsDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &PenButtonsDialog::reject);
    connect(box->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &PenButtonsDialog::resetToDefaults);
    layout->addWidget(box);

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(DetectTimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, &PenButtonsDialog::slotCaptureTimeout);
    connect(m_buttons, &StudioPenButtons::captured, this, &PenButtonsDialog::slotCaptured);

    refresh();
}

PenButtonsDialog::~PenButtonsDialog()
{
    m_buttons->cancelCapture();
}

void PenButtonsDialog::refresh()
{
    m_enabled->setChecked(m_buttons->isEnabled());
    for (int i = 0; i < StudioPenButtons::ButtonCount; ++i) {
        const StudioPenButtons::Button button = StudioPenButtons::Button(i);
        m_action[i]->setCurrentIndex(m_action[i]->findData(int(m_buttons->action(button))));
        m_signature[i]->setText(m_buttons->signature(button).describe());
    }
}

void PenButtonsDialog::detect(StudioPenButtons::Button button)
{
    m_detecting = button;
    for (int i = 0; i < StudioPenButtons::ButtonCount; ++i) {
        m_detect[i]->setEnabled(false);
    }
    m_status->setText(tr("Press the <b>%1</b> on your pen now… (Esc cancels)").arg(buttonTitle(button).toLower()));
    m_buttons->startCapture();
    m_timeout.start();
}

void PenButtonsDialog::slotCaptured(const StudioPenButtons::Signature &signature)
{
    if (m_detecting < 0) return;

    const StudioPenButtons::Button button = StudioPenButtons::Button(m_detecting);
    m_buttons->setSignature(button, signature);
    refresh();
    stopDetecting(tr("Got it: %1.").arg(signature.describe()));
}

void PenButtonsDialog::slotCaptureTimeout()
{
    m_buttons->cancelCapture();
    stopDetecting(tr("No pen button was seen. If the button opens a Windows menu or app instead, "
                       "turn that shortcut off under Settings › Bluetooth & devices › Pen & Windows Ink."));
}

void PenButtonsDialog::stopDetecting(const QString &message)
{
    m_detecting = -1;
    m_timeout.stop();
    for (int i = 0; i < StudioPenButtons::ButtonCount; ++i) {
        m_detect[i]->setEnabled(true);
    }
    m_status->setText(message);
}

void PenButtonsDialog::resetToDefaults()
{
    m_buttons->cancelCapture();
    m_buttons->resetToDefaults();
    refresh();
    stopDetecting(tr("Defaults restored: upper button redoes, lower button undoes."));
}

void PenButtonsDialog::accept()
{
    m_buttons->cancelCapture();
    m_buttons->setEnabled(m_enabled->isChecked());
    for (int i = 0; i < StudioPenButtons::ButtonCount; ++i) {
        m_buttons->setAction(StudioPenButtons::Button(i),
                             StudioPenButtons::Action(m_action[i]->currentData().toInt()));
    }
    m_buttons->save();
    QDialog::accept();
}

void PenButtonsDialog::reject()
{
    if (m_detecting >= 0) {
        // Esc while detecting only stops detecting.
        m_buttons->cancelCapture();
        stopDetecting(tr("Detection cancelled."));
        return;
    }
    // Throw away detected signatures that were not confirmed with OK.
    m_buttons->setEnabled(m_savedEnabled);
    for (int i = 0; i < StudioPenButtons::ButtonCount; ++i) {
        m_buttons->setSignature(StudioPenButtons::Button(i), m_savedSignature[i]);
        m_buttons->setAction(StudioPenButtons::Button(i), m_savedAction[i]);
    }
    QDialog::reject();
}

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_PENBUTTONSDIALOG_H
#define ARTFLOW_PENBUTTONSDIALOG_H

#include <QDialog>
#include <QTimer>

#include "StudioPenButtons.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

/**
 * Pen button settings: what each side button does, and the "press each
 * button" step that records how this particular pen reports it.
 */
class PenButtonsDialog : public QDialog
{
    Q_OBJECT
public:
    PenButtonsDialog(StudioPenButtons *buttons, QWidget *parent = nullptr);
    ~PenButtonsDialog() override;

protected:
    void reject() override;
    void accept() override;

private Q_SLOTS:
    void detect(StudioPenButtons::Button button);
    void slotCaptured(const StudioPenButtons::Signature &signature);
    void slotCaptureTimeout();
    void resetToDefaults();

private:
    void refresh();
    void stopDetecting(const QString &message);

private:
    StudioPenButtons *m_buttons;
    QCheckBox *m_enabled;
    QComboBox *m_action[StudioPenButtons::ButtonCount];
    QLabel *m_signature[StudioPenButtons::ButtonCount];
    QPushButton *m_detect[StudioPenButtons::ButtonCount];
    QLabel *m_status;

    int m_detecting {-1};
    QTimer m_timeout;

    // What the settings were when the dialog opened, for Cancel.
    bool m_savedEnabled {true};
    StudioPenButtons::Signature m_savedSignature[StudioPenButtons::ButtonCount];
    StudioPenButtons::Action m_savedAction[StudioPenButtons::ButtonCount] {};
};

#endif // ARTFLOW_PENBUTTONSDIALOG_H

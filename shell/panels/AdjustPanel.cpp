/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "AdjustPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "StudioCore.h"

#include "ArtflowTheme.h"
#include "ShellControls.h"

namespace {
const int Shadow = 14;
}

AdjustPanel::AdjustPanel(StudioCore *core, bool selectionOnly, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_core(core)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Shadow + 18, Shadow + 14, Shadow + 18, Shadow + 16);
    layout->setSpacing(8);

    QLabel *title = new QLabel(tr("Adjust"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    title->setFont(ArtflowTheme::uiFont(11, QFont::DemiBold));
    layout->addWidget(title);
    QLabel *hint = new QLabel(selectionOnly ? tr("Changes the selected part of this drawing.")
                                            : tr("Changes the drawing on this frame."), this);
    hint->setObjectName(QStringLiteral("muted"));
    layout->addWidget(hint);
    layout->addSpacing(4);

    m_hue = new ValueSlider(tr("Hue"), -180, 180, 0, this);
    m_hue->setSuffix(QStringLiteral("°"));
    m_saturation = new ValueSlider(tr("Saturation"), -100, 100, 0, this);
    m_brightness = new ValueSlider(tr("Brightness"), -100, 100, 0, this);
    m_contrast = new ValueSlider(tr("Contrast"), -100, 100, 0, this);
    m_blur = new ValueSlider(tr("Blur"), 0, 40, 0, this);
    m_blur->setSuffix(tr(" px"));
    for (ValueSlider *slider : {m_hue, m_saturation, m_brightness, m_contrast, m_blur}) {
        slider->setMinimumWidth(300);
        layout->addWidget(slider);
        connect(slider, &ValueSlider::valueChanged, this, [this]() { m_preview.start(); });
    }

    QHBoxLayout *toggles = new QHBoxLayout();
    toggles->setSpacing(8);
    auto toggle = [this](const QString &text) {
        QToolButton *button = new QToolButton(this);
        button->setObjectName(QStringLiteral("pillButton"));
        button->setText(text);
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QToolButton::toggled, this, [this]() { m_preview.start(); });
        return button;
    };
    m_invert = toggle(tr("Invert"));
    m_grayscale = toggle(tr("Black && white"));
    toggles->addWidget(m_invert);
    toggles->addWidget(m_grayscale);
    toggles->addStretch(1);
    layout->addSpacing(2);
    layout->addLayout(toggles);
    layout->addSpacing(8);

    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *resetButton = new QPushButton(tr("Reset"), this);
    QPushButton *cancel = new QPushButton(tr("Cancel"), this);
    QPushButton *apply = new QPushButton(tr("Apply"), this);
    apply->setDefault(true);
    connect(resetButton, &QPushButton::clicked, this, &AdjustPanel::reset);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(resetButton);
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addWidget(apply);
    layout->addLayout(buttons);

    // Big drawings take a moment to redo: wait until the slider pauses.
    m_preview.setSingleShot(true);
    m_preview.setInterval(40);
    connect(&m_preview, &QTimer::timeout, this, [this]() { m_core->adjust()->preview(adjustments()); });
}

Adjustments AdjustPanel::adjustments() const
{
    Adjustments a;
    a.hue = m_hue->value();
    a.saturation = m_saturation->value();
    a.brightness = m_brightness->value();
    a.contrast = m_contrast->value();
    a.blur = m_blur->value();
    a.invert = m_invert->isChecked();
    a.grayscale = m_grayscale->isChecked();
    return a;
}

void AdjustPanel::setAdjustments(const Adjustments &a)
{
    m_hue->setValue(a.hue);
    m_saturation->setValue(a.saturation);
    m_brightness->setValue(a.brightness);
    m_contrast->setValue(a.contrast);
    m_blur->setValue(a.blur);
    m_invert->setChecked(a.invert);
    m_grayscale->setChecked(a.grayscale);
}

void AdjustPanel::reset()
{
    setAdjustments(Adjustments());
    m_preview.start();
}

void AdjustPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF panel = QRectF(rect()).adjusted(Shadow, Shadow, -Shadow, -Shadow);
    for (int i = Shadow; i > 0; i -= 2) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, int(26 * (1.0 - qreal(i) / Shadow))));
        painter.drawRoundedRect(panel.adjusted(-i, -i + 4, i, i + 4), 16 + i, 16 + i);
    }
    painter.setPen(QPen(ArtflowTheme::line(), 1));
    painter.setBrush(ArtflowTheme::panel());
    painter.drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
}

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "BrushSlidersPanel.h"

#include <cmath>

#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "StudioCore.h"
#include "StudioTools.h"

namespace {

// The size slider is logarithmic: fine steps for thin lines, big ones for
// wide brushes. Slider units 0..1000 map to 1..MaxSize px.
const qreal MaxSize = 200.0;
const int SliderSteps = 1000;

int sizeToSlider(qreal size)
{
    return qRound(std::log(qMax(1.0, size)) / std::log(MaxSize) * SliderSteps);
}

qreal sliderToSize(int value)
{
    return std::pow(MaxSize, qreal(value) / SliderSteps);
}

QSlider *makeSlider(QWidget *parent)
{
    QSlider *slider = new QSlider(Qt::Vertical, parent);
    slider->setMinimumHeight(160);
    slider->setFocusPolicy(Qt::NoFocus);
    return slider;
}

}

BrushSlidersPanel::BrushSlidersPanel(StudioCore *core, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 10, 6, 10);
    layout->setSpacing(4);

    m_sizeLabel = new QLabel(this);
    m_sizeLabel->setAlignment(Qt::AlignCenter);
    m_size = makeSlider(this);
    m_size->setRange(0, SliderSteps);
    m_size->setToolTip(tr("Brush size  ( [ smaller, ] bigger )"));

    m_opacityLabel = new QLabel(this);
    m_opacityLabel->setAlignment(Qt::AlignCenter);
    m_opacity = makeSlider(this);
    m_opacity->setRange(1, 100);
    m_opacity->setToolTip(tr("Brush opacity"));

    QLabel *sizeTitle = new QLabel(tr("Size", "brush size, short"), this);
    QLabel *opacityTitle = new QLabel(tr("Opacity", "brush opacity, short"), this);
    sizeTitle->setAlignment(Qt::AlignCenter);
    opacityTitle->setAlignment(Qt::AlignCenter);

    layout->addWidget(sizeTitle);
    layout->addWidget(m_size, 3, Qt::AlignHCenter);
    layout->addWidget(m_sizeLabel);
    layout->addSpacing(16);
    layout->addWidget(opacityTitle);
    layout->addWidget(m_opacity, 2, Qt::AlignHCenter);
    layout->addWidget(m_opacityLabel);
    layout->addStretch(1);

    connect(m_size, &QSlider::valueChanged, this, [this](int value) {
        m_core->tools()->setBrushSize(sliderToSize(value));
    });
    connect(m_opacity, &QSlider::valueChanged, this, [this](int value) {
        m_core->tools()->setBrushOpacity(value / 100.0);
    });
    connect(core->tools(), &StudioTools::brushChanged, this, &BrushSlidersPanel::slotBrushChanged);

    setFixedWidth(64);
    slotBrushChanged();
}

void BrushSlidersPanel::slotBrushChanged()
{
    const qreal size = m_core->tools()->brushSize();
    const qreal opacity = m_core->tools()->brushOpacity();

    {
        QSignalBlocker blocker(m_size);
        m_size->setValue(sizeToSlider(size));
    }
    {
        QSignalBlocker blocker(m_opacity);
        m_opacity->setValue(qRound(opacity * 100));
    }
    m_size->setEnabled(size > 0);
    m_sizeLabel->setText(tr("%1 px", "brush size in pixels").arg(qRound(size)));
    m_opacityLabel->setText(tr("%1%", "percent").arg(qRound(opacity * 100)));
}

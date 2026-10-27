/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "BrushSlidersPanel.h"

#include <cmath>

#include <QVBoxLayout>

#include "StudioCore.h"
#include "StudioTools.h"

#include "ArtflowIcons.h"
#include "ShellControls.h"

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

}

BrushSlidersPanel::BrushSlidersPanel(StudioCore *core, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
{
    setObjectName(QStringLiteral("artflowPanel"));
    setAttribute(Qt::WA_StyledBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 14, 10, 14);
    layout->setSpacing(18);

    m_size = new PillSlider(this);
    m_size->setRange(0, SliderSteps);
    m_size->setIcon(ArtflowIcons::icon(QStringLiteral("size")));
    m_size->setToolTip(tr("Brush size  ( [ smaller, ] bigger )"));

    m_opacity = new PillSlider(this);
    m_opacity->setRange(1, 100);
    m_opacity->setIcon(ArtflowIcons::icon(QStringLiteral("opacity")));
    m_opacity->setToolTip(tr("Brush opacity"));

    // On a big screen the sliders stop growing and the rest stays empty.
    m_size->setMaximumHeight(300);
    m_opacity->setMaximumHeight(220);
    layout->addWidget(m_size, 3, Qt::AlignHCenter);
    layout->addWidget(m_opacity, 2, Qt::AlignHCenter);
    layout->addStretch(1);

    connect(m_size, &PillSlider::valueChanged, this, [this](int value) {
        m_core->tools()->setBrushSize(sliderToSize(value));
    });
    connect(m_opacity, &PillSlider::valueChanged, this, [this](int value) {
        m_core->tools()->setBrushOpacity(value / 100.0);
    });
    connect(core->tools(), &StudioTools::brushChanged, this, &BrushSlidersPanel::slotBrushChanged);

    setFixedWidth(62);
    slotBrushChanged();
}

void BrushSlidersPanel::slotBrushChanged()
{
    const qreal size = m_core->tools()->brushSize();
    const qreal opacity = m_core->tools()->brushOpacity();

    m_size->setEnabled(size > 0);
    m_size->setValue(sizeToSlider(size));
    m_size->setValueText(size > 0 ? QString::number(qRound(size)) : QStringLiteral("–"));

    m_opacity->setValue(qRound(opacity * 100));
    m_opacity->setValueText(QString::number(qRound(opacity * 100)));
}

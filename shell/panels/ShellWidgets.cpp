/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ShellWidgets.h"

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>

#include "StudioCore.h"
#include "StudioPlayback.h"
#include "StudioTimeline.h"

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"

QToolButton *ShellWidgets::makeButton(const QString &icon, const QString &toolTip, QWidget *parent)
{
    QToolButton *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("iconButton"));
    button->setIcon(ArtflowIcons::icon(icon));
    button->setToolTip(toolTip);
    button->setIconSize(QSize(20, 20));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

TransportBar::TransportBar(StudioCore *core, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QToolButton *first = ShellWidgets::makeButton(QStringLiteral("first"), tr("First frame"), this);
    QToolButton *previous = ShellWidgets::makeButton(QStringLiteral("prev"), tr("Previous frame  (Left)"), this);
    m_play = ShellWidgets::makeButton(QStringLiteral("play"), tr("Play"), this);
    m_play->setObjectName(QStringLiteral("playButton"));
    m_play->setFixedSize(40, 40);
    m_play->setStyleSheet(QStringLiteral("QToolButton#playButton { border-radius: 20px; padding: 0; background: %1; }"
                                         "QToolButton#playButton:hover { background: %2; }")
                              .arg(ArtflowTheme::accent().name(), ArtflowTheme::accentHover().name()));
    QToolButton *next = ShellWidgets::makeButton(QStringLiteral("next"), tr("Next frame  (Right)"), this);
    QToolButton *last = ShellWidgets::makeButton(QStringLiteral("last"), tr("Last frame"), this);

    StudioPlayback *playback = core->playback();
    connect(first, &QToolButton::clicked, playback, &StudioPlayback::firstFrame);
    connect(previous, &QToolButton::clicked, playback, &StudioPlayback::previousFrame);
    connect(m_play, &QToolButton::clicked, playback, &StudioPlayback::togglePlay);
    connect(next, &QToolButton::clicked, playback, &StudioPlayback::nextFrame);
    connect(last, &QToolButton::clicked, playback, &StudioPlayback::lastFrame);
    connect(playback, &StudioPlayback::playingChanged, this, &TransportBar::slotPlayingChanged);

    m_fps = new QSpinBox(this);
    m_fps->setRange(1, 60);
    m_fps->setSuffix(tr(" fps", "frames per second, after the number"));
    m_fps->setToolTip(tr("Playback speed (scroll to change)"));
    m_fps->setAlignment(Qt::AlignCenter);
    m_fps->setFixedWidth(70);
    connect(m_fps, QOverload<int>::of(&QSpinBox::valueChanged), core->timeline(), &StudioTimeline::setFrameRate);
    connect(core->timeline(), &StudioTimeline::frameRateChanged, this, &TransportBar::slotFrameRateChanged);

    layout->addWidget(first);
    layout->addWidget(previous);
    layout->addWidget(m_play);
    layout->addWidget(next);
    layout->addWidget(last);
    layout->addSpacing(8);
    layout->addWidget(m_fps);

    slotPlayingChanged(playback->isPlaying());
    slotFrameRateChanged();
}

void TransportBar::slotPlayingChanged(bool playing)
{
    m_play->setIcon(ArtflowIcons::icon(playing ? QStringLiteral("stop") : QStringLiteral("play"), Qt::white));
    m_play->setToolTip(playing ? tr("Stop") : tr("Play"));
}

void TransportBar::slotFrameRateChanged()
{
    QSignalBlocker blocker(m_fps);
    m_fps->setValue(m_core->timeline()->frameRate());
}

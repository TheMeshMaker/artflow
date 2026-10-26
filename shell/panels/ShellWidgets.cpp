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

QToolButton *ShellWidgets::makeButton(const char *themeIcon, const QString &toolTip, QWidget *parent)
{
    QToolButton *button = new QToolButton(parent);
    button->setIcon(QIcon(QStringLiteral(":/icons/themes/playful/%1.svg").arg(QLatin1String(themeIcon))));
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    button->setIconSize(QSize(22, 22));
    return button;
}

TransportBar::TransportBar(StudioCore *core, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QToolButton *first = ShellWidgets::makeButton("controls/control-play-start", tr("First frame"), this);
    QToolButton *previous = ShellWidgets::makeButton("controls/control-play-start", tr("Previous frame"), this);
    previous->setIcon(QIcon());
    previous->setText(QStringLiteral("‹"));
    m_play = ShellWidgets::makeButton("controls/control-play", tr("Play"), this);
    QToolButton *next = ShellWidgets::makeButton("controls/control-play-end", tr("Next frame"), this);
    next->setIcon(QIcon());
    next->setText(QStringLiteral("›"));
    QToolButton *last = ShellWidgets::makeButton("controls/control-play-end", tr("Last frame"), this);

    for (QToolButton *button : {previous, next}) {
        QFont font = button->font();
        font.setPointSizeF(font.pointSizeF() * 1.6);
        font.setBold(true);
        button->setFont(font);
    }

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
    m_fps->setToolTip(tr("Playback speed"));
    connect(m_fps, QOverload<int>::of(&QSpinBox::valueChanged), core->timeline(), &StudioTimeline::setFrameRate);
    connect(core->timeline(), &StudioTimeline::frameRateChanged, this, &TransportBar::slotFrameRateChanged);

    layout->addWidget(first);
    layout->addWidget(previous);
    layout->addWidget(m_play);
    layout->addWidget(next);
    layout->addWidget(last);
    layout->addSpacing(6);
    layout->addWidget(m_fps);

    slotPlayingChanged(playback->isPlaying());
    slotFrameRateChanged();
}

void TransportBar::slotPlayingChanged(bool playing)
{
    m_play->setIcon(QIcon(playing ? QStringLiteral(":/icons/themes/playful/controls/control-stop.svg")
                                  : QStringLiteral(":/icons/themes/playful/controls/control-play.svg")));
    m_play->setToolTip(playing ? tr("Stop") : tr("Play"));
}

void TransportBar::slotFrameRateChanged()
{
    QSignalBlocker blocker(m_fps);
    m_fps->setValue(m_core->timeline()->frameRate());
}

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "FrameStripPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include "layer.h"

#include "StudioCore.h"
#include "StudioLayers.h"
#include "StudioTimeline.h"
#include "StudioTimelineModel.h"

#include "ShellWidgets.h"

namespace {
const int CellWidth = 34;
const int CellHeight = 46;
const int CellGap = 4;
}

// ---------------------------------------------------------------- FrameCells

FrameCells::FrameCells(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_model(model)
{
    setMinimumHeight(CellHeight + 2 * CellGap);
    connect(model, &QAbstractItemModel::modelReset, this, [this]() {
        resize(sizeHint());
        update();
    });
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() { update(); });
}

QSize FrameCells::sizeHint() const
{
    return QSize(CellGap + m_model->frameCount() * (CellWidth + CellGap), CellHeight + 2 * CellGap);
}

QRect FrameCells::cellRect(int frame) const
{
    const int column = StudioTimelineModel::columnOfFrame(frame);
    return QRect(CellGap + column * (CellWidth + CellGap), CellGap, CellWidth, CellHeight);
}

void FrameCells::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPalette pal = palette();
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor text = pal.color(QPalette::WindowText);
    QColor faded = accent;
    faded.setAlphaF(0.28);
    QColor outline = text;
    outline.setAlphaF(0.25);

    const int row = m_model->activeRow();

    for (int column = 0; column < m_model->frameCount(); ++column) {
        const int frame = StudioTimelineModel::frameOfColumn(column);
        const QRectF cell = QRectF(cellRect(frame)).adjusted(0.5, 0.5, -0.5, -0.5);

        bool key = false;
        bool hold = false;
        if (row >= 0) {
            const QModelIndex index = m_model->index(row, column);
            key = index.data(StudioTimelineModel::HasKeyframeRole).toBool();
            hold = index.data(StudioTimelineModel::IsHoldRole).toBool();
        }

        if (key) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);
        } else if (hold) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(faded);
        } else {
            painter.setPen(QPen(outline, 1, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
        }
        painter.drawRoundedRect(cell, 4, 4);

        if (frame == m_model->currentFrame()) {
            painter.setPen(QPen(text, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(cell.adjusted(-2, -2, 2, 2), 5, 5);
        }

        painter.setPen(key ? pal.color(QPalette::HighlightedText) : text);
        painter.drawText(cell, Qt::AlignCenter, QString::number(frame));
    }
}

void FrameCells::mousePressEvent(QMouseEvent *event)
{
    const int column = (int(event->position().x()) - CellGap) / (CellWidth + CellGap);
    if (column >= 0 && column < m_model->frameCount()) {
        m_core->timeline()->setCurrentFrame(StudioTimelineModel::frameOfColumn(column));
    }
}

// ----------------------------------------------------------- FrameStripPanel

FrameStripPanel::FrameStripPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_model(model)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);

    QHBoxLayout *bar = new QHBoxLayout();
    bar->setSpacing(2);

    m_layerLabel = new QLabel(this);
    m_layerLabel->setToolTip(tr("Frames of this layer"));
    m_frameLabel = new QLabel(this);

    QToolButton *add = ShellWidgets::makeButton("timeline/frame-add", tr("New frame after this one"), this);
    QToolButton *duplicate = ShellWidgets::makeButton("timeline/frame-duplicate", tr("Copy this frame into the next one"), this);
    QToolButton *remove = ShellWidgets::makeButton("timeline/frame-remove", tr("Delete this frame"), this);
    m_onion = ShellWidgets::makeButton("onion/onion-prev", tr("Onion skin: see the frames before and after"), this);
    m_onion->setIcon(QIcon());
    m_onion->setText(tr("Onion"));
    m_onion->setCheckable(true);

    StudioTimeline *timeline = core->timeline();
    StudioLayers *layers = core->layers();
    connect(add, &QToolButton::clicked, this, [timeline, layers]() {
        timeline->addFrameAfter(layers->activeLayer(), timeline->currentFrame());
    });
    connect(duplicate, &QToolButton::clicked, this, [timeline, layers]() {
        timeline->duplicateFrame(layers->activeLayer(), timeline->currentFrame());
    });
    connect(remove, &QToolButton::clicked, this, [timeline, layers]() {
        timeline->removeFrame(layers->activeLayer(), timeline->currentFrame());
    });
    connect(m_onion, &QToolButton::toggled, layers, &StudioLayers::setOnionSkins);
    connect(layers, &StudioLayers::onionSkinsChanged, this, [this](bool on) {
        QSignalBlocker blocker(m_onion);
        m_onion->setChecked(on);
    });

    bar->addWidget(m_layerLabel);
    bar->addSpacing(12);
    bar->addWidget(new TransportBar(core, this));
    bar->addSpacing(12);
    bar->addWidget(add);
    bar->addWidget(duplicate);
    bar->addWidget(remove);
    bar->addSpacing(12);
    bar->addWidget(m_onion);
    bar->addStretch(1);
    bar->addWidget(m_frameLabel);
    layout->addLayout(bar);

    m_cells = new FrameCells(core, model, this);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(m_cells);
    m_scroll->setWidgetResizable(false);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFixedHeight(m_cells->sizeHint().height() + m_scroll->horizontalScrollBar()->sizeHint().height());
    layout->addWidget(m_scroll);

    connect(model, &QAbstractItemModel::modelReset, this, &FrameStripPanel::slotModelChanged);
    connect(timeline, &StudioTimeline::currentFrameChanged, this, &FrameStripPanel::slotCurrentFrameChanged);

    m_cells->resize(m_cells->sizeHint());
    {
        QSignalBlocker blocker(m_onion);
        m_onion->setChecked(layers->onionSkins());
    }
    slotModelChanged();
}

void FrameStripPanel::slotModelChanged()
{
    Layer *layer = m_model->layerAt(m_model->activeRow());
    m_layerLabel->setText(layer ? QStringLiteral("<b>%1</b>").arg(layer->name().toHtmlEscaped()) : tr("No layer"));
    slotCurrentFrameChanged(m_model->currentFrame());
}

void FrameStripPanel::slotCurrentFrameChanged(int frame)
{
    m_frameLabel->setText(tr("Frame %1").arg(frame));
    m_scroll->ensureVisible(m_cells->cellRect(frame).center().x(), m_cells->height() / 2, 60, 0);
}

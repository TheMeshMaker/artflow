/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TracksPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include "layer.h"

#include "StudioCore.h"
#include "StudioLayers.h"
#include "StudioTimeline.h"
#include "StudioTimelineModel.h"

#include "ShellWidgets.h"

namespace {
const int NameWidth = 150;
const int FrameWidth = 16;
const int RowHeight = 26;
const int RulerHeight = 20;

QPainterPath diamond(const QPointF &center, qreal radius)
{
    QPainterPath path;
    path.moveTo(center.x(), center.y() - radius);
    path.lineTo(center.x() + radius, center.y());
    path.lineTo(center.x(), center.y() + radius);
    path.lineTo(center.x() - radius, center.y());
    path.closeSubpath();
    return path;
}
}

// ----------------------------------------------------------------- TrackGrid

TrackGrid::TrackGrid(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_model(model)
{
    connect(model, &QAbstractItemModel::modelReset, this, [this]() {
        resize(sizeHint());
        update();
    });
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() { update(); });
}

QSize TrackGrid::sizeHint() const
{
    return QSize(NameWidth + m_model->frameCount() * FrameWidth + 1,
                 RulerHeight + qMax(1, m_model->rowCount()) * RowHeight + 1);
}

int TrackGrid::playheadX() const
{
    return NameWidth + StudioTimelineModel::columnOfFrame(m_model->currentFrame()) * FrameWidth + FrameWidth / 2;
}

void TrackGrid::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::WindowText);
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor camera(0xd9, 0x8a, 0x2b);
    QColor grid = text;
    grid.setAlphaF(0.12);
    QColor activeRow = accent;
    activeRow.setAlphaF(0.10);

    const int frames = m_model->frameCount();
    const int rows = m_model->rowCount();

    for (int column = 0; column < frames; ++column) {
        const int frame = StudioTimelineModel::frameOfColumn(column);
        const int x = NameWidth + column * FrameWidth;
        painter.setPen(grid);
        painter.drawLine(x, RulerHeight - 4, x, height());
        if (frame % 5 == 0 || frame == 1) {
            painter.setPen(text);
            painter.drawText(QRect(x - 10, 0, FrameWidth + 20, RulerHeight - 4), Qt::AlignCenter, QString::number(frame));
        }
    }

    for (int row = 0; row < rows; ++row) {
        const int y = RulerHeight + row * RowHeight;
        const QModelIndex first = m_model->index(row, 0);
        const bool isCamera = first.data(StudioTimelineModel::LayerTypeRole).toInt() == Layer::CAMERA;
        const QColor keyColor = isCamera ? camera : accent;
        QColor holdColor = keyColor;
        holdColor.setAlphaF(0.22);

        if (first.data(StudioTimelineModel::IsActiveLayerRole).toBool()) {
            painter.fillRect(QRect(0, y, width(), RowHeight), activeRow);
        }
        painter.setPen(grid);
        painter.drawLine(0, y + RowHeight, width(), y + RowHeight);

        QColor nameColor = text;
        if (!first.data(StudioTimelineModel::IsVisibleRole).toBool()) nameColor.setAlphaF(0.4);
        painter.setPen(nameColor);
        const QString name = first.data(Qt::DisplayRole).toString();
        painter.drawText(QRect(6, y, NameWidth - 12, RowHeight), Qt::AlignVCenter | Qt::AlignLeft,
                         painter.fontMetrics().elidedText(name, Qt::ElideRight, NameWidth - 12));

        for (int column = 0; column < frames; ++column) {
            const QModelIndex index = m_model->index(row, column);
            const QRectF cell(NameWidth + column * FrameWidth, y, FrameWidth, RowHeight);

            if (index.data(StudioTimelineModel::IsHoldRole).toBool()) {
                painter.fillRect(cell.adjusted(0, 8, 0, -8), holdColor);
            }
            if (index.data(StudioTimelineModel::HasKeyframeRole).toBool()) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(keyColor);
                painter.drawPath(diamond(cell.center(), 6));
            }
        }
    }

    painter.setPen(grid);
    painter.drawLine(NameWidth, 0, NameWidth, height());

    const int x = playheadX();
    painter.setPen(QPen(text, 2));
    painter.drawLine(x, 0, x, height());
    painter.setBrush(text);
    painter.setPen(Qt::NoPen);
    painter.drawPath(diamond(QPointF(x, RulerHeight - 6), 5));
}

void TrackGrid::mousePressEvent(QMouseEvent *event)
{
    seekTo(event->position().toPoint(), true);
}

void TrackGrid::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        seekTo(event->position().toPoint(), false);
    }
}

void TrackGrid::seekTo(const QPoint &pos, bool selectRow)
{
    if (selectRow && pos.y() >= RulerHeight) {
        const int row = (pos.y() - RulerHeight) / RowHeight;
        if (Layer *layer = m_model->layerAt(row)) {
            m_core->layers()->setActiveLayer(layer);
        }
    }
    if (pos.x() >= NameWidth) {
        const int column = (pos.x() - NameWidth) / FrameWidth;
        const int frame = StudioTimelineModel::frameOfColumn(column);
        if (column >= 0 && column < m_model->frameCount() && frame != m_model->currentFrame()) {
            m_core->timeline()->setCurrentFrame(frame);
        }
    }
}

// --------------------------------------------------------------- TracksPanel

TracksPanel::TracksPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);

    QHBoxLayout *bar = new QHBoxLayout();
    bar->setSpacing(2);

    QToolButton *addKey = ShellWidgets::makeButton("timeline/frame-add", tr("Add a key here for the selected track"), this);
    connect(addKey, &QToolButton::clicked, this, [this]() {
        m_core->timeline()->addKeyAt(m_core->layers()->activeLayer(), m_core->timeline()->currentFrame());
    });

    QLabel *hint = new QLabel(tr("Pick the Camera track and the Camera tool to move, zoom and turn the view over time."), this);
    hint->setEnabled(false);

    bar->addWidget(new TransportBar(core, this));
    bar->addSpacing(12);
    bar->addWidget(addKey);
    bar->addStretch(1);
    bar->addWidget(hint);
    layout->addLayout(bar);

    m_grid = new TrackGrid(core, model, this);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(m_grid);
    m_scroll->setWidgetResizable(false);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setMinimumHeight(RulerHeight + 4 * RowHeight);
    layout->addWidget(m_scroll, 1);

    m_grid->resize(m_grid->sizeHint());

    connect(core->timeline(), &StudioTimeline::currentFrameChanged, this, [this]() {
        m_scroll->ensureVisible(m_grid->playheadX(), 0, 80, 0);
    });
}

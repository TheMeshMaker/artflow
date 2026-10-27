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

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"
#include "ShellWidgets.h"

namespace {
const int NameWidth = 160;
const int FrameWidth = 18;
const int RowHeight = 32;
const int RulerHeight = 26;

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
        setMinimumSize(sizeHint());
        update();
    });
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() { update(); });
}

QSize TrackGrid::sizeHint() const
{
    return QSize(NameWidth + m_model->frameCount() * FrameWidth + 1,
                 RulerHeight + qMax(1, m_model->rowCount()) * RowHeight + 1);
}

int TrackGrid::columnCount() const
{
    // The lanes run to the right edge, past the end of the scene.
    return qMax(m_model->frameCount(), (width() - NameWidth) / FrameWidth + 1);
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

    const QColor accent = ArtflowTheme::accent();
    const QColor camera = ArtflowTheme::camera();
    QColor tick = ArtflowTheme::line();
    const int frames = m_model->frameCount();
    const int rows = m_model->rowCount();
    const int gridRight = width() - 2;

    // Ruler: a tick per frame, a label every five.
    painter.setFont(ArtflowTheme::uiFont(8, QFont::Medium));
    for (int column = 0; column < columnCount(); ++column) {
        const int frame = StudioTimelineModel::frameOfColumn(column);
        const qreal x = NameWidth + column * FrameWidth + FrameWidth / 2.0;
        const bool major = frame % 5 == 0 || frame == 1;
        painter.setPen(QPen(major ? ArtflowTheme::textFaint() : tick, 1));
        painter.drawLine(QPointF(x, RulerHeight - (major ? 8 : 5)), QPointF(x, RulerHeight - 2));
        if (major) {
            painter.setPen(ArtflowTheme::textMuted());
            painter.drawText(QRectF(x - 14, 0, 28, RulerHeight - 9), Qt::AlignCenter, QString::number(frame));
        }
    }

    for (int row = 0; row < rows; ++row) {
        const int y = RulerHeight + row * RowHeight;
        const QModelIndex first = m_model->index(row, 0);
        const bool isCamera = first.data(StudioTimelineModel::LayerTypeRole).toInt() == Layer::CAMERA;
        const bool active = first.data(StudioTimelineModel::IsActiveLayerRole).toBool();
        const QColor keyColor = isCamera ? camera : accent;

        // Track lane
        const QRectF lane(2, y + 3, gridRight - 2, RowHeight - 6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(active ? ArtflowTheme::raised() : ArtflowTheme::panel().lighter(108));
        painter.drawRoundedRect(lane, 8, 8);
        if (active) {
            painter.setBrush(keyColor);
            painter.drawRoundedRect(QRectF(lane.left(), lane.top() + 6, 3, lane.height() - 12), 1.5, 1.5);
        }

        // Track name with its type icon
        const int type = first.data(StudioTimelineModel::LayerTypeRole).toInt();
        const QString iconName = type == Layer::CAMERA ? QStringLiteral("camera")
            : type == Layer::VECTOR ? QStringLiteral("layer-vector")
            : type == Layer::SOUND ? QStringLiteral("layer-sound") : QStringLiteral("layer-bitmap");
        const QIcon icon = isCamera ? ArtflowIcons::icon(iconName, camera) : ArtflowIcons::icon(iconName);
        icon.paint(&painter, QRect(12, y + RowHeight / 2 - 8, 16, 16), Qt::AlignCenter,
                   active ? QIcon::Active : QIcon::Normal);

        QColor nameColor = active ? ArtflowTheme::text() : ArtflowTheme::textMuted();
        if (!first.data(StudioTimelineModel::IsVisibleRole).toBool()) nameColor = ArtflowTheme::textFaint();
        painter.setPen(nameColor);
        painter.setFont(ArtflowTheme::uiFont(9, active ? QFont::DemiBold : QFont::Normal));
        const QString name = first.data(Qt::DisplayRole).toString();
        painter.drawText(QRect(36, y, NameWidth - 44, RowHeight), Qt::AlignVCenter | Qt::AlignLeft,
                         painter.fontMetrics().elidedText(name, Qt::ElideRight, NameWidth - 44));

        // Holds as a soft bar, keys as diamonds
        QColor holdColor = keyColor;
        holdColor.setAlphaF(0.22);
        for (int column = 0; column < frames; ++column) {
            const QModelIndex index = m_model->index(row, column);
            const QRectF cell(NameWidth + column * FrameWidth, y, FrameWidth, RowHeight);
            if (index.data(StudioTimelineModel::IsHoldRole).toBool()) {
                painter.fillRect(cell.adjusted(-0.5, RowHeight / 2.0 - 2, 0.5, -(RowHeight / 2.0 - 2)), holdColor);
            }
        }
        for (int column = 0; column < frames; ++column) {
            const QModelIndex index = m_model->index(row, column);
            if (!index.data(StudioTimelineModel::HasKeyframeRole).toBool()) continue;
            const QRectF cell(NameWidth + column * FrameWidth, y, FrameWidth, RowHeight);
            painter.setPen(QPen(ArtflowTheme::window(), 2));
            painter.setBrush(keyColor);
            painter.drawPath(diamond(cell.center(), 6.5));
        }
    }

    // Playhead: a line and a pill with the frame number.
    const qreal x = playheadX();
    painter.setPen(QPen(ArtflowTheme::text(), 1.5));
    painter.drawLine(QPointF(x, RulerHeight - 2), QPointF(x, height()));
    const QString number = QString::number(m_model->currentFrame());
    painter.setFont(ArtflowTheme::uiFont(8, QFont::Bold));
    const qreal pillWidth = qMax(22.0, painter.fontMetrics().horizontalAdvance(number) + 12.0);
    const QRectF pill(x - pillWidth / 2, 3, pillWidth, RulerHeight - 7);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ArtflowTheme::text());
    painter.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
    painter.setPen(ArtflowTheme::window());
    painter.drawText(pill, Qt::AlignCenter, number);
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
        if (column >= 0 && column < columnCount() && frame != m_model->currentFrame()) {
            m_core->timeline()->setCurrentFrame(frame);
        }
    }
}

// --------------------------------------------------------------- TracksPanel

TracksPanel::TracksPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
{
    setObjectName(QStringLiteral("artflowPanel"));
    setAttribute(Qt::WA_StyledBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 6);
    layout->setSpacing(2);

    QHBoxLayout *bar = new QHBoxLayout();
    bar->setSpacing(2);

    QToolButton *addKey = ShellWidgets::makeButton(QStringLiteral("key-add"), tr("Add a key here for the selected track"), this);
    connect(addKey, &QToolButton::clicked, this, [this]() {
        m_core->timeline()->addKeyAt(m_core->layers()->activeLayer(), m_core->timeline()->currentFrame());
    });

    QLabel *hint = new QLabel(tr("Pick the Camera track, then drag on the canvas to move, zoom or turn the view."), this);
    hint->setObjectName(QStringLiteral("muted"));

    bar->addWidget(addKey);
    bar->addSpacing(12);
    bar->addWidget(hint);
    bar->addStretch(1);
    bar->addWidget(new TransportBar(core, this));
    bar->addStretch(1);
    layout->addLayout(bar);

    m_grid = new TrackGrid(core, model, this);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(m_grid);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setMinimumHeight(RulerHeight + 4 * RowHeight);
    layout->addWidget(m_scroll, 1);

    m_grid->setMinimumSize(m_grid->sizeHint());

    connect(core->timeline(), &StudioTimeline::currentFrameChanged, this, [this]() {
        m_scroll->ensureVisible(m_grid->playheadX(), 0, 80, 0);
    });
}

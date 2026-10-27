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
#include <QPainterPath>
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

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"
#include "ShellWidgets.h"

namespace {
const int TileWidth = 68;
const int TileHeight = 50;
const int TileGap = 8;
const int Margin = 6;
const qreal Radius = 9.0;
}

// ---------------------------------------------------------------- FrameCells

FrameCells::FrameCells(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_model(model)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(TileHeight + 2 * Margin);
    connect(model, &QAbstractItemModel::modelReset, this, [this]() {
        m_thumbnails.clear();  // every reset follows an edit, an undo or a layer switch
        setMinimumWidth(sizeHint().width());
        update();
    });
    connect(model, &QAbstractItemModel::dataChanged, this, [this]() { update(); });
}

QSize FrameCells::sizeHint() const
{
    return QSize(Margin + m_model->frameCount() * (TileWidth + TileGap), TileHeight + 2 * Margin);
}

QRect FrameCells::cellRect(int frame) const
{
    const int column = StudioTimelineModel::columnOfFrame(frame);
    return QRect(Margin + column * (TileWidth + TileGap), Margin, TileWidth, TileHeight);
}

int FrameCells::columnCount() const
{
    // Empty tiles run to the right edge, past the end of the scene.
    return qMax(m_model->frameCount(), (width() - Margin) / (TileWidth + TileGap));
}

int FrameCells::frameAt(const QPoint &pos) const
{
    const int column = (pos.x() - Margin) / (TileWidth + TileGap);
    if (column < 0 || column >= columnCount()) return -1;
    const int frame = StudioTimelineModel::frameOfColumn(column);
    return cellRect(frame).contains(pos) ? frame : -1;
}

QPixmap FrameCells::thumbnail(int frame)
{
    auto it = m_thumbnails.constFind(frame);
    if (it != m_thumbnails.constEnd()) return *it;

    const qreal dpr = devicePixelRatioF();
    const QImage image = m_core->timeline()->thumbnail(m_model->layerAt(m_model->activeRow()), frame,
                                                       QSize(TileWidth, TileHeight) * dpr);
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(dpr);
    m_thumbnails.insert(frame, pixmap);
    return pixmap;
}

void FrameCells::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const int row = m_model->activeRow();
    const QColor accent = ArtflowTheme::accent();

    for (int column = 0; column < columnCount(); ++column) {
        const int frame = StudioTimelineModel::frameOfColumn(column);
        const QRect cellBounds = cellRect(frame);
        if (!event->rect().intersects(cellBounds.adjusted(-4, -4, 4, 4))) continue;

        const QRectF cell = QRectF(cellBounds).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath shape;
        shape.addRoundedRect(cell, Radius, Radius);

        bool key = false;
        bool hold = false;
        if (row >= 0 && column < m_model->frameCount()) {
            const QModelIndex index = m_model->index(row, column);
            key = index.data(StudioTimelineModel::HasKeyframeRole).toBool();
            hold = index.data(StudioTimelineModel::IsHoldRole).toBool();
        }
        const bool current = frame == m_model->currentFrame();
        const bool hovered = frame == m_hoveredFrame;

        if (key) {
            const QPixmap thumb = thumbnail(frame);
            painter.save();
            painter.setClipPath(shape);
            if (thumb.isNull()) {
                painter.fillPath(shape, Qt::white);
            } else {
                painter.drawPixmap(cell.toRect(), thumb);
            }
            painter.restore();
            painter.setPen(QPen(ArtflowTheme::line(), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(shape);
        } else if (hold) {
            // A held frame repeats the drawing before it: a quiet tile with a bar through it.
            painter.fillPath(shape, ArtflowTheme::raised());
            painter.setPen(QPen(ArtflowTheme::textFaint(), 2, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(cell.left() + 12, cell.center().y()), QPointF(cell.right() - 12, cell.center().y()));
        } else {
            painter.setPen(QPen(hovered ? ArtflowTheme::textFaint() : ArtflowTheme::line(), 1, Qt::DashLine));
            painter.setBrush(hovered ? QBrush(ArtflowTheme::raised()) : QBrush(Qt::NoBrush));
            painter.drawPath(shape);
        }

        if (hovered && key) {
            painter.fillPath(shape, QColor(255, 255, 255, 18));
        }

        if (current) {
            painter.setPen(QPen(accent, 2.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(cell.adjusted(-2.5, -2.5, 2.5, 2.5), Radius + 2.5, Radius + 2.5);
        }

        // Frame number in a small badge in the corner.
        const QString number = QString::number(frame);
        painter.setFont(ArtflowTheme::uiFont(7.5, QFont::DemiBold));
        const QRectF badge(cell.left() + 4, cell.bottom() - 17, qMax(16.0, painter.fontMetrics().horizontalAdvance(number) + 9.0), 13);
        if (key) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(current ? accent : QColor(15, 17, 22, 190));
            painter.drawRoundedRect(badge, 6.5, 6.5);
            painter.setPen(Qt::white);
        } else {
            painter.setPen(current ? ArtflowTheme::accentHover() : ArtflowTheme::textFaint());
        }
        painter.drawText(badge, Qt::AlignCenter, number);
    }
}

void FrameCells::mousePressEvent(QMouseEvent *event)
{
    const int frame = frameAt(event->position().toPoint());
    if (frame > 0) m_core->timeline()->setCurrentFrame(frame);
}

void FrameCells::mouseMoveEvent(QMouseEvent *event)
{
    const int frame = frameAt(event->position().toPoint());
    if (frame != m_hoveredFrame) {
        m_hoveredFrame = frame;
        update();
    }
}

void FrameCells::leaveEvent(QEvent *event)
{
    m_hoveredFrame = -1;
    update();
    QWidget::leaveEvent(event);
}

// ----------------------------------------------------------- FrameStripPanel

FrameStripPanel::FrameStripPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_model(model)
{
    setObjectName(QStringLiteral("artflowPanel"));
    setAttribute(Qt::WA_StyledBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 8, 14, 6);
    layout->setSpacing(4);

    QHBoxLayout *bar = new QHBoxLayout();
    bar->setSpacing(4);

    QLabel *layerIcon = new QLabel(this);
    layerIcon->setPixmap(ArtflowIcons::icon(QStringLiteral("layers")).pixmap(18, 18));
    m_layerLabel = new QLabel(this);
    m_layerLabel->setObjectName(QStringLiteral("panelTitle"));
    m_layerLabel->setToolTip(tr("Frames of this layer"));
    m_frameLabel = new QLabel(this);
    m_frameLabel->setObjectName(QStringLiteral("muted"));

    QToolButton *add = ShellWidgets::makeButton(QStringLiteral("frame-add"), tr("New frame after this one"), this);
    QToolButton *duplicate = ShellWidgets::makeButton(QStringLiteral("frame-duplicate"), tr("Copy this frame into the next one"), this);
    QToolButton *remove = ShellWidgets::makeButton(QStringLiteral("frame-delete"), tr("Delete this frame"), this);

    m_onion = new QToolButton(this);
    m_onion->setObjectName(QStringLiteral("pillButton"));
    m_onion->setIcon(ArtflowIcons::icon(QStringLiteral("onion")));
    m_onion->setText(tr("Onion skin"));
    m_onion->setToolTip(tr("See the frames before and after, faintly"));
    m_onion->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_onion->setIconSize(QSize(18, 18));
    m_onion->setCheckable(true);
    m_onion->setCursor(Qt::PointingHandCursor);

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

    bar->addWidget(layerIcon);
    bar->addWidget(m_layerLabel);
    bar->addSpacing(18);
    bar->addWidget(add);
    bar->addWidget(duplicate);
    bar->addWidget(remove);
    bar->addStretch(1);
    bar->addWidget(new TransportBar(core, this));
    bar->addStretch(1);
    bar->addWidget(m_onion);
    bar->addSpacing(14);
    bar->addWidget(m_frameLabel);
    layout->addLayout(bar);

    m_cells = new FrameCells(core, model, this);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(m_cells);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFixedHeight(m_cells->sizeHint().height() + 10);
    layout->addWidget(m_scroll);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(model, &QAbstractItemModel::modelReset, this, &FrameStripPanel::slotModelChanged);
    connect(timeline, &StudioTimeline::currentFrameChanged, this, &FrameStripPanel::slotCurrentFrameChanged);

    m_cells->setMinimumWidth(m_cells->sizeHint().width());
    {
        QSignalBlocker blocker(m_onion);
        m_onion->setChecked(layers->onionSkins());
    }
    slotModelChanged();
}

void FrameStripPanel::slotModelChanged()
{
    Layer *layer = m_model->layerAt(m_model->activeRow());
    m_layerLabel->setText(layer ? layer->name() : tr("No layer"));
    slotCurrentFrameChanged(m_model->currentFrame());
}

void FrameStripPanel::slotCurrentFrameChanged(int frame)
{
    m_frameLabel->setText(tr("Frame %1 of %2").arg(frame).arg(m_core->timeline()->lastUsedFrame()));
    m_scroll->ensureVisible(m_cells->cellRect(frame).center().x(), m_cells->height() / 2, 80, 0);
}

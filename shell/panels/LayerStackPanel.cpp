/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "LayerStackPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include "layer.h"

#include "StudioCore.h"
#include "StudioLayers.h"
#include "StudioTimelineModel.h"

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"
#include "ShellWidgets.h"

namespace {

const int RowHeight = 44;
const int IconSize = 18;

QIcon typeIcon(int type)
{
    switch (type) {
    case Layer::CAMERA: return ArtflowIcons::icon(QStringLiteral("camera"), ArtflowTheme::camera());
    case Layer::VECTOR: return ArtflowIcons::icon(QStringLiteral("layer-vector"));
    case Layer::SOUND: return ArtflowIcons::icon(QStringLiteral("layer-sound"));
    default: return ArtflowIcons::icon(QStringLiteral("layer-bitmap"));
    }
}

class LayerDelegate : public QStyledItemDelegate
{
public:
    LayerDelegate(StudioCore *core, StudioTimelineModel *model, QObject *parent)
        : QStyledItemDelegate(parent)
        , m_core(core)
        , m_model(model)
    {
    }

    static QRect eyeRect(const QStyleOptionViewItem &option)
    {
        const QRect r = option.rect;
        return QRect(r.left() + 14, r.center().y() - IconSize / 2, IconSize, IconSize);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), RowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const bool active = index.data(StudioTimelineModel::IsActiveLayerRole).toBool();
        const bool visible = index.data(StudioTimelineModel::IsVisibleRole).toBool();
        const bool hovered = option.state & QStyle::State_MouseOver;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QRectF card = QRectF(option.rect).adjusted(2, 3, -2, -3);
        if (active || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(active ? ArtflowTheme::raised() : ArtflowTheme::panel().lighter(115));
            painter->drawRoundedRect(card, 10, 10);
        }
        if (active) {
            painter->setBrush(ArtflowTheme::accent());
            painter->drawRoundedRect(QRectF(card.left(), card.top() + 9, 3, card.height() - 18), 1.5, 1.5);
        }

        ArtflowIcons::icon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off"))
            .paint(painter, eyeRect(option), Qt::AlignCenter, hovered || active ? QIcon::Active : QIcon::Normal);

        const QRect type(eyeRect(option).right() + 12, option.rect.center().y() - IconSize / 2, IconSize, IconSize);
        typeIcon(index.data(StudioTimelineModel::LayerTypeRole).toInt())
            .paint(painter, type, Qt::AlignCenter, visible ? QIcon::Normal : QIcon::Disabled);

        QColor text = active ? ArtflowTheme::text() : ArtflowTheme::textMuted();
        if (!visible) text = ArtflowTheme::textFaint();
        painter->setPen(text);
        painter->setFont(ArtflowTheme::uiFont(9.5, active ? QFont::DemiBold : QFont::Normal));
        const QRect textRect(type.right() + 10, option.rect.top(), option.rect.right() - type.right() - 16,
                             option.rect.height());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                                            textRect.width()));
        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override
    {
        Q_UNUSED(model);
        if (event->type() != QEvent::MouseButtonPress) return false;

        QMouseEvent *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton) return false;

        Layer *layer = m_model->layerAt(index.row());
        if (!layer) return false;

        if (eyeRect(option).adjusted(-4, -4, 4, 4).contains(mouse->position().toPoint())) {
            m_core->layers()->setVisible(layer, !m_core->layers()->isVisible(layer));
        } else {
            m_core->layers()->setActiveLayer(layer);
        }
        return true;
    }

private:
    StudioCore *m_core;
    StudioTimelineModel *m_model;
};

}

LayerStackPanel::LayerStackPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_model(model)
{
    setObjectName(QStringLiteral("artflowPanel"));
    setAttribute(Qt::WA_StyledBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(4);

    QHBoxLayout *bar = new QHBoxLayout();
    QLabel *title = new QLabel(tr("Layers"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    QToolButton *add = ShellWidgets::makeButton(QStringLiteral("layer-add"), tr("New drawing layer"), this);
    QToolButton *remove = ShellWidgets::makeButton(QStringLiteral("layer-delete"), tr("Delete layer"), this);
    connect(add, &QToolButton::clicked, this, [this]() { m_core->layers()->addDrawingLayer(); });
    connect(remove, &QToolButton::clicked, this, [this]() { m_core->layers()->removeActiveLayer(); });
    bar->addWidget(title);
    bar->addStretch(1);
    bar->addWidget(add);
    bar->addWidget(remove);
    layout->addLayout(bar);

    m_list = new QListView(this);
    m_list->setModel(model);
    m_list->setModelColumn(0);
    m_list->setItemDelegate(new LayerDelegate(core, model, m_list));
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_list, 1);

    connect(model, &QAbstractItemModel::modelReset, this, &LayerStackPanel::slotModelReset);
    setMinimumWidth(250);
    slotModelReset();
}

void LayerStackPanel::slotModelReset()
{
    // Up to six layers show without scrolling, and the panel ends right
    // below the last one.
    const QMargins margins = layout()->contentsMargins();
    const int header = layout()->itemAt(0)->sizeHint().height();
    const int rows = qBound(2, m_model->rowCount(), 6);
    const int height = margins.top() + header + layout()->spacing() + rows * RowHeight + margins.bottom() + 4;
    if (height != maximumHeight()) {
        setMaximumHeight(height);
        Q_EMIT heightLimitChanged();
    }

    const int row = m_model->activeRow();
    if (row >= 0) m_list->scrollTo(m_model->index(row, 0));
}

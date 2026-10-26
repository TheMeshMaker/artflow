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

#include "ShellWidgets.h"

namespace {

const int RowHeight = 34;
const int IconSize = 18;

QString typeIcon(int type)
{
    switch (type) {
    case Layer::CAMERA: return QStringLiteral(":/icons/themes/playful/timeline/cell-camera.svg");
    case Layer::VECTOR: return QStringLiteral(":/icons/themes/playful/timeline/cell-vector.svg");
    case Layer::SOUND: return QStringLiteral(":/icons/themes/playful/timeline/cell-sound.svg");
    default: return QStringLiteral(":/icons/themes/playful/timeline/cell-bitmap.svg");
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
        return QRect(r.left() + 8, r.center().y() - IconSize / 2, IconSize, IconSize);
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), RowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QPalette pal = option.palette;
        const bool active = index.data(StudioTimelineModel::IsActiveLayerRole).toBool();
        const bool visible = index.data(StudioTimelineModel::IsVisibleRole).toBool();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (active) {
            painter->fillRect(option.rect, pal.color(QPalette::Highlight));
        } else if (option.state & QStyle::State_MouseOver) {
            QColor hover = pal.color(QPalette::Highlight);
            hover.setAlphaF(0.15);
            painter->fillRect(option.rect, hover);
        }

        QColor text = pal.color(active ? QPalette::HighlightedText : QPalette::WindowText);

        // Eye: a filled dot when visible, an empty ring when hidden.
        const QRectF eye = QRectF(eyeRect(option)).adjusted(3, 3, -3, -3);
        painter->setPen(QPen(text, 1.6));
        painter->setBrush(visible ? QBrush(text) : Qt::NoBrush);
        painter->drawEllipse(eye);

        const QRect type(eyeRect(option).right() + 10, option.rect.center().y() - IconSize / 2, IconSize, IconSize);
        QIcon(typeIcon(index.data(StudioTimelineModel::LayerTypeRole).toInt())).paint(painter, type);

        if (!visible) text.setAlphaF(0.5);
        painter->setPen(text);
        const QRect textRect(type.right() + 8, option.rect.top(), option.rect.right() - type.right() - 12,
                             option.rect.height());
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          option.fontMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
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
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    QHBoxLayout *bar = new QHBoxLayout();
    QLabel *title = new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Layers")), this);
    QToolButton *add = ShellWidgets::makeButton("timeline/layer-add", tr("New drawing layer"), this);
    QToolButton *remove = ShellWidgets::makeButton("timeline/layer-remove", tr("Delete layer"), this);
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
    setMinimumWidth(200);
}

void LayerStackPanel::slotModelReset()
{
    const int row = m_model->activeRow();
    if (row >= 0) m_list->scrollTo(m_model->index(row, 0));
}

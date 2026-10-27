/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "LayerStackPanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

#include "keyframe.h"
#include "layer.h"

#include "StudioCore.h"
#include "StudioLayers.h"
#include "StudioTimeline.h"
#include "StudioTimelineModel.h"

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"
#include "ShellControls.h"
#include "ShellWidgets.h"

namespace {

const int RowHeight = 52;
const int IconSize = 18;
const QSize ThumbSize(48, 30);

QIcon typeIcon(int type)
{
    switch (type) {
    case Layer::CAMERA: return ArtflowIcons::icon(QStringLiteral("camera"), ArtflowTheme::camera());
    case Layer::VECTOR: return ArtflowIcons::icon(QStringLiteral("layer-vector"));
    case Layer::SOUND: return ArtflowIcons::icon(QStringLiteral("layer-sound"));
    default: return ArtflowIcons::icon(QStringLiteral("layer-bitmap"));
    }
}

QRect eyeRect(const QRect &row)
{
    return QRect(row.left() + 12, row.center().y() - IconSize / 2, IconSize, IconSize);
}

QRect thumbRect(const QRect &row)
{
    return QRect(eyeRect(row).right() + 10, row.center().y() - ThumbSize.height() / 2, ThumbSize.width(), ThumbSize.height());
}

QRect nameRect(const QRect &row)
{
    const int left = thumbRect(row).right() + 10;
    return QRect(left, row.top() + 8, row.right() - left - 8, row.height() - 16);
}

bool hasDrawing(int type)
{
    return type == Layer::BITMAP || type == Layer::VECTOR;
}

class LayerDelegate : public QStyledItemDelegate
{
public:
    explicit LayerDelegate(LayerList *list)
        : QStyledItemDelegate(list)
        , m_list(list)
    {
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
        const int type = index.data(StudioTimelineModel::LayerTypeRole).toInt();
        StudioTimelineModel *model = static_cast<StudioTimelineModel *>(m_list->model());
        Layer *layer = model->layerAt(index.row());

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
            painter->drawRoundedRect(QRectF(card.left(), card.top() + 11, 3, card.height() - 22), 1.5, 1.5);
        }

        ArtflowIcons::icon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off"))
            .paint(painter, eyeRect(option.rect), Qt::AlignCenter, hovered || active ? QIcon::Active : QIcon::Normal);

        // The drawing on this frame, small; the camera and sound get their sign instead.
        const QRect thumb = thumbRect(option.rect);
        QPainterPath frame;
        frame.addRoundedRect(QRectF(thumb).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
        if (layer && hasDrawing(type)) {
            painter->save();
            painter->setClipPath(frame);
            painter->setOpacity(visible ? 1.0 : 0.4);
            painter->drawPixmap(thumb, m_list->thumbnail(layer, thumb.size()));
            painter->restore();
            painter->setPen(QPen(active ? ArtflowTheme::accent() : ArtflowTheme::line(), active ? 1.4 : 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(frame);
        } else {
            painter->setPen(Qt::NoPen);
            painter->setBrush(ArtflowTheme::window());
            painter->drawPath(frame);
            const QRect sign(thumb.center().x() - IconSize / 2 + 1, thumb.center().y() - IconSize / 2 + 1, IconSize, IconSize);
            typeIcon(type).paint(painter, sign, Qt::AlignCenter, visible ? QIcon::Normal : QIcon::Disabled);
        }

        QColor text = active ? ArtflowTheme::text() : ArtflowTheme::textMuted();
        if (!visible) text = ArtflowTheme::textFaint();
        const QRect names = nameRect(option.rect);
        QString detail;
        if (type == Layer::CAMERA) detail = QObject::tr("Camera");
        else if (type == Layer::SOUND) detail = QObject::tr("Sound");
        else if (index.data(StudioTimelineModel::OpacityRole).toReal() < 0.995)
            detail = QObject::tr("Opacity %1%").arg(qRound(index.data(StudioTimelineModel::OpacityRole).toReal() * 100));
        else if (type == Layer::VECTOR) detail = QObject::tr("Vector");

        painter->setPen(text);
        painter->setFont(ArtflowTheme::uiFont(9.5, active ? QFont::DemiBold : QFont::Normal));
        const QRect nameLine = detail.isEmpty() ? names : names.adjusted(0, 0, 0, -names.height() / 2);
        painter->drawText(nameLine, (detail.isEmpty() ? Qt::AlignVCenter : Qt::AlignBottom) | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                                            names.width()));
        if (!detail.isEmpty()) {
            painter->setPen(ArtflowTheme::textFaint());
            painter->setFont(ArtflowTheme::uiFont(8, QFont::Normal));
            painter->drawText(names.adjusted(0, names.height() / 2 + 1, 0, 0), Qt::AlignTop | Qt::AlignLeft, detail);
        }
        painter->restore();
    }

private:
    LayerList *m_list;
};

}

// ----------------------------------------------------------------- LayerList

LayerList::LayerList(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QListView(parent)
    , m_core(core)
    , m_model(model)
{
    setModel(model);
    setModelColumn(0);
    setItemDelegate(new LayerDelegate(this));
    setMouseTracking(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFrameShape(QFrame::NoFrame);

    // Every reset follows an edit, an undo, a frame or a layer change.
    connect(model, &QAbstractItemModel::modelReset, this, [this]() { m_thumbnails.clear(); });
    connect(core->timeline(), &StudioTimeline::currentFrameChanged, this, [this]() {
        m_thumbnails.clear();
        viewport()->update();
    });
}

QPixmap LayerList::thumbnail(Layer *layer, const QSize &size)
{
    if (!m_core->layers()->contains(layer)) return QPixmap();
    auto it = m_thumbnails.constFind(layer);
    if (it != m_thumbnails.constEnd()) return *it;

    // The drawing seen on this frame, which may be held from an earlier one;
    // blank paper when there is none yet.
    const qreal dpr = devicePixelRatioF();
    const KeyFrame *shown = layer->getKeyFrameWhichCovers(m_core->timeline()->currentFrame());
    QImage image = shown ? m_core->timeline()->thumbnail(layer, shown->pos(), size * dpr) : QImage();
    if (image.isNull()) {
        image = QImage(size * dpr, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
    }
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(dpr);
    m_thumbnails.insert(layer, pixmap);
    return pixmap;
}

int LayerList::gapAt(int y) const
{
    const int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QRect rect = visualRect(m_model->index(row, 0));
        if (y < rect.center().y()) return row;
    }
    return rows;
}

void LayerList::mousePressEvent(QMouseEvent *event)
{
    m_pressRow = -1;
    if (event->button() != Qt::LeftButton) return;
    const QModelIndex index = indexAt(event->position().toPoint());
    Layer *layer = index.isValid() ? m_model->layerAt(index.row()) : nullptr;
    if (!m_core->layers()->contains(layer)) return;

    if (eyeRect(visualRect(index)).adjusted(-5, -5, 5, 5).contains(event->position().toPoint())) {
        m_core->layers()->setVisible(layer, !m_core->layers()->isVisible(layer));
        return;
    }
    m_core->layers()->setActiveLayer(layer);
    m_pressRow = index.row();
    m_pressPos = event->position().toPoint();
}

void LayerList::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressRow >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragging && (event->position().toPoint() - m_pressPos).manhattanLength() > 6) m_dragging = true;
        if (m_dragging) {
            const int gap = gapAt(event->position().toPoint().y());
            if (gap != m_dropGap) {
                m_dropGap = gap;
                viewport()->update();
            }
            return;
        }
    }
    QListView::mouseMoveEvent(event);
}

void LayerList::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_dragging && m_pressRow >= 0) {
        Layer *layer = m_model->layerAt(m_pressRow);
        if (!m_core->layers()->contains(layer)) layer = nullptr;
        // Below its own place, the layer passes one row less than the gap says.
        const int row = m_dropGap > m_pressRow ? m_dropGap - 1 : m_dropGap;
        m_dragging = false;
        m_dropGap = -1;
        m_pressRow = -1;
        viewport()->update();
        if (layer && row >= 0) m_core->layers()->moveTo(layer, row);
        return;
    }
    m_dragging = false;
    m_dropGap = -1;
    m_pressRow = -1;
}

void LayerList::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QModelIndex index = indexAt(event->position().toPoint());
    if (index.isValid() && !eyeRect(visualRect(index)).adjusted(-5, -5, 5, 5).contains(event->position().toPoint())) {
        rename(index.row());
    }
}

void LayerList::rename(int row)
{
    Layer *layer = m_model->layerAt(row);
    if (!m_core->layers()->contains(layer)) return;
    delete m_editor;

    QLineEdit *editor = new QLineEdit(layer->name(), viewport());
    editor->setObjectName(QStringLiteral("layerNameEditor"));
    editor->setGeometry(nameRect(visualRect(m_model->index(row, 0))).adjusted(-6, 0, 0, 0));
    editor->selectAll();
    m_editor = editor;

    QAction *escape = new QAction(editor);
    escape->setShortcut(Qt::Key_Escape);
    escape->setShortcutContext(Qt::WidgetShortcut);
    editor->addAction(escape);
    connect(escape, &QAction::triggered, editor, [editor]() {
        editor->blockSignals(true);
        editor->deleteLater();
    });
    connect(editor, &QLineEdit::editingFinished, this, [this, editor, layer]() {
        const QString name = editor->text();
        editor->blockSignals(true);
        editor->deleteLater();
        if (m_core->layers()->contains(layer)) m_core->layers()->rename(layer, name);
    });
    editor->show();
    editor->setFocus();
}

void LayerList::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (!m_dragging || m_dropGap < 0) return;

    // Where the dragged layer will land.
    const int rows = m_model->rowCount();
    int y = 0;
    if (rows == 0) return;
    if (m_dropGap >= rows) {
        y = visualRect(m_model->index(rows - 1, 0)).bottom() - 1;
    } else {
        y = visualRect(m_model->index(m_dropGap, 0)).top() + 1;
    }
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(ArtflowTheme::accent(), 2.4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(8, y), QPointF(viewport()->width() - 8, y));
    painter.setBrush(ArtflowTheme::accent());
    painter.drawEllipse(QPointF(8, y), 3.5, 3.5);
}

// ----------------------------------------------------------- LayerStackPanel

LayerStackPanel::LayerStackPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent)
    : QWidget(parent)
    , m_core(core)
    , m_model(model)
{
    setObjectName(QStringLiteral("artflowPanel"));
    setAttribute(Qt::WA_StyledBackground);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 10);
    layout->setSpacing(4);

    QHBoxLayout *bar = new QHBoxLayout();
    QLabel *title = new QLabel(tr("Layers"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    QToolButton *add = ShellWidgets::makeButton(QStringLiteral("layer-add"), tr("New drawing layer"), this);
    QToolButton *copy = ShellWidgets::makeButton(QStringLiteral("layer-duplicate"), tr("Copy this layer with all its drawings"), this);
    QToolButton *remove = ShellWidgets::makeButton(QStringLiteral("layer-delete"), tr("Delete layer"), this);
    add->setObjectName(QStringLiteral("layerAdd"));
    copy->setObjectName(QStringLiteral("layerDuplicate"));
    connect(add, &QToolButton::clicked, this, [this]() { m_core->layers()->addDrawingLayer(); });
    connect(copy, &QToolButton::clicked, this, [this]() { m_core->layers()->duplicateActiveLayer(); });
    connect(remove, &QToolButton::clicked, this, [this]() { m_core->layers()->removeActiveLayer(); });
    bar->addWidget(title);
    bar->addStretch(1);
    bar->addWidget(add);
    bar->addWidget(copy);
    bar->addWidget(remove);
    layout->addLayout(bar);

    m_list = new LayerList(core, model, this);
    m_list->setToolTip(tr("Click to choose a layer, drag it to reorder, double-click to rename"));
    layout->addWidget(m_list, 1);

    m_opacity = new ValueSlider(tr("Layer opacity"), 0, 100, 100, this);
    m_opacity->setObjectName(QStringLiteral("layerOpacity"));
    m_opacity->setSuffix(QStringLiteral("%"));
    connect(m_opacity, &ValueSlider::valueChanged, this, [this](int value) {
        m_core->layers()->setOpacity(m_core->layers()->activeLayer(), value / 100.0);
        m_list->viewport()->update();
    });
    layout->addSpacing(2);
    layout->addWidget(m_opacity);

    connect(model, &QAbstractItemModel::modelReset, this, &LayerStackPanel::slotModelReset);
    connect(core->layers(), &StudioLayers::activeLayerChanged, this, &LayerStackPanel::slotModelReset);
    setMinimumWidth(250);
    slotModelReset();
}

void LayerStackPanel::slotModelReset()
{
    Layer *active = m_core->layers()->activeLayer();
    const bool drawing = active && hasDrawing(active->type());
    m_opacity->setVisible(drawing);
    if (drawing) m_opacity->setValue(qRound(active->opacity() * 100));

    // Up to six layers show without scrolling, and the panel ends right
    // below the last one.
    QVBoxLayout *box = static_cast<QVBoxLayout *>(layout());
    const QMargins margins = box->contentsMargins();
    const int header = box->itemAt(0)->sizeHint().height();
    const int rows = qBound(2, m_model->rowCount(), 6);
    const int slider = drawing ? m_opacity->sizeHint().height() + 2 + box->spacing() : 0;
    const int height = margins.top() + header + box->spacing() + rows * RowHeight + slider + margins.bottom() + 4;
    if (height != maximumHeight()) {
        setMaximumHeight(height);
        Q_EMIT heightLimitChanged();
    }

    const int row = m_model->activeRow();
    if (row >= 0) m_list->scrollTo(m_model->index(row, 0));
}

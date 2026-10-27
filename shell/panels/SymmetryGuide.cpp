/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "SymmetryGuide.h"

#include <QEvent>
#include <QPainter>

#include "editor.h"
#include "layercamera.h"
#include "object.h"
#include "viewmanager.h"

#include "StudioBrushes.h"
#include "StudioCore.h"

#include "ArtflowTheme.h"

SymmetryGuide::SymmetryGuide(StudioCore *core, QWidget *canvas)
    : QWidget(canvas)
    , m_core(core)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setGeometry(canvas->rect());
    canvas->installEventFilter(this);

    connect(core->editor()->view(), &ViewManager::viewChanged, this, qOverload<>(&QWidget::update));
    connect(core->brushes(), &StudioBrushes::symmetryChanged, this, [this](bool on) {
        setVisible(on);
        raise();
        update();
    });
    setVisible(core->brushes()->symmetry());
}

bool SymmetryGuide::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parent() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
    }
    return QWidget::eventFilter(watched, event);
}

void SymmetryGuide::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    const Object *object = m_core->editor()->object();
    if (!object) return;
    const std::vector<LayerCamera *> cameras = object->getLayersByType<LayerCamera>();
    if (cameras.empty()) return;

    // The line runs the height of the page, through its middle.
    const QRectF page = cameras.front()->getViewRect();
    const qreal x = m_core->brushes()->symmetryAxis();
    ViewManager *view = m_core->editor()->view();
    const QPointF top = view->mapCanvasToScreen(QPointF(x, page.top()));
    const QPointF bottom = view->mapCanvasToScreen(QPointF(x, page.bottom()));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor accent = ArtflowTheme::accent();
    accent.setAlpha(200);
    QPen pen(accent, 1.6, Qt::CustomDashLine, Qt::RoundCap);
    pen.setDashPattern({4, 4});
    painter.setPen(pen);
    painter.drawLine(top, bottom);
}

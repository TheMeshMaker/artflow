/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ViewControls.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QToolButton>
#include <QtMath>

#include "editor.h"
#include "viewmanager.h"

#include "StudioCore.h"

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"
#include "ShellWidgets.h"

ViewControls::ViewControls(StudioCore *core, QWidget *canvas)
    : QWidget(canvas)
    , m_core(core)
{
    setObjectName(QStringLiteral("viewControls"));
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(0);

    ViewManager *view = core->editor()->view();
    QToolButton *out = ShellWidgets::makeButton(QStringLiteral("minus"), tr("Zoom out  (Ctrl+-)"), this);
    m_zoom = new QToolButton(this);
    m_zoom->setObjectName(QStringLiteral("zoomLabel"));
    m_zoom->setToolTip(tr("Show at 100 %"));
    m_zoom->setCursor(Qt::PointingHandCursor);
    m_zoom->setFixedWidth(58);
    QToolButton *in = ShellWidgets::makeButton(QStringLiteral("plus"), tr("Zoom in  (Ctrl++)"), this);
    QToolButton *fit = ShellWidgets::makeButton(QStringLiteral("fit"), tr("Fit the page to the window  (Ctrl+0)"), this);
    m_straighten = ShellWidgets::makeButton(QStringLiteral("rotate"), tr("Straighten the page"), this);
    for (QToolButton *button : {out, in, fit, m_straighten}) button->setIconSize(QSize(18, 18));

    connect(out, &QToolButton::clicked, view, &ViewManager::scaleDown);
    connect(in, &QToolButton::clicked, view, &ViewManager::scaleUp);
    connect(m_zoom, &QToolButton::clicked, view, &ViewManager::scale100);
    connect(fit, &QToolButton::clicked, core, &StudioCore::fitView);
    connect(m_straighten, &QToolButton::clicked, view, &ViewManager::resetRotation);
    connect(view, &ViewManager::viewChanged, this, &ViewControls::refresh);

    layout->addWidget(out);
    layout->addWidget(m_zoom);
    layout->addWidget(in);
    layout->addSpacing(4);
    layout->addWidget(fit);
    layout->addWidget(m_straighten);

    canvas->installEventFilter(this);
    refresh();
    show();
}

bool ViewControls::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parent() && event->type() == QEvent::Resize) place();
    return QWidget::eventFilter(watched, event);
}

void ViewControls::refresh()
{
    ViewManager *view = m_core->editor()->view();
    m_zoom->setText(QStringLiteral("%1 %").arg(qRound(view->scaling() * 100)));
    const qreal turn = std::fmod(qAbs(view->rotation()), 360.0);
    m_straighten->setVisible(turn > 0.01 && turn < 359.99);
    adjustSize();
    place();
}

void ViewControls::place()
{
    QWidget *canvas = parentWidget();
    if (!canvas) return;
    move(canvas->width() - width() - 14, canvas->height() - height() - 14);
    raise();
}

void ViewControls::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor fill = ArtflowTheme::panel();
    fill.setAlpha(235);
    painter.setPen(QPen(ArtflowTheme::line(), 1));
    painter.setBrush(fill);
    const QRectF pill = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
}

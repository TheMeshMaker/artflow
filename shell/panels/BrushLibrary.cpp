/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "BrushLibrary.h"

#include <QButtonGroup>
#include <QEnterEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

#include "StudioBrushes.h"
#include "StudioCore.h"

#include "ArtflowTheme.h"

namespace {
const int CardWidth = 196;
const int CardHeight = 74;
const int NameHeight = 22;
const int Shadow = 14;
}

// ----------------------------------------------------------------- BrushCard

BrushCard::BrushCard(const BrushPreset &preset, QWidget *parent)
    : QAbstractButton(parent)
    , m_preset(preset)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(preset.name);
    setFixedSize(sizeHint());
}

QString BrushCard::presetId() const
{
    return m_preset.id;
}

QSize BrushCard::sizeHint() const
{
    return QSize(CardWidth, CardHeight);
}

void BrushCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
    const QColor accent = ArtflowTheme::accent();
    painter.setPen(isChecked() ? QPen(accent, 1.6) : QPen(ArtflowTheme::line(), 1));
    painter.setBrush(isChecked() ? ArtflowTheme::accentSoft() : (m_hovered ? ArtflowTheme::hover() : ArtflowTheme::raised()));
    painter.drawRoundedRect(card, 11, 11);

    painter.setFont(ArtflowTheme::uiFont(8.5, isChecked() ? QFont::DemiBold : QFont::Medium));
    painter.setPen(isChecked() ? ArtflowTheme::text() : ArtflowTheme::textMuted());
    painter.drawText(QRectF(card.left() + 11, card.top() + 4, card.width() - 22, NameHeight), Qt::AlignVCenter | Qt::AlignLeft,
                     m_preset.name);

    // The sample stroke is drawn once per screen scale and kept.
    const QRect area = QRect(8, NameHeight + 2, width() - 16, height() - NameHeight - 8);
    const qreal dpr = devicePixelRatioF();
    if (m_preview.isNull() || !qFuzzyCompare(m_preview.devicePixelRatio(), dpr)) {
        m_preview = StudioBrushes::preview(m_preset, area.size(), dpr, ArtflowTheme::text());
    }
    painter.drawImage(area.topLeft(), m_preview);
}

void BrushCard::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void BrushCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

// -------------------------------------------------------------- BrushLibrary

BrushLibrary::BrushLibrary(StudioCore *core, QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_core(core)
{
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Shadow + 14, Shadow + 12, Shadow + 14, Shadow + 14);
    outer->setSpacing(0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_body = new QWidget(scroll);
    scroll->setWidget(m_body);
    outer->addWidget(scroll);
}

void BrushLibrary::popup(ToolType tool, const QPoint &globalPos)
{
    m_tool = tool == PENCIL ? PENCIL : BRUSH;
    rebuild();

    // As tall as the styles need, as far as the screen allows.
    const QScreen *screen = QGuiApplication::screenAt(globalPos);
    const QRect available = screen ? screen->availableGeometry() : QRect(globalPos, QSize(800, 800));
    const QMargins margins = layout()->contentsMargins();
    const QSize content = m_body->sizeHint();
    const int width = content.width() + margins.left() + margins.right() + 10;
    const int height = qMin(content.height() + margins.top() + margins.bottom(), available.height() - 40);
    resize(width, height);

    QPoint pos = globalPos - QPoint(Shadow, Shadow);
    pos.setX(qBound(available.left(), pos.x(), available.right() - width));
    pos.setY(qBound(available.top(), pos.y(), available.bottom() - height));
    move(pos);
    show();
}

void BrushLibrary::rebuild()
{
    // Cards, labels, the button group and the layout of the last showing.
    qDeleteAll(m_body->children());

    QVBoxLayout *layout = new QVBoxLayout(m_body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    QLabel *title = new QLabel(m_tool == PENCIL ? tr("Pencils") : tr("Brushes"), m_body);
    title->setObjectName(QStringLiteral("panelTitle"));
    title->setFont(ArtflowTheme::uiFont(11, QFont::DemiBold));
    layout->addWidget(title);
    QLabel *hint = new QLabel(tr("Each style keeps its own size."), m_body);
    hint->setObjectName(QStringLiteral("muted"));
    layout->addWidget(hint);

    StudioBrushes *brushes = m_core->brushes();
    const QString current = brushes->current(m_tool).id;
    QButtonGroup *group = new QButtonGroup(m_body);

    QString family;
    QGridLayout *grid = nullptr;
    int index = 0;
    for (const BrushPreset &preset : brushes->presetsFor(m_tool)) {
        if (preset.family != family) {
            family = preset.family;
            layout->addSpacing(8);
            QLabel *section = new QLabel(StudioBrushes::familyTitle(family).toUpper(), m_body);
            section->setObjectName(QStringLiteral("muted"));
            section->setFont(ArtflowTheme::uiFont(7.5, QFont::DemiBold));
            layout->addWidget(section);
            grid = new QGridLayout();
            grid->setHorizontalSpacing(8);
            grid->setVerticalSpacing(8);
            layout->addLayout(grid);
            index = 0;
        }

        BrushCard *card = new BrushCard(preset, m_body);
        card->setChecked(preset.id == current);
        group->addButton(card);
        grid->addWidget(card, index / 2, index % 2);
        ++index;

        connect(card, &BrushCard::clicked, this, [this, brushes, id = preset.id]() {
            brushes->select(id);
            close();
        });
    }
    layout->addStretch(1);
    m_body->adjustSize();
}

void BrushLibrary::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // A soft shadow under the panel, made of a few widening rings.
    const QRectF panel = QRectF(rect()).adjusted(Shadow, Shadow, -Shadow, -Shadow);
    for (int i = Shadow; i > 0; i -= 2) {
        QColor shade(0, 0, 0, int(26 * (1.0 - qreal(i) / Shadow)));
        painter.setPen(Qt::NoPen);
        painter.setBrush(shade);
        painter.drawRoundedRect(panel.adjusted(-i, -i + 4, i, i + 4), 16 + i, 16 + i);
    }

    painter.setPen(QPen(ArtflowTheme::line(), 1));
    painter.setBrush(ArtflowTheme::panel());
    painter.drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
}

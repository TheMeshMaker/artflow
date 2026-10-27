/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "HomeScreen.h"

#include <QDateTime>
#include <QDir>
#include <QEnterEvent>
#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include "mainwindow2.h"
#include "pencildef.h"

#include "ArtflowIcons.h"
#include "ArtflowTheme.h"

namespace {
const int CardWidth = 150;
const int CardHeight = 150;
}

// ------------------------------------------------------------------ PageCard

PageCard::PageCard(const QString &name, const QSize &page, QWidget *parent)
    : QAbstractButton(parent)
    , m_page(page)
{
    setText(name);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("A new drawing, %1 x %2").arg(page.width()).arg(page.height()));
    setFixedSize(sizeHint());
}

QSize PageCard::sizeHint() const
{
    return QSize(CardWidth, CardHeight);
}

void PageCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(QPen(m_hovered ? ArtflowTheme::accent() : ArtflowTheme::line(), m_hovered ? 1.6 : 1));
    painter.setBrush(m_hovered ? ArtflowTheme::hover() : ArtflowTheme::raised());
    painter.drawRoundedRect(card, 14, 14);

    // The page itself, in its shape.
    const QRectF area(card.left() + 24, card.top() + 18, card.width() - 48, card.height() - 70);
    const qreal scale = qMin(area.width() / m_page.width(), area.height() / m_page.height());
    QRectF page(0, 0, m_page.width() * scale, m_page.height() * scale);
    page.moveCenter(area.center());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawRoundedRect(page.translated(0, 2), 4, 4);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(page, 4, 4);

    painter.setPen(ArtflowTheme::text());
    painter.setFont(ArtflowTheme::uiFont(9.5, QFont::DemiBold));
    painter.drawText(QRectF(card.left(), card.bottom() - 46, card.width(), 20), Qt::AlignCenter, text());
    painter.setPen(ArtflowTheme::textMuted());
    painter.setFont(ArtflowTheme::uiFont(8));
    painter.drawText(QRectF(card.left(), card.bottom() - 26, card.width(), 18), Qt::AlignCenter,
                     QStringLiteral("%1 x %2").arg(m_page.width()).arg(m_page.height()));
}

void PageCard::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void PageCard::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

// ---------------------------------------------------------------- HomeScreen

HomeScreen::HomeScreen(MainWindow2 *window)
    : QWidget(window)
    , m_window(window)
{
    setObjectName(QStringLiteral("homeScreen"));
    setAttribute(Qt::WA_StyledBackground);
    window->installEventFilter(this);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *body = new QWidget(scroll);
    scroll->setWidget(body);
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    QHBoxLayout *centre = new QHBoxLayout(body);
    centre->addStretch(1);
    QWidget *column = new QWidget(body);
    column->setMaximumWidth(760);
    centre->addWidget(column, 4);
    centre->addStretch(1);

    QVBoxLayout *layout = new QVBoxLayout(column);
    layout->setContentsMargins(24, 48, 24, 32);
    layout->setSpacing(10);

    QLabel *title = new QLabel(QStringLiteral("ArtFlow"), column);
    title->setFont(ArtflowTheme::uiFont(26, QFont::Bold));
    layout->addWidget(title);
    QLabel *tagline = new QLabel(tr("Draw, animate, and learn as you go."), column);
    tagline->setObjectName(QStringLiteral("muted"));
    tagline->setFont(ArtflowTheme::uiFont(11));
    layout->addWidget(tagline);
    layout->addSpacing(22);

    QLabel *newTitle = new QLabel(tr("New drawing"), column);
    newTitle->setObjectName(QStringLiteral("panelTitle"));
    newTitle->setFont(ArtflowTheme::uiFont(11, QFont::DemiBold));
    layout->addWidget(newTitle);
    QHBoxLayout *cards = new QHBoxLayout();
    cards->setSpacing(12);
    const QList<QPair<QString, QSize>> pages = {
        {tr("Wide (HD)"), QSize(1920, 1080)},
        {tr("Square"), QSize(1080, 1080)},
        {tr("Tall (phone)"), QSize(1080, 1920)},
        {tr("Small"), QSize(1280, 720)},
    };
    for (const auto &page : pages) {
        PageCard *card = new PageCard(page.first, page.second, column);
        cards->addWidget(card);
        const QSize size = page.second;
        connect(card, &PageCard::clicked, this, [this, size]() {
            // New drawings take their page size from these settings.
            QSettings settings(PENCIL2D, PENCIL2D);
            settings.setValue(QStringLiteral("FieldW"), size.width());
            settings.setValue(QStringLiteral("FieldH"), size.height());
            m_window->newDocument();
            finish();
        });
    }
    cards->addStretch(1);
    layout->addLayout(cards);
    layout->addSpacing(16);

    QHBoxLayout *buttons = new QHBoxLayout();
    QPushButton *openButton = new QPushButton(ArtflowIcons::icon(QStringLiteral("image")), tr("Open a drawing..."), column);
    openButton->setObjectName(QStringLiteral("homeOpen"));
    QPushButton *back = new QPushButton(tr("Back to my drawing"), column);
    back->setObjectName(QStringLiteral("homeBack"));
    back->setDefault(true);
    connect(openButton, &QPushButton::clicked, this, [this]() {
        m_window->openDocument();
        finish();
    });
    connect(back, &QPushButton::clicked, this, &HomeScreen::finish);
    buttons->addWidget(openButton);
    buttons->addStretch(1);
    buttons->addWidget(back);
    layout->addLayout(buttons);
    layout->addSpacing(22);

    QLabel *recentTitle = new QLabel(tr("Recent"), column);
    recentTitle->setObjectName(QStringLiteral("panelTitle"));
    recentTitle->setFont(ArtflowTheme::uiFont(11, QFont::DemiBold));
    layout->addWidget(recentTitle);
    m_recent = new QVBoxLayout();
    m_recent->setSpacing(6);
    layout->addLayout(m_recent);
    layout->addStretch(1);

    hide();
}

QStringList HomeScreen::recentFiles()
{
    QStringList files;
    for (const QString &path : QSettings(PENCIL2D, PENCIL2D).value(QStringLiteral("RecentFiles")).toStringList()) {
        if (QFileInfo::exists(path)) files << path;
    }
    return files;
}

void HomeScreen::rebuildRecent()
{
    while (QLayoutItem *item = m_recent->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QStringList files = recentFiles();
    if (files.isEmpty()) {
        QLabel *none = new QLabel(tr("Drawings you save show up here."), this);
        none->setObjectName(QStringLiteral("muted"));
        m_recent->addWidget(none);
        return;
    }
    for (const QString &path : files.mid(0, 8)) {
        const QFileInfo info(path);
        QToolButton *row = new QToolButton(this);
        row->setObjectName(QStringLiteral("recentRow"));
        row->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        row->setIcon(ArtflowIcons::icon(QStringLiteral("layers")));
        row->setIconSize(QSize(20, 20));
        row->setText(QStringLiteral("%1    %2  ·  %3").arg(info.completeBaseName(),
                                                          QDir::toNativeSeparators(info.absolutePath()),
                                                          QLocale().toString(info.lastModified(), QLocale::ShortFormat)));
        row->setToolTip(QDir::toNativeSeparators(path));
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->setCursor(Qt::PointingHandCursor);
        connect(row, &QToolButton::clicked, this, [this, path]() {
            m_window->openFile(path);
            finish();
        });
        m_recent->addWidget(row);
    }
}

void HomeScreen::open()
{
    rebuildRecent();
    setGeometry(m_window->rect());
    show();
    raise();
    setFocus();
}

void HomeScreen::finish()
{
    hide();
    Q_EMIT closed();
}

bool HomeScreen::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::Resize && isVisible()) setGeometry(m_window->rect());
    return QWidget::eventFilter(watched, event);
}

void HomeScreen::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), ArtflowTheme::window());
}

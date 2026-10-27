/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "LookDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include "pencildef.h"

#include "StudioCore.h"

#include "ArtflowTheme.h"
#include "TouchController.h"

namespace {

const int Shadow = 14;

QString setting(const char *key, const char *fallback)
{
    return QSettings(PENCIL2D, PENCIL2D).value(QStringLiteral("ArtFlowLook/%1").arg(QLatin1String(key)), QLatin1String(fallback)).toString();
}

void store(const char *key, const QString &value)
{
    QSettings(PENCIL2D, PENCIL2D).setValue(QStringLiteral("ArtFlowLook/%1").arg(QLatin1String(key)), value);
}

QLabel *heading(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text.toUpper(), parent);
    label->setObjectName(QStringLiteral("muted"));
    label->setFont(ArtflowTheme::uiFont(7.5, QFont::DemiBold));
    return label;
}

/// A row of exclusive pill buttons; returns the group.
QButtonGroup *choices(const QStringList &ids, const QStringList &names, const QString &current, QWidget *parent,
                      QVBoxLayout *layout)
{
    QHBoxLayout *row = new QHBoxLayout();
    row->setSpacing(6);
    QButtonGroup *group = new QButtonGroup(parent);
    for (int i = 0; i < ids.size(); ++i) {
        QToolButton *button = new QToolButton(parent);
        button->setObjectName(QStringLiteral("pillButton"));
        button->setText(names.at(i));
        button->setCheckable(true);
        button->setChecked(ids.at(i) == current);
        button->setProperty("artflowChoice", ids.at(i));
        button->setCursor(Qt::PointingHandCursor);
        group->addButton(button);
        row->addWidget(button);
    }
    row->addStretch(1);
    layout->addLayout(row);
    return group;
}

}

QString LookDialog::uiSize()
{
    return setting("uiSize", "auto");
}

qreal LookDialog::scaleFor(const QString &size)
{
    if (size == QLatin1String("compact")) return 0.8;
    if (size == QLatin1String("large")) return 1.2;
    return 1.0;
}

QString LookDialog::panelSide()
{
    return setting("panelSide", "right");
}

QString LookDialog::sliderSide()
{
    return setting("sliderSide", "left");
}

LookDialog::LookDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Shadow + 20, Shadow + 16, Shadow + 20, Shadow + 16);
    layout->setSpacing(8);

    QLabel *title = new QLabel(tr("Look and layout"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    title->setFont(ArtflowTheme::uiFont(11, QFont::DemiBold));
    layout->addWidget(title);
    layout->addSpacing(6);

    // Style
    layout->addWidget(heading(tr("Style"), this));
    QStringList styleNames;
    for (const QString &id : ArtflowTheme::styleIds()) styleNames << ArtflowTheme::styleName(id);
    QButtonGroup *styles = choices(ArtflowTheme::styleIds(), styleNames, ArtflowTheme::currentStyle(), this, layout);
    connect(styles, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
        ArtflowTheme::setLook(button->property("artflowChoice").toString(), ArtflowTheme::currentAccent());
        Q_EMIT lookChanged();
        update();
    });

    // Accent
    layout->addSpacing(4);
    layout->addWidget(heading(tr("Accent colour"), this));
    QHBoxLayout *accents = new QHBoxLayout();
    accents->setSpacing(8);
    QButtonGroup *accentGroup = new QButtonGroup(this);
    for (const QString &id : ArtflowTheme::accentIds()) {
        QToolButton *swatch = new QToolButton(this);
        swatch->setObjectName(QStringLiteral("accentSwatch"));
        swatch->setCheckable(true);
        swatch->setChecked(id == ArtflowTheme::currentAccent());
        swatch->setFixedSize(30, 30);
        swatch->setCursor(Qt::PointingHandCursor);
        swatch->setStyleSheet(QStringLiteral("QToolButton#accentSwatch { background: %1; border-radius: 15px; border: 2px solid transparent; }"
                                             " QToolButton#accentSwatch:checked { border: 2px solid palette(text); }")
                                  .arg(ArtflowTheme::accentColor(id).name()));
        swatch->setProperty("artflowChoice", id);
        accentGroup->addButton(swatch);
        accents->addWidget(swatch);
    }
    accents->addStretch(1);
    layout->addLayout(accents);
    connect(accentGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
        ArtflowTheme::setLook(ArtflowTheme::currentStyle(), button->property("artflowChoice").toString());
        Q_EMIT lookChanged();
        update();
    });

    // Size
    layout->addSpacing(4);
    layout->addWidget(heading(tr("Interface size"), this));
    QButtonGroup *sizes = choices({QStringLiteral("auto"), QStringLiteral("compact"), QStringLiteral("normal"), QStringLiteral("large")},
                                  {tr("Fit my screen"), tr("Compact"), tr("Normal"), tr("Large")}, uiSize(), this, layout);
    QLabel *sizeNote = new QLabel(tr("Applies the next time ArtFlow starts. \"Fit my screen\" picks Compact on small laptops."), this);
    sizeNote->setObjectName(QStringLiteral("muted"));
    sizeNote->setWordWrap(true);
    layout->addWidget(sizeNote);
    connect(sizes, &QButtonGroup::buttonClicked, this, [](QAbstractButton *button) {
        store("uiSize", button->property("artflowChoice").toString());
        // "Fit my screen" looks again at the next start.
        QSettings(PENCIL2D, PENCIL2D).remove(QStringLiteral("ArtFlowLook/autoSize"));
    });

    // Placement
    layout->addSpacing(4);
    layout->addWidget(heading(tr("Panels"), this));
    QButtonGroup *panels = choices({QStringLiteral("right"), QStringLiteral("left")}, {tr("Panels on the right"), tr("Panels on the left")},
                                   panelSide(), this, layout);
    QButtonGroup *sliders = choices({QStringLiteral("left"), QStringLiteral("right")}, {tr("Sliders on the left"), tr("Sliders on the right")},
                                    sliderSide(), this, layout);
    connect(panels, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
        store("panelSide", button->property("artflowChoice").toString());
        Q_EMIT placementChanged();
    });
    connect(sliders, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
        store("sliderSide", button->property("artflowChoice").toString());
        Q_EMIT placementChanged();
    });

    // Touch and drawing
    layout->addSpacing(4);
    layout->addWidget(heading(tr("Touch and drawing"), this));
    QCheckBox *fingers = new QCheckBox(tr("Fingers draw (off: fingers only move and zoom the page)"), this);
    fingers->setChecked(TouchController::fingersDraw());
    connect(fingers, &QCheckBox::toggled, this, &TouchController::setFingersDraw);
    layout->addWidget(fingers);
    QCheckBox *onPage = new QCheckBox(tr("Keep drawing on the page"), this);
    onPage->setChecked(StudioCore::keepOnPage());
    connect(onPage, &QCheckBox::toggled, this, &StudioCore::setKeepOnPage);
    layout->addWidget(onPage);

    layout->addSpacing(10);
    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    QPushButton *done = new QPushButton(tr("Done"), this);
    done->setDefault(true);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(done);
    layout->addLayout(buttons);
}

void LookDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF panel = QRectF(rect()).adjusted(Shadow, Shadow, -Shadow, -Shadow);
    for (int i = Shadow; i > 0; i -= 2) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, int(26 * (1.0 - qreal(i) / Shadow))));
        painter.drawRoundedRect(panel.adjusted(-i, -i + 4, i, i + 4), 16 + i, 16 + i);
    }
    painter.setPen(QPen(ArtflowTheme::line(), 1));
    painter.setBrush(ArtflowTheme::panel());
    painter.drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
}

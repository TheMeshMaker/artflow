/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ArtflowTheme.h"

#include "pencildef.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QSettings>
#include <QPalette>
#include <QStyleFactory>
#include <QWidget>

namespace {

QString hex(const QColor &color)
{
    return color.name(color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
}

QString styleSheet()
{
    using namespace ArtflowTheme;

    QString qss = QStringLiteral(R"(
QWidget { color: @text; }
QMainWindow, QDialog { background: @window; }
QMainWindow::separator { background: @window; width: 6px; height: 6px; }

QToolBar#ArtFlowTopBar { background: @window; border: none; border-bottom: 1px solid @raised; padding: 6px 10px; spacing: 4px; }
QToolBar#ArtFlowTopBar::separator { width: 1px; background: @line; margin: 10px 8px; }

QToolButton { background: transparent; border: none; border-radius: 10px; padding: 6px; color: @text; }
QToolButton:hover { background: @hover; }
QToolButton:pressed { background: @line; }
QToolButton:checked { background: @accent; color: #ffffff; }
QToolButton:checked:hover { background: @accentHover; }
QToolButton:disabled { color: @faint; }
QToolButton::menu-indicator { image: none; width: 0; }
QToolButton#toolTile { min-width: 38px; max-width: 38px; min-height: 38px; max-height: 38px; padding: 0; }
QToolButton#iconButton { min-width: 36px; min-height: 36px; padding: 0; }
QToolButton#pillButton { border-radius: 9px; padding: 5px 10px; background: @raised; color: @muted; }
QToolButton#optionTile { min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px; padding: 0; border-radius: 9px; }
QToolButton#optionTile:checked { background: @accentSoft; }
QToolButton#optionTile:checked:hover { background: @accentSoft; }
QLineEdit#layerNameEditor { padding: 2px 6px; border-radius: 6px; }
QToolButton#brushStyle { background: @raised; border: 1px solid @line; border-radius: 12px; padding: 0; }
QToolButton#brushStyle:hover { background: @hover; border-color: @strong; }
QToolButton#brushStyle:disabled { background: transparent; border-color: @raised; }
QToolButton#pillButton:hover { background: @hover; color: @text; }
QToolButton#pillButton:checked { background: @accentSoft; color: @accentHover; }

QDockWidget { border: none; }
QWidget#artflowPanel { background: @panel; }
QWidget[artflowSection="true"] { background: @panel; }
QLabel { background: transparent; }
QLabel#panelTitle { color: @text; font-weight: 600; }
QLabel#muted { color: @muted; }
QToolButton#recentRow { background: @raised; border: 1px solid @line; border-radius: 10px; padding: 9px 12px; text-align: left; }
QToolButton#recentRow:hover { background: @hover; border-color: @accent; }
QToolButton#zoomLabel { padding: 0 4px; color: @text; font-weight: 600; }

QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:horizontal { height: 8px; background: transparent; margin: 0 6px; }
QScrollBar:vertical { width: 8px; background: transparent; margin: 6px 0; }
QScrollBar::handle { background: @line; border-radius: 4px; min-width: 36px; min-height: 36px; }
QScrollBar::handle:hover { background: @strong; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QSlider::groove:horizontal { height: 4px; background: @line; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: @text; }
QSlider::handle:horizontal:hover { background: #ffffff; }

QSpinBox, QDoubleSpinBox, QLineEdit, QComboBox {
    background: @raised; border: 1px solid @line; border-radius: 8px; padding: 4px 8px;
    selection-background-color: @accent; selection-color: #ffffff; }
QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus, QComboBox:focus { border-color: @accent; }
QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; border: none; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox::down-arrow { image: url(:/artflow/icons/chevron-down-muted.svg); width: 12px; height: 12px; }
QComboBox QAbstractItemView { background: @raised; border: 1px solid @line; border-radius: 8px; padding: 4px;
    selection-background-color: @hover; outline: 0; }

QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid @strong; background: @raised; }
QCheckBox::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; image: url(:/artflow/icons/check-white.svg); }

QPushButton { background: @raised; border: 1px solid @line; border-radius: 9px; padding: 6px 14px; }
QPushButton:hover { background: @hover; }
QPushButton:pressed { background: @line; }
QPushButton:default { background: @accent; border-color: @accent; color: #ffffff; }
QPushButton:default:hover { background: @accentHover; }
QPushButton:disabled { color: @faint; }
QPushButton[artflowFlat="true"] { background: transparent; border: none; border-radius: 8px; padding: 0; }
QPushButton[artflowFlat="true"]:hover { background: @hover; }
QPushButton[artflowFlat="true"]:pressed { background: @line; }

QListView, QListWidget, QTreeView, QTableView { background: transparent; border: none; outline: 0; }
QListWidget::item { border-radius: 8px; padding: 3px; }
QListWidget::item:hover { background: @raised; }
QListWidget::item:selected { background: @hover; color: @text; }

QMenu { background: @raised; border: 1px solid @line; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 6px; }
QMenu::item:selected { background: @accent; color: #ffffff; }
QMenu::item:disabled { color: @faint; }
QMenu::separator { height: 1px; background: @line; margin: 5px 8px; }
QMenu::icon { padding-left: 6px; }

QToolTip { background: @raised; color: @text; border: 1px solid @line; border-radius: 8px; padding: 6px 9px; }

QStatusBar { background: @window; color: @muted; border-top: 1px solid @raised; }
QStatusBar::item { border: none; }

QGroupBox { border: 1px solid @line; border-radius: 10px; margin-top: 16px; padding-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: @muted; }
QTabWidget::pane { border: 1px solid @line; border-radius: 10px; top: -1px; }
QTabBar::tab { background: transparent; color: @muted; padding: 7px 14px; border-radius: 8px; margin: 2px; }
QTabBar::tab:selected { background: @raised; color: @text; }
QHeaderView::section { background: @panel; color: @muted; border: none; padding: 4px 8px; }
)");

    // Longest names first, so "@accentHover" is not eaten by "@accent".
    const QList<QPair<QString, QColor>> tokens = {
        {QStringLiteral("@accentHover"), accentHover()},
        {QStringLiteral("@strong"), colors().dark ? line().lighter(135) : line().darker(118)},
        {QStringLiteral("@accentSoft"), accentSoft()},
        {QStringLiteral("@accent"), accent()},
        {QStringLiteral("@window"), window()},
        {QStringLiteral("@panel"), panel()},
        {QStringLiteral("@raised"), raised()},
        {QStringLiteral("@hover"), hover()},
        {QStringLiteral("@line"), line()},
        {QStringLiteral("@muted"), textMuted()},
        {QStringLiteral("@faint"), textFaint()},
        {QStringLiteral("@text"), text()},
    };
    for (const auto &token : tokens) {
        qss.replace(token.first, hex(token.second));
    }
    return qss;
}

QPalette palette()
{
    using namespace ArtflowTheme;

    QPalette p;
    p.setColor(QPalette::Window, window());
    p.setColor(QPalette::WindowText, text());
    p.setColor(QPalette::Base, raised());
    p.setColor(QPalette::AlternateBase, panel());
    p.setColor(QPalette::Text, text());
    p.setColor(QPalette::PlaceholderText, textFaint());
    p.setColor(QPalette::Button, raised());
    p.setColor(QPalette::ButtonText, text());
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Highlight, accent());
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, raised());
    p.setColor(QPalette::ToolTipText, text());
    p.setColor(QPalette::Light, hover());
    p.setColor(QPalette::Midlight, line());
    p.setColor(QPalette::Mid, line());
    p.setColor(QPalette::Dark, window());
    p.setColor(QPalette::Shadow, Qt::black);
    p.setColor(QPalette::Link, accentHover());
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        p.setColor(QPalette::Disabled, role, textFaint());
    }
    return p;
}

}

QFont ArtflowTheme::uiFont(qreal pointSize, int weight)
{
    static const QString family = []() {
        const QStringList families = QFontDatabase::families();
        for (const QString &candidate : {QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                                         QStringLiteral("Inter"), QStringLiteral("Noto Sans")}) {
            if (families.contains(candidate)) return candidate;
        }
        return QString();
    }();

    QFont font(family);
    font.setPointSizeF(pointSize);
    font.setWeight(QFont::Weight(weight));
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

namespace {

ArtflowTheme::Colors styleColors(const QString &id)
{
    ArtflowTheme::Colors c;
    if (id == QLatin1String("graphite")) {
        c.window = QColor(0x14, 0x14, 0x15);
        c.panel = QColor(0x1b, 0x1b, 0x1d);
        c.raised = QColor(0x25, 0x25, 0x28);
        c.hover = QColor(0x30, 0x30, 0x34);
        c.line = QColor(0x36, 0x36, 0x3b);
        c.canvasSurround = QColor(0x0e, 0x0e, 0x10);
        c.text = QColor(0xed, 0xed, 0xef);
        c.textMuted = QColor(0xa2, 0xa2, 0xa8);
        c.textFaint = QColor(0x66, 0x66, 0x6c);
    } else if (id == QLatin1String("paper")) {
        c.dark = false;
        c.window = QColor(0xe9, 0xeb, 0xf0);
        c.panel = QColor(0xf5, 0xf6, 0xf8);
        c.raised = QColor(0xff, 0xff, 0xff);
        c.hover = QColor(0xe4, 0xe7, 0xee);
        c.line = QColor(0xd5, 0xd9, 0xe2);
        c.canvasSurround = QColor(0xc9, 0xce, 0xd8);
        c.text = QColor(0x1d, 0x21, 0x2c);
        c.textMuted = QColor(0x59, 0x60, 0x72);
        c.textFaint = QColor(0x98, 0x9f, 0xae);
    } else {
        c.window = QColor(0x0f, 0x11, 0x16);
        c.panel = QColor(0x15, 0x18, 0x1e);
        c.raised = QColor(0x1d, 0x21, 0x2a);
        c.hover = QColor(0x26, 0x2b, 0x36);
        c.line = QColor(0x2a, 0x2f, 0x3b);
        c.canvasSurround = QColor(0x0b, 0x0d, 0x11);
        c.text = QColor(0xe8, 0xea, 0xf0);
        c.textMuted = QColor(0x95, 0x9c, 0xab);
        c.textFaint = QColor(0x5d, 0x64, 0x72);
    }
    return c;
}

ArtflowTheme::Colors &current()
{
    static ArtflowTheme::Colors colors = [] {
        QSettings settings(PENCIL2D, PENCIL2D);
        ArtflowTheme::Colors c = styleColors(settings.value(QStringLiteral("ArtFlowLook/style"), QStringLiteral("midnight")).toString());
        c.accent = ArtflowTheme::accentColor(settings.value(QStringLiteral("ArtFlowLook/accent"), QStringLiteral("violet")).toString());
        return c;
    }();
    return colors;
}

}

const ArtflowTheme::Colors &ArtflowTheme::colors()
{
    return current();
}

QStringList ArtflowTheme::styleIds()
{
    return {QStringLiteral("midnight"), QStringLiteral("graphite"), QStringLiteral("paper")};
}

QString ArtflowTheme::styleName(const QString &id)
{
    if (id == QLatin1String("graphite")) return QCoreApplication::translate("ArtflowTheme", "Graphite");
    if (id == QLatin1String("paper")) return QCoreApplication::translate("ArtflowTheme", "Paper");
    return QCoreApplication::translate("ArtflowTheme", "Midnight");
}

QStringList ArtflowTheme::accentIds()
{
    return {QStringLiteral("violet"), QStringLiteral("blue"), QStringLiteral("teal"), QStringLiteral("rose"), QStringLiteral("amber")};
}

QColor ArtflowTheme::accentColor(const QString &id)
{
    if (id == QLatin1String("blue")) return QColor(0x4c, 0x8d, 0xff);
    if (id == QLatin1String("teal")) return QColor(0x1f, 0xb5, 0xa3);
    if (id == QLatin1String("rose")) return QColor(0xf2, 0x5a, 0x8a);
    if (id == QLatin1String("amber")) return QColor(0xe8, 0x9a, 0x1c);
    return QColor(0x8b, 0x7c, 0xff);
}

QString ArtflowTheme::currentStyle()
{
    return QSettings(PENCIL2D, PENCIL2D).value(QStringLiteral("ArtFlowLook/style"), QStringLiteral("midnight")).toString();
}

QString ArtflowTheme::currentAccent()
{
    return QSettings(PENCIL2D, PENCIL2D).value(QStringLiteral("ArtFlowLook/accent"), QStringLiteral("violet")).toString();
}

void ArtflowTheme::setLook(const QString &style, const QString &accent)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    settings.setValue(QStringLiteral("ArtFlowLook/style"), style);
    settings.setValue(QStringLiteral("ArtFlowLook/accent"), accent);
    Colors c = styleColors(style);
    c.accent = accentColor(accent);
    current() = c;
}

void ArtflowTheme::apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setPalette(palette());
    app.setFont(uiFont());
    app.setStyleSheet(styleSheet());
}

void ArtflowTheme::restyleEnginePanel(QWidget *panel)
{
    if (!panel) return;
    QList<QWidget *> widgets = panel->findChildren<QWidget *>();
    widgets << panel;
    for (QWidget *widget : widgets) {
        const QString own = widget->styleSheet();
        if (own.contains(QLatin1String("#D5D5D5"), Qt::CaseInsensitive)
            || own.contains(QLatin1String("#ADADAD"), Qt::CaseInsensitive)) {
            widget->setStyleSheet(QStringLiteral("::menu-indicator { image: none; }"));
        }
    }
}

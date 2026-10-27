/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ArtflowTheme.h"

#include <QApplication>
#include <QFontDatabase>
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
QToolButton#pillButton:hover { background: @hover; color: @text; }
QToolButton#pillButton:checked { background: @accentSoft; color: @accentHover; }

QDockWidget { border: none; }
QWidget#artflowPanel { background: @panel; }
QWidget[artflowSection="true"] { background: @panel; }
QLabel { background: transparent; }
QLabel#panelTitle { color: @text; font-weight: 600; }
QLabel#muted { color: @muted; }

QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:horizontal { height: 8px; background: transparent; margin: 0 6px; }
QScrollBar:vertical { width: 8px; background: transparent; margin: 6px 0; }
QScrollBar::handle { background: @line; border-radius: 4px; min-width: 36px; min-height: 36px; }
QScrollBar::handle:hover { background: #3a4150; }
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
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid #3a4150; background: @raised; }
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

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_HOMESCREEN_H
#define ARTFLOW_HOMESCREEN_H

#include <QAbstractButton>
#include <QSize>
#include <QWidget>

class MainWindow2;
class QVBoxLayout;

/// A "new drawing" choice: the page's shape drawn small, its name and size.
class PageCard : public QAbstractButton
{
    Q_OBJECT
public:
    PageCard(const QString &name, const QSize &page, QWidget *parent = nullptr);
    QSize page() const { return m_page; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QSize m_page;
    bool m_hovered {false};
};

/**
 * The start screen over the whole window: new drawings by page size, open a
 * drawing, the recent ones, or go back to the drawing that is open.
 */
class HomeScreen : public QWidget
{
    Q_OBJECT
public:
    explicit HomeScreen(MainWindow2 *window);

    /// Shows the screen with an up-to-date list of recent drawings.
    void open();
    /// Recent drawings that still exist, newest first.
    static QStringList recentFiles();

Q_SIGNALS:
    void closed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuildRecent();
    void finish();

    MainWindow2 *m_window;
    QVBoxLayout *m_recent;
};

#endif // ARTFLOW_HOMESCREEN_H

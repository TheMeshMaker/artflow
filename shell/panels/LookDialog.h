/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_LOOKDIALOG_H
#define ARTFLOW_LOOKDIALOG_H

#include <QDialog>

/**
 * How ArtFlow looks and where things sit: the style and accent colour,
 * the size of the interface (smaller for small laptops), which side the
 * panels and the brush sliders are on, what fingers do on a touch screen,
 * and whether drawing stays on the page. Changes show at once, except the
 * size, which applies on the next start.
 */
class LookDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LookDialog(QWidget *parent = nullptr);

    /// "auto", "compact", "normal" or "large".
    static QString uiSize();
    static qreal scaleFor(const QString &size);
    /// "left" or "right".
    static QString panelSide();
    static QString sliderSide();

Q_SIGNALS:
    void lookChanged();
    void placementChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // ARTFLOW_LOOKDIALOG_H

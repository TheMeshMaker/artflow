/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_BRUSHLIBRARY_H
#define ARTFLOW_BRUSHLIBRARY_H

#include <QAbstractButton>
#include <QImage>
#include <QWidget>

#include "BrushPreset.h"

class QVBoxLayout;
class StudioCore;

/// One brush style in the picker: its name over a sample stroke.
class BrushCard : public QAbstractButton
{
    Q_OBJECT
public:
    BrushCard(const BrushPreset &preset, QWidget *parent = nullptr);

    QString presetId() const;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    BrushPreset m_preset;
    QImage m_preview;
    bool m_hovered {false};
};

/**
 * The brush picker: a pop-up with every style of the Brush or the Pencil
 * tool, grouped by family, each shown with a sample stroke.
 */
class BrushLibrary : public QWidget
{
    Q_OBJECT
public:
    explicit BrushLibrary(StudioCore *core, QWidget *parent = nullptr);

    /// Shows the styles of \p tool with its top-left corner at \p globalPos
    /// (moved to stay on the screen).
    void popup(ToolType tool, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuild();

    StudioCore *m_core;
    ToolType m_tool {BRUSH};
    QWidget *m_body {nullptr};
};

#endif // ARTFLOW_BRUSHLIBRARY_H

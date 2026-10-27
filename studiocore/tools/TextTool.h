/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_TEXTTOOL_H
#define ARTFLOW_TEXTTOOL_H

#include <QFont>
#include <QPointF>
#include <QPointer>
#include <QStringList>

#include "ArtTool.h"

class QPlainTextEdit;

/**
 * Click where the text should start, type, then press Enter (Shift+Enter
 * for a new line, Escape to drop it). The text is drawn into the drawing
 * in the current colour, so it animates and erases like anything drawn.
 */
class TextTool : public ArtTool
{
    Q_OBJECT
public:
    explicit TextTool(QObject *parent = nullptr);
    ~TextTool() override;

    ToolType type() const override { return TEXT; }
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent *event) override;
    void pointerMoveEvent(PointerEvent *event) override;
    void pointerReleaseEvent(PointerEvent *event) override;
    bool leavingThisTool() override;

    /// The font choices, as shown to people, and the families behind them.
    static QStringList styleNames();
    int fontStyle() const;
    void setFontStyle(int style);
    int size() const;            ///< in canvas pixels
    void setSize(int size);
    bool bold() const;
    void setBold(bool bold);

    /// The text being typed, if any.
    QString pendingText() const;
    /// Draws the text being typed into the drawing.
    void commitText();
    /// Types \p text at \p canvasPoint and commits it (for tests and scripts).
    void placeText(const QPointF &canvasPoint, const QString &text);

Q_SIGNALS:
    void optionsChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QFont font(qreal scale) const;
    void openEditor(const QPointF &canvasPoint);
    void cancelText();
    void updateEditor();

    QPointer<QPlainTextEdit> m_editor;
    QPointF m_anchor;
    int m_style = 0;
    int m_size = 48;
    bool m_bold = false;
};

#endif // ARTFLOW_TEXTTOOL_H

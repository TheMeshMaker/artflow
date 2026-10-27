/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TextTool.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSettings>
#include <QtMath>

#include "colormanager.h"
#include "editor.h"
#include "pointerevent.h"
#include "scribblearea.h"
#include "viewmanager.h"

namespace {

const char *const Families[] = {"Segoe UI", "Georgia", "Segoe Print", "Comic Sans MS", "Consolas"};
const int StyleCount = 5;

QString settingsKey(const char *name)
{
    return QStringLiteral("ArtFlowTools/text%1").arg(QLatin1String(name));
}

}

TextTool::TextTool(QObject *parent)
    : ArtTool(parent)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    m_style = qBound(0, settings.value(settingsKey("Style"), 0).toInt(), StyleCount - 1);
    m_size = qBound(6, settings.value(settingsKey("Size"), 48).toInt(), 600);
    m_bold = settings.value(settingsKey("Bold"), false).toBool();
}

TextTool::~TextTool()
{
    delete m_editor;
}

QCursor TextTool::cursor()
{
    return QCursor(Qt::IBeamCursor);
}

QStringList TextTool::styleNames()
{
    return {tr("Clean"), tr("Classic"), tr("Handwriting"), tr("Comic"), tr("Typewriter")};
}

int TextTool::fontStyle() const
{
    return m_style;
}

void TextTool::setFontStyle(int style)
{
    style = qBound(0, style, StyleCount - 1);
    if (style == m_style) return;
    m_style = style;
    QSettings(PENCIL2D, PENCIL2D).setValue(settingsKey("Style"), style);
    updateEditor();
    Q_EMIT optionsChanged();
}

int TextTool::size() const
{
    return m_size;
}

void TextTool::setSize(int size)
{
    size = qBound(6, size, 600);
    if (size == m_size) return;
    m_size = size;
    QSettings(PENCIL2D, PENCIL2D).setValue(settingsKey("Size"), size);
    updateEditor();
    Q_EMIT optionsChanged();
}

bool TextTool::bold() const
{
    return m_bold;
}

void TextTool::setBold(bool bold)
{
    if (bold == m_bold) return;
    m_bold = bold;
    QSettings(PENCIL2D, PENCIL2D).setValue(settingsKey("Bold"), bold);
    updateEditor();
    Q_EMIT optionsChanged();
}

QFont TextTool::font(qreal scale) const
{
    QFont result(QString::fromLatin1(Families[m_style]));
    result.setPixelSize(qMax(1, qRound(m_size * scale)));
    result.setBold(m_bold);
    result.setStyleStrategy(QFont::PreferAntialias);
    return result;
}

void TextTool::pointerPressEvent(PointerEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    if (m_editor) {
        // Clicking elsewhere finishes the text being typed.
        commitText();
        return;
    }
    if (!canDraw()) return;
    openEditor(event->canvasPos());
}

void TextTool::pointerMoveEvent(PointerEvent *event)
{
    Q_UNUSED(event);
}

void TextTool::pointerReleaseEvent(PointerEvent *event)
{
    Q_UNUSED(event);
}

bool TextTool::leavingThisTool()
{
    commitText();
    return ArtTool::leavingThisTool();
}

QString TextTool::pendingText() const
{
    return m_editor ? m_editor->toPlainText() : QString();
}

void TextTool::openEditor(const QPointF &canvasPoint)
{
    m_anchor = canvasPoint;

    QPlainTextEdit *editor = new QPlainTextEdit(mScribbleArea);
    editor->setObjectName(QStringLiteral("artflowTextEditor"));
    editor->setFrameShape(QFrame::NoFrame);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor->document()->setDocumentMargin(0);
    editor->setContentsMargins(0, 0, 0, 0);
    editor->installEventFilter(this);
    m_editor = editor;

    connect(editor, &QPlainTextEdit::textChanged, this, &TextTool::updateEditor);
    connect(mEditor->color(), &ColorManager::colorChanged, editor, [this]() { updateEditor(); });
    connect(mEditor->view(), &ViewManager::viewChanged, editor, [this]() { updateEditor(); });

    updateEditor();
    editor->show();
    editor->setFocus();
}

void TextTool::updateEditor()
{
    if (!m_editor) return;

    const QFont shown = font(mEditor->view()->scaling());
    m_editor->setFont(shown);
    QColor color = mEditor->color()->frontColor();
    color.setAlpha(255);
    m_editor->setStyleSheet(QStringLiteral("QPlainTextEdit#artflowTextEditor { background: rgba(255, 255, 255, 40);"
                                           " border: 1px dashed #8b7cff; border-radius: 3px; color: %1; padding: 0; }")
                                .arg(color.name()));

    // Grows with the text, never smaller than a few letters.
    const QFontMetricsF metrics(shown);
    qreal width = metrics.horizontalAdvance(QStringLiteral("MMMM"));
    const QStringList lines = m_editor->toPlainText().split(QLatin1Char('\n'));
    for (const QString &line : lines) width = qMax(width, metrics.horizontalAdvance(line) + metrics.averageCharWidth() * 2);
    m_editor->resize(qCeil(width) + 6, qCeil(metrics.lineSpacing() * lines.size()) + 6);
    m_editor->move((mEditor->view()->mapCanvasToScreen(m_anchor) - QPointF(3, 3)).toPoint());
}

bool TextTool::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !(key->modifiers() & Qt::ShiftModifier)) {
            commitText();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            cancelText();
            return true;
        }
    }
    return ArtTool::eventFilter(watched, event);
}

void TextTool::cancelText()
{
    if (!m_editor) return;
    m_editor->deleteLater();
    m_editor = nullptr;
    mScribbleArea->setFocus();
}

void TextTool::commitText()
{
    if (!m_editor) return;
    const QString text = m_editor->toPlainText();
    cancelText();
    if (text.trimmed().isEmpty() || !canDraw()) return;

    const QFont drawn = font(1.0);
    const QFontMetricsF metrics(drawn);
    const QStringList lines = text.split(QLatin1Char('\n'));
    qreal width = 0.0;
    for (const QString &line : lines) width = qMax(width, metrics.horizontalAdvance(line));
    const int pad = qCeil(m_size * 0.2) + 2;

    QImage image(qCeil(width) + 2 * pad, qCeil(metrics.lineSpacing() * lines.size()) + 2 * pad,
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setFont(drawn);
    painter.setPen(mEditor->color()->frontColor());
    for (int i = 0; i < lines.size(); ++i) {
        painter.drawText(QPointF(pad, pad + metrics.ascent() + i * metrics.lineSpacing()), lines[i]);
    }
    painter.end();

    beginEdit();
    showPreview(image, (m_anchor - QPointF(pad, pad)).toPoint());
    commit();
}

void TextTool::placeText(const QPointF &canvasPoint, const QString &text)
{
    commitText();
    if (!canDraw()) return;
    openEditor(canvasPoint);
    m_editor->setPlainText(text);
    commitText();
}

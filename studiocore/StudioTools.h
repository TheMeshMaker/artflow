/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOTOOLS_H
#define STUDIOTOOLS_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include "pencildef.h"

class StrokeTool;
class StudioCore;

/**
 * What the user wants to do, independent of how the engine does it.
 * A mode lists the intents it offers; StudioTools turns an intent into
 * an engine tool.
 */
enum class ToolIntent {
    Brush,
    Pencil,
    Eraser,
    Smudge,
    Fill,
    LassoFill,
    Picker,
    Select,
    Move,
    Shapes,
    Text,
    Camera,
};

struct ToolIntentInfo
{
    ToolIntent intent;
    QString key;       ///< stable id used in settings, e.g. "brush"
    QString label;     ///< user-visible name
    QString shortcut;  ///< beginner shortcut (QKeySequence text)
    QString icon;      ///< icon name (the shell's icon set)
    ToolType tool;     ///< engine tool
};

class StudioTools : public QObject
{
    Q_OBJECT
public:
    explicit StudioTools(StudioCore *core);
    ~StudioTools() override;

    static QVector<ToolIntentInfo> allIntents();
    static ToolIntentInfo info(ToolIntent intent);

    ToolIntent currentIntent() const;
    bool activate(ToolIntent intent);

    /// Flips between Eraser and the last non-eraser intent.
    void toggleEraser();

    /// Width of the current stroke tool in pixels (0 for other tools).
    qreal brushSize() const;
    void setBrushSize(qreal size);
    /// Opacity of the drawing colour, 0..1 (the engine paints with the colour's alpha).
    qreal brushOpacity() const;
    void setBrushOpacity(qreal opacity);

Q_SIGNALS:
    void intentChanged(ToolIntent intent);
    void brushChanged();

private Q_SLOTS:
    void slotToolChanged(ToolType tool);

private:
    StrokeTool *strokeTool() const;
    void setCurrent(ToolIntent intent);

private:
    StudioCore *m_core;
    ToolIntent m_current {ToolIntent::Brush};
    ToolIntent m_beforeEraser {ToolIntent::Brush};
    QPointer<StrokeTool> m_watchedTool;
    bool m_switching {false};
};

#endif // STUDIOTOOLS_H

/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_STUDIOBRUSHES_H
#define ARTFLOW_STUDIOBRUSHES_H

#include <QHash>
#include <QImage>
#include <QObject>
#include <QVector>

#include "bitmapbrushengine.h"

#include "BrushPreset.h"
#include "BrushStroke.h"

class StrokeTool;
class StudioCore;

/**
 * ArtFlow's brushes: the library of brush styles, the style chosen for the
 * Brush and the Pencil tool, and the engine that paints their strokes (and
 * the eraser's) on bitmap layers. Installed into the core on creation.
 */
class StudioBrushes : public QObject, public BitmapBrushEngine
{
    Q_OBJECT
public:
    explicit StudioBrushes(StudioCore *core);
    ~StudioBrushes() override;

    /// Every brush style, grouped by family in display order.
    static QVector<BrushPreset> library();
    static QString familyTitle(const QString &family);
    QVector<BrushPreset> presetsFor(ToolType tool) const;
    BrushPreset preset(const QString &id) const;

    /// The style the Brush or Pencil tool paints with.
    BrushPreset current(ToolType tool) const;
    /// Chooses a style for its tool; the tool takes the size this style had last time.
    void select(const QString &id);

    /// A sample stroke drawn with \p preset, for the brush picker.
    static QImage preview(const BrushPreset &preset, const QSize &size, qreal devicePixelRatio, const QColor &color,
                          bool compact = false);

    // BitmapBrushEngine
    bool handles(ToolType tool) const override;
    QVector<Patch> beginStroke(const StrokeStart &start, const QPointF &point, qreal pressure) override;
    QVector<Patch> strokeTo(const QPointF &point, qreal pressure) override;
    QVector<Patch> endStroke() override;

Q_SIGNALS:
    void presetChanged(ToolType tool, const QString &id);

private:
    static BrushPreset eraserPreset();
    StrokeTool *strokeTool(ToolType tool) const;
    void applySize(ToolType tool);

    StudioCore *m_core;
    QHash<int, QString> m_current;
    BrushStroke m_stroke;
    bool m_applying = false;
};

#endif // ARTFLOW_STUDIOBRUSHES_H

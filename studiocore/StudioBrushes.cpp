/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StudioBrushes.h"

#include <QSettings>
#include <QtMath>

#include "editor.h"
#include "layercamera.h"
#include "object.h"
#include "stroketool.h"
#include "toolmanager.h"

#include "StudioCore.h"

namespace {

QString settingsKey(ToolType tool)
{
    return tool == PENCIL ? QStringLiteral("ArtFlowBrushes/pencil") : QStringLiteral("ArtFlowBrushes/brush");
}

QString sizeKey(const QString &id)
{
    return QStringLiteral("ArtFlowBrushes/size/%1").arg(id);
}

BrushPreset make(const char *id, const QString &name, const char *family, ToolType tool, qreal size)
{
    BrushPreset preset;
    preset.id = QLatin1String(id);
    preset.name = name;
    preset.family = QLatin1String(family);
    preset.tool = tool;
    preset.size = size;
    return preset;
}

}

StudioBrushes::StudioBrushes(StudioCore *core)
    : QObject(core)
    , m_core(core)
{
    QSettings settings(PENCIL2D, PENCIL2D);
    m_current[BRUSH] = settings.value(settingsKey(BRUSH), QStringLiteral("ink-pen")).toString();
    m_current[PENCIL] = settings.value(settingsKey(PENCIL), QStringLiteral("pencil")).toString();
    for (ToolType tool : {BRUSH, PENCIL}) {
        if (preset(m_current[tool]).tool != tool) m_current[tool] = presetsFor(tool).first().id;

        // Each style remembers the size it was last used at.
        if (StrokeTool *stroke = strokeTool(tool)) {
            connect(stroke, &StrokeTool::widthChanged, this, [this, tool](qreal width) {
                if (!m_applying) QSettings(PENCIL2D, PENCIL2D).setValue(sizeKey(m_current[tool]), width);
            });
        }
        applySize(tool);
    }

    BitmapBrushEngine::setEngine(this);
}

StudioBrushes::~StudioBrushes()
{
    if (BitmapBrushEngine::engine() == this) BitmapBrushEngine::setEngine(nullptr);
}

QVector<BrushPreset> StudioBrushes::library()
{
    QVector<BrushPreset> all;

    // Pencil tool: sketching
    BrushPreset p = make("pencil", tr("Pencil"), "sketch", PENCIL, 4);
    p.hardness = 0.8; p.spacing = 0.07; p.flow = 0.9; p.pressureSize = 0.3; p.pressureOpacity = 0.65;
    p.grain = 0.55; p.grainScale = 1.0;
    all << p;

    p = make("soft-pencil", tr("Soft Pencil"), "sketch", PENCIL, 9);
    p.hardness = 0.55; p.spacing = 0.07; p.flow = 0.8; p.pressureSize = 0.35; p.pressureOpacity = 0.75;
    p.grain = 0.7; p.grainScale = 1.3;
    all << p;

    p = make("clean-line", tr("Clean Line"), "sketch", PENCIL, 3);
    p.hardness = 1.0; p.spacing = 0.05; p.pressureSize = 0.4; p.pressureOpacity = 0.2; p.speedThinning = true;
    all << p;

    p = make("charcoal", tr("Charcoal"), "sketch", PENCIL, 22);
    p.hardness = 0.7; p.spacing = 0.06; p.flow = 0.95; p.pressureSize = 0.25; p.pressureOpacity = 0.7;
    p.grain = 0.85; p.grainScale = 2.0; p.roundness = 0.7; p.angle = 35; p.sizeJitter = 0.06;
    all << p;

    p = make("crayon", tr("Crayon"), "sketch", PENCIL, 14);
    p.hardness = 0.9; p.spacing = 0.06; p.pressureSize = 0.15; p.pressureOpacity = 0.45;
    p.grain = 0.8; p.grainScale = 1.6;
    all << p;

    // Brush tool: inking
    p = make("ink-pen", tr("Ink Pen"), "ink", BRUSH, 8);
    p.hardness = 1.0; p.spacing = 0.04; p.pressureSize = 0.75; p.taper = 1.2; p.speedThinning = true;
    all << p;

    p = make("fine-liner", tr("Fine Liner"), "ink", BRUSH, 4);
    p.hardness = 1.0; p.spacing = 0.04; p.pressureSize = 0.1;
    all << p;

    p = make("brush-pen", tr("Brush Pen"), "ink", BRUSH, 16);
    p.hardness = 0.98; p.spacing = 0.03; p.pressureSize = 0.92; p.taper = 2.5; p.speedThinning = true;
    all << p;

    p = make("calligraphy", tr("Calligraphy"), "ink", BRUSH, 20);
    p.hardness = 1.0; p.spacing = 0.025; p.roundness = 0.2; p.angle = 40; p.pressureSize = 0.3;
    all << p;

    // Brush tool: painting
    p = make("round-brush", tr("Round Brush"), "paint", BRUSH, 26);
    p.hardness = 0.85; p.spacing = 0.05; p.pressureSize = 0.55; p.pressureOpacity = 0.3;
    all << p;

    p = make("soft-brush", tr("Soft Brush"), "paint", BRUSH, 60);
    p.hardness = 0.05; p.spacing = 0.05; p.flow = 0.55; p.pressureSize = 0.2; p.pressureOpacity = 0.6;
    all << p;

    p = make("marker", tr("Marker"), "paint", BRUSH, 24);
    p.hardness = 0.92; p.spacing = 0.04; p.flow = 0.55; p.pressureSize = 0.1; p.pressureOpacity = 0.1;
    p.roundness = 0.75; p.angle = 60;
    all << p;

    p = make("flat-paint", tr("Flat Paint"), "paint", BRUSH, 34);
    p.hardness = 0.95; p.spacing = 0.03; p.flow = 0.95; p.roundness = 0.4; p.angle = 75; p.pressureSize = 0.35;
    p.grain = 0.25; p.grainScale = 2.5;
    all << p;

    p = make("watercolor", tr("Watercolor"), "paint", BRUSH, 44);
    p.hardness = 0.4; p.spacing = 0.05; p.flow = 0.35; p.pressureSize = 0.2; p.pressureOpacity = 0.5;
    p.grain = 0.4; p.grainScale = 3.0; p.sizeJitter = 0.05;
    all << p;

    // Brush tool: effects
    p = make("airbrush", tr("Airbrush"), "effects", BRUSH, 90);
    p.hardness = 0.0; p.spacing = 0.05; p.flow = 0.07; p.buildUp = true; p.pressureSize = 0.1; p.pressureOpacity = 0.8;
    all << p;

    p = make("spray", tr("Spray Can"), "effects", BRUSH, 70);
    p.spacing = 0.18; p.flow = 0.8; p.buildUp = true; p.particles = 20; p.pressureSize = 0.2; p.pressureOpacity = 0.3;
    all << p;

    return all;
}

QString StudioBrushes::familyTitle(const QString &family)
{
    if (family == QLatin1String("sketch")) return tr("Sketching");
    if (family == QLatin1String("ink")) return tr("Inking");
    if (family == QLatin1String("paint")) return tr("Painting");
    if (family == QLatin1String("effects")) return tr("Effects");
    return family;
}

QVector<BrushPreset> StudioBrushes::presetsFor(ToolType tool) const
{
    QVector<BrushPreset> presets;
    for (const BrushPreset &preset : library()) {
        if (preset.tool == tool) presets << preset;
    }
    return presets;
}

BrushPreset StudioBrushes::preset(const QString &id) const
{
    for (const BrushPreset &preset : library()) {
        if (preset.id == id) return preset;
    }
    return BrushPreset();
}

BrushPreset StudioBrushes::current(ToolType tool) const
{
    return preset(m_current.value(tool == PENCIL ? PENCIL : BRUSH));
}

void StudioBrushes::select(const QString &id)
{
    const BrushPreset chosen = preset(id);
    if (chosen.id.isEmpty()) return;

    m_current[chosen.tool] = id;
    QSettings(PENCIL2D, PENCIL2D).setValue(settingsKey(chosen.tool), id);
    applySize(chosen.tool);
    Q_EMIT presetChanged(chosen.tool, id);
}

void StudioBrushes::applySize(ToolType tool)
{
    const BrushPreset style = current(tool);
    StrokeTool *stroke = strokeTool(tool);
    if (!stroke || style.id.isEmpty()) return;

    const qreal size = QSettings(PENCIL2D, PENCIL2D).value(sizeKey(style.id), style.size).toReal();
    m_applying = true;
    stroke->setWidth(size);
    m_applying = false;
}

StrokeTool *StudioBrushes::strokeTool(ToolType tool) const
{
    return qobject_cast<StrokeTool *>(m_core->editor()->tools()->getTool(tool));
}

BrushPreset StudioBrushes::eraserPreset()
{
    BrushPreset p = make("eraser", tr("Eraser"), "erase", ERASER, 20);
    p.hardness = 0.9; p.spacing = 0.04; p.pressureSize = 0.4;
    return p;
}

QImage StudioBrushes::preview(const BrushPreset &preset, const QSize &size, qreal devicePixelRatio,
                              const QColor &color, bool compact)
{
    const qreal w = size.width() * devicePixelRatio;
    const qreal h = size.height() * devicePixelRatio;

    // Each style is shown at a size that suits the sample, not at its real size.
    const bool wide = preset.particles > 0 || preset.hardness < 0.3;
    const qreal width = qBound(h * 0.08, preset.size * devicePixelRatio * 0.55, h * (wide ? 0.5 : 0.3));
    const qreal margin = width * 0.6 + 4 * devicePixelRatio;

    const int count = 48;
    QVector<QPointF> points;
    QVector<qreal> pressures;
    for (int i = 0; i <= count; ++i) {
        const qreal t = qreal(i) / count;
        if (compact) {
            points << QPointF(margin + (w - 2 * margin) * t, h * 0.5 - qSin(t * 2 * M_PI) * (h * 0.5 - margin) * 0.8);
        } else {
            points << QPointF(margin + (w - 2 * margin) * t, h * 0.5 + qSin(t * 2 * M_PI) * (h * 0.5 - margin) * 0.6);
        }
        // Light at both ends, full pressure in the middle, as a hand would.
        pressures << 0.2 + 0.8 * qSin(t * M_PI);
    }

    BrushStroke stroke;
    stroke.begin(preset, color, width, true, true, points.first(), pressures.first());
    for (int i = 1; i < points.size(); ++i) stroke.moveTo(points[i], pressures[i]);
    stroke.finish();

    QImage image = stroke.render(QRect(0, 0, qRound(w), qRound(h)));
    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

bool StudioBrushes::symmetry() const
{
    return m_symmetry;
}

void StudioBrushes::setSymmetry(bool on)
{
    if (on == m_symmetry) return;
    m_symmetry = on;
    Q_EMIT symmetryChanged(on);
}

qreal StudioBrushes::symmetryAxis() const
{
    const Object *object = m_core->editor()->object();
    if (!object) return 0.0;
    const std::vector<LayerCamera *> cameras = object->getLayersByType<LayerCamera>();
    return cameras.empty() ? 0.0 : QRectF(cameras.front()->getViewRect()).center().x();
}

bool StudioBrushes::handles(ToolType tool) const
{
    return tool == BRUSH || tool == PENCIL || tool == ERASER;
}

QVector<BitmapBrushEngine::Patch> StudioBrushes::beginStroke(const StrokeStart &start, const QPointF &point,
                                                             qreal pressure)
{
    const bool erasing = start.tool == ERASER;
    m_stroke.setMirror(m_symmetry, symmetryAxis());
    // The eraser's stroke is a mask the engine cuts out of the layer, so it
    // is drawn fully opaque whatever the colour.
    m_stroke.begin(erasing ? eraserPreset() : current(start.tool), erasing ? QColor(Qt::black) : start.color,
                   start.width, start.pressureEnabled, start.tablet, point, pressure);
    return m_stroke.takePatches();
}

QVector<BitmapBrushEngine::Patch> StudioBrushes::strokeTo(const QPointF &point, qreal pressure)
{
    m_stroke.moveTo(point, pressure);
    return m_stroke.takePatches();
}

QVector<BitmapBrushEngine::Patch> StudioBrushes::endStroke()
{
    m_stroke.finish();
    const QVector<Patch> patches = m_stroke.takePatches();
    m_stroke.clear();
    return patches;
}

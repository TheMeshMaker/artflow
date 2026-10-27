/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_LASSOFILLTOOL_H
#define ARTFLOW_LASSOFILLTOOL_H

#include <QPolygonF>

#include "ArtTool.h"

/**
 * Draw around an area and it fills with the current colour: the quick way
 * to colour flat shapes in animation, with no closed outline needed.
 */
class LassoFillTool : public ArtTool
{
    Q_OBJECT
public:
    explicit LassoFillTool(QObject *parent = nullptr);

    ToolType type() const override { return LASSOFILL; }
    QCursor cursor() override;

    void pointerPressEvent(PointerEvent *event) override;
    void pointerMoveEvent(PointerEvent *event) override;
    void pointerReleaseEvent(PointerEvent *event) override;

private:
    void render(bool finished);

    QPolygonF m_points;
};

#endif // ARTFLOW_LASSOFILLTOOL_H

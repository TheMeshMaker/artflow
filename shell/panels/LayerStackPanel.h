/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_LAYERSTACKPANEL_H
#define ARTFLOW_LAYERSTACKPANEL_H

#include <QWidget>

class QListView;
class StudioCore;
class StudioTimelineModel;

/**
 * Studio mode: the layer stack. It is the third view on the timeline
 * model and only uses its first column (one row per layer).
 */
class LayerStackPanel : public QWidget
{
    Q_OBJECT
public:
    LayerStackPanel(StudioCore *core, StudioTimelineModel *model, QWidget *parent = nullptr);

Q_SIGNALS:
    /// The panel's maximum height changed with the number of layers.
    void heightLimitChanged();

private Q_SLOTS:
    void slotModelReset();

private:
    StudioCore *m_core;
    StudioTimelineModel *m_model;
    QListView *m_list;
};

#endif // ARTFLOW_LAYERSTACKPANEL_H

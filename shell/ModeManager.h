/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ARTFLOW_MODEMANAGER_H
#define ARTFLOW_MODEMANAGER_H

#include <functional>

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <StudioTools.h>

class QWidget;
class StudioCore;
class StudioTimelineModel;

/**
 * Everything that makes a mode a mode. A mode never owns document state:
 * it only decides which tools, panels and engine dockers sit around the
 * one canvas, so switching modes cannot lose anything.
 */
struct ModeDescriptor
{
    using PanelFactory = std::function<QWidget *(StudioCore *, StudioTimelineModel *, QWidget *parent)>;

    QString id;                   ///< "flip", "motion", "studio"
    QString title;                ///< shown on the mode switcher
    QString tagline;              ///< tooltip
    QVector<ToolIntent> tools;    ///< tool strip, in order
    ToolIntent defaultTool {ToolIntent::Brush};
    QStringList engineDocks;      ///< engine dock widgets shown in this mode (by object name)
    bool showLayerStack {false};  ///< show the layer stack panel beside the canvas
    bool showStatusBar {false};
    PanelFactory createTimelinePanel; ///< panel under the canvas, may be empty
};

class ModeManager : public QObject
{
    Q_OBJECT
public:
    explicit ModeManager(StudioCore *core, QObject *parent = nullptr);
    ~ModeManager() override;

    void registerMode(const ModeDescriptor &mode);
    const QVector<ModeDescriptor> &modes() const;
    const ModeDescriptor *mode(const QString &id) const;

    QString currentModeId() const;
    const ModeDescriptor *currentMode() const;

    /**
     * Switches to \p id: remembers the tool used in the mode being left
     * and restores the one last used in the new mode. The canvas and the
     * document are not touched.
     */
    bool switchTo(const QString &id);

Q_SIGNALS:
    void modeChanged(const QString &id, const QString &previousId);

private:
    StudioCore *m_core;
    QVector<ModeDescriptor> m_modes;
    QString m_current;
    QHash<QString, ToolIntent> m_lastTool;
};

/// Registers Flip, Motion and Studio.
void registerDefaultModes(ModeManager *manager);

#endif // ARTFLOW_MODEMANAGER_H

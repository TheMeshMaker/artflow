/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOTIMELINE_H
#define STUDIOTIMELINE_H

#include <QList>
#include <QObject>

class Layer;
class StudioCore;

/**
 * Frames of the document, in the engine's terms: frames are numbered from
 * 1, a keyframe at frame n holds a drawing (or a camera position), and
 * the frames after it show it until the next keyframe.
 *
 * Edits go through the engine's Editor / ActionCommands so they land in
 * its undo history (adding and deleting do; duplicating does not record
 * an undo step in the engine yet).
 */
class StudioTimeline : public QObject
{
    Q_OBJECT
public:
    explicit StudioTimeline(StudioCore *core);
    ~StudioTimeline() override;

    int currentFrame() const;
    void setCurrentFrame(int frame);

    int frameRate() const;
    void setFrameRate(int fps);

    /// Last frame that holds a keyframe on any layer.
    int lastUsedFrame() const;

    bool hasKeyframe(Layer *layer, int frame) const;
    QList<int> keyframeTimes(Layer *layer) const;

    /// Adds an empty drawing in the first free frame after \p frame and goes there.
    void addFrameAfter(Layer *layer, int frame);
    /// Adds a key at \p frame (or the next free frame). On a camera layer the
    /// new key holds the camera as it is at that frame.
    void addKeyAt(Layer *layer, int frame);
    /// Copies the drawing at \p frame into the next free frame and goes there.
    void duplicateFrame(Layer *layer, int frame);
    /// Removes the keyframe at \p frame.
    void removeFrame(Layer *layer, int frame);

Q_SIGNALS:
    void currentFrameChanged(int frame);
    void keyframesChanged();
    void frameRateChanged();

private:
    void focus(Layer *layer, int frame);

    StudioCore *m_core;
};

#endif // STUDIOTIMELINE_H

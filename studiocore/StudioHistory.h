/*
 *  SPDX-FileCopyrightText: 2026 ArtFlow contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef STUDIOHISTORY_H
#define STUDIOHISTORY_H

#include <QObject>

class QAction;
class StudioCore;

/**
 * Undo/redo for the document. Uses the engine's UndoRedoManager actions,
 * which drive either its QUndoStack or its legacy backup list depending
 * on the user's preference, i.e. the same path as Ctrl+Z.
 */
class StudioHistory : public QObject
{
    Q_OBJECT
public:
    explicit StudioHistory(StudioCore *core);
    ~StudioHistory() override;

    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;

public Q_SLOTS:
    void undo();
    void redo();

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void refresh();

private:
    StudioCore *m_core;
    QAction *m_undo;
    QAction *m_redo;
};

#endif // STUDIOHISTORY_H

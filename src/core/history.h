#pragma once

// Undo, by snapshot.
//
// A deck at this size is small enough that whole-document snapshots are honest
// and fast, and they make the one rule that matters easy to keep: one gesture
// is one undo step. A drag calls begin() when it starts and commit() when it
// ends, so the twenty intermediate positions never reach the stack.

#include <QString>
#include <QVector>

#include "core/scene.h"

class History {
public:
    void reset(const Document &document);

    // Call before mutating. Nested begins collapse into the outermost one, so a
    // command built from smaller commands still lands as a single step.
    void begin(const Document &document, const QString &label);
    void commit();
    void abandon();
    bool cancel(Document &document);

    bool canUndo() const { return !m_past.isEmpty(); }
    bool canRedo() const { return !m_future.isEmpty(); }
    QString undoLabel() const;
    QString redoLabel() const;

    bool undo(Document &document);
    bool redo(Document &document);

private:
    struct Entry {
        Document document;
        QString label;
    };

    QVector<Entry> m_past;
    QVector<Entry> m_future;
    int m_depth = 0;
    Entry m_pending;
    bool m_hasPending = false;
};

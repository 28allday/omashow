#include "core/history.h"

namespace {
constexpr int kMaxUndo = 200;
}

void History::reset(const Document &document) {
    Q_UNUSED(document);
    m_past.clear();
    m_future.clear();
    m_depth = 0;
    m_hasPending = false;
}

void History::begin(const Document &document, const QString &label) {
    if (m_depth++ > 0)
        return;
    m_pending = {document, label};
    m_hasPending = true;
}

void History::commit() {
    if (m_depth > 0)
        --m_depth;
    if (m_depth > 0 || !m_hasPending)
        return;

    m_past.append(m_pending);
    if (m_past.size() > kMaxUndo)
        m_past.removeFirst();
    m_future.clear();
    m_hasPending = false;
}

void History::abandon() {
    if (m_depth > 0)
        --m_depth;
    if (m_depth == 0)
        m_hasPending = false;
}

QString History::undoLabel() const {
    return m_past.isEmpty() ? QString() : m_past.last().label;
}

QString History::redoLabel() const {
    return m_future.isEmpty() ? QString() : m_future.last().label;
}

bool History::undo(Document &document) {
    if (m_past.isEmpty())
        return false;
    const Entry entry = m_past.takeLast();
    m_future.append({document, entry.label});
    document = entry.document;
    return true;
}

bool History::redo(Document &document) {
    if (m_future.isEmpty())
        return false;
    const Entry entry = m_future.takeLast();
    m_past.append({document, entry.label});
    document = entry.document;
    return true;
}

bool History::cancel(Document &document) {
    if (!m_hasPending) return false;
    document = m_pending.document;
    m_hasPending = false; m_depth = 0;
    return true;
}

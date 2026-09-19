#pragma once

#include "anim/presentation.h"
#include "anim/morph.h"
#include "core/scene.h"
#include <QCache>

// Per-document playback data. Timings are indexed once; resolved slides and
// transition matches are reused across frames. Never shared between threads.
class PresentationCache {
public:
    void reset(const Document &document, bool includeSkipped);
    Frame frameAt(qreal time) const;
    qreal duration() const { return m_duration; }
    Slide slide(int index) const;
    QColor backgroundAt(qreal time) const;
    QVector<SceneObject> statesAt(qreal time) const;
private:
    Document m_document;
    QVector<int> m_indices;
    QVector<qreal> m_starts, m_holds;
    qreal m_duration = 0;
    mutable QCache<int, Slide> m_slides{16};
    mutable int m_from = -1, m_to = -1;
    mutable Slide m_settled, m_arriving;
    mutable QVector<MorphPair> m_pairs;
};

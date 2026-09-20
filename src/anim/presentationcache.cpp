#include "anim/presentationcache.h"
#include "anim/evaluator.h"
#include "core/design.h"
#include <algorithm>

void PresentationCache::reset(const Document &document, bool includeSkipped) {
    m_document = document;
    m_indices = Presentation::slideIndices(document, includeSkipped);
    m_starts.clear(); m_holds.clear(); m_slides.clear();
    m_from = m_to = -1; m_settled = {}; m_arriving = {}; m_pairs.clear();
    qreal cursor = 0;
    m_transitions.clear();
    for (int n = 0; n < m_indices.size(); ++n) {
        const int index = m_indices.at(n);
        m_starts.append(cursor);
        const auto hold = Presentation::slideDuration(document, index);
        m_holds.append(hold);
        // The transition belongs to the slide being arrived at, so slide n's
        // entry is what follows slide n-1.
        const qreal seconds = n + 1 < m_indices.size()
            ? Presentation::transitionSeconds(document, m_indices.at(n + 1)) : 0;
        m_transitions.append(seconds);
        cursor += hold + seconds;
    }
    m_duration = m_indices.isEmpty() ? 0 : cursor - (m_transitions.isEmpty() ? 0 : m_transitions.last());
}

Frame PresentationCache::frameAt(qreal time) const {
    Frame frame;
    if (m_indices.isEmpty()) return frame;
    time = qMax(qreal(0), time);
    const int n = int(std::upper_bound(m_starts.cbegin(), m_starts.cend(), time) - m_starts.cbegin()) - 1;
    frame.slideIndex = m_indices[n];
    frame.slideTime = time - m_starts[n];
    if (n + 1 < m_indices.size() && frame.slideTime >= m_holds[n]) {
        const qreal seconds = m_transitions[n];
        frame.inTransition = true;
        frame.fromSlide = frame.slideIndex;
        frame.slideIndex = m_indices[n + 1];
        frame.transitionProgress = seconds > 0 ? (frame.slideTime - m_holds[n]) / seconds : 1;
        frame.slideTime = 0;
    }
    return frame;
}

Slide PresentationCache::slide(int index) const {
    if (auto *cached = m_slides.object(index)) return *cached;
    const auto resolved = Design::resolve(m_document, index);
    m_slides.insert(index, new Slide(resolved));
    return resolved;
}

QColor PresentationCache::backgroundAt(qreal time) const {
    const auto frame = frameAt(time);
    if (frame.slideIndex < 0) return Qt::black;
    const auto to = slide(frame.slideIndex).background;
    if (!frame.inTransition) return to;
    const auto from = slide(frame.fromSlide).background;
    return Presentation::blendBackground(from, to,
                                         Presentation::transitionKind(m_document, frame.slideIndex),
                                         frame.transitionProgress);
}

QVector<SceneObject> PresentationCache::statesAt(qreal time) const {
    const auto frame = frameAt(time);
    if (frame.slideIndex < 0) return {};
    if (!frame.inTransition) return Evaluator::stateAt(slide(frame.slideIndex), frame.slideTime);
    if (m_from != frame.fromSlide || m_to != frame.slideIndex) {
        m_from = frame.fromSlide; m_to = frame.slideIndex;
        m_settled = slide(m_from);
        m_settled.objects = Evaluator::stateAt(m_settled, m_settled.timeline.duration());
        m_settled.timeline = {};
        const auto incoming = slide(m_to);
        m_arriving = incoming; m_arriving.objects.clear(); m_arriving.timeline = {};
        for (const auto &object : incoming.objects) {
            bool buildsIn = false;
            for (const auto *step : incoming.timeline.stepsFor(object.id))
                if (step->startsHidden()) buildsIn = true;
            if (!buildsIn) m_arriving.objects.append(object);
        }
        m_kind = Presentation::transitionKind(m_document, m_to);
        m_pairs = m_kind == Presentation::Morph ? Morph::match(m_settled, m_arriving)
                                                : QVector<MorphPair>();
    }
    return Presentation::blend(m_document, m_settled, m_arriving, m_pairs, m_kind,
                               m_document.slides.at(m_to).transitionDirection,
                               frame.transitionProgress);
}

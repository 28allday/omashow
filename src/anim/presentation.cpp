#include "anim/presentation.h"
#include "core/design.h"

#include "anim/evaluator.h"
#include "anim/morph.h"
#include "core/scene.h"

#include <algorithm>

qreal Presentation::slideDuration(const Document &document, int slideIndex) {
  if (slideIndex < 0 || slideIndex >= document.slides.size())
    return 0.0;
  return document.slides.at(slideIndex).timeline.duration() + kHold;
}

QVector<int> Presentation::slideIndices(const Document &document,
                                        bool includeSkipped) {
  QVector<int> indices;
  for (int i = 0; i < document.slides.size(); ++i)
    if (includeSkipped || !document.slides.at(i).skipped)
      indices.append(i);
  return indices;
}
qreal Presentation::slideStart(const Document &document, int index,
                               bool includeSkipped) {
  qreal start = 0;
  for (int i : slideIndices(document, includeSkipped)) {
    if (i >= index)
      return start;
    start += slideDuration(document, i) + document.transitionDuration;
  }
  return duration(document, includeSkipped);
}
qreal Presentation::duration(const Document &document, bool includeSkipped) {
  const auto indices = slideIndices(document, includeSkipped);
  qreal total = qMax(0, int(indices.size()) - 1) * document.transitionDuration;
  for (int i : indices)
    total += slideDuration(document, i);
  return total;
}
Frame Presentation::frameAt(const Document &document, qreal t,
                            bool includeSkipped) {
  Frame frame;
  const auto indices = slideIndices(document, includeSkipped);
  t = std::max(0.0, t);
  qreal cursor = 0;
  for (int n = 0; n < indices.size(); ++n) {
    const int i = indices.at(n);
    const qreal hold = slideDuration(document, i);
    if (t < cursor + hold || n == indices.size() - 1) {
      frame.slideIndex = i;
      frame.slideTime = t - cursor;
      return frame;
    }
    cursor += hold;
    if (t < cursor + document.transitionDuration) {
      frame.inTransition = true;
      frame.fromSlide = i;
      frame.slideIndex = indices.at(n + 1);
      frame.transitionProgress =
          document.transitionDuration > 0
              ? (t - cursor) / document.transitionDuration
              : 1;
      return frame;
    }
    cursor += document.transitionDuration;
  }
  return frame;
}

QVector<SceneObject> Presentation::stateAt(const Document &document, qreal t,
                                           bool includeSkipped) {
  if (document.slides.isEmpty())
    return {};

  const Frame frame = frameAt(document, t, includeSkipped);
  if (frame.slideIndex < 0)
    return {};
  if (frame.inTransition) {
    const Slide from = Design::resolve(document, frame.fromSlide);
    const Slide to = Design::resolve(document, frame.slideIndex);

    // The outgoing slide is taken at its settled state, so a transition
    // shows a finished slide moving, never a half-built one sliding past.
    Slide settledFrom = from;
    settledFrom.objects = Evaluator::stateAt(from, from.timeline.duration());
    settledFrom.timeline = Timeline();

    // The incoming slide contributes only the objects that are *already
    // there* when it opens. Anything with a build of its own arrives by
    // that build once the transition has finished — letting it morph in
    // too would animate it twice, appearing during the transition and then
    // snapping back to invisible the moment the slide takes over.
    Slide arrivingTo = to;
    arrivingTo.objects.clear();
    for (const SceneObject &object : to.objects) {
      bool buildsIn = false;
      for (const auto *step : to.timeline.stepsFor(object.id))
        if (step->phase == BuildPhase::In && step->effect != Effect::None && step->effect != Effect::Media)
          buildsIn = true;
      if (!buildsIn)
        arrivingTo.objects.append(object);
    }
    arrivingTo.timeline = Timeline();

    return Morph::stateAt(settledFrom, arrivingTo,
                          Morph::match(settledFrom, arrivingTo),
                          frame.transitionProgress);
  }
  return Evaluator::stateAt(Design::resolve(document, frame.slideIndex),
                            frame.slideTime);
}

QColor Presentation::backgroundAt(const Document &document, qreal t,
                                  bool includeSkipped) {
  if (document.slides.isEmpty())
    return QColor(Qt::black);
  const auto frame = frameAt(document, t, includeSkipped);
  if (frame.slideIndex < 0)
    return QColor(Qt::black);
  const QColor to = Design::resolve(document, frame.slideIndex).background;
  if (!frame.inTransition)
    return to;
  const QColor from = Design::resolve(document, frame.fromSlide).background;
  const qreal p = QEasingCurve(QEasingCurve::InOutCubic)
                      .valueForProgress(frame.transitionProgress);
  return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * p,
                          from.greenF() + (to.greenF() - from.greenF()) * p,
                          from.blueF() + (to.blueF() - from.blueF()) * p,
                          from.alphaF() + (to.alphaF() - from.alphaF()) * p);
}

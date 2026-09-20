#include "anim/presentation.h"
#include "core/design.h"

#include "anim/evaluator.h"
#include "anim/morph.h"
#include "core/scene.h"

#include <QEasingCurve>
#include <algorithm>

namespace {

// A slide's own background, as something that can move with it.
SceneObject backdrop(const QSizeF &size, const QColor &color, const QString &id) {
  SceneObject object;
  object.id = id;
  object.type = ObjectType::Rect;
  object.rect = QRectF(QPointF(), size);
  object.fill = color;
  object.strokeWidth = 0;
  return object;
}

} // namespace

int Presentation::transitionKind(const Document &document, int index) {
  if (index < 0 || index >= document.slides.size()) return document.transition;
  const int own = document.slides.at(index).transition;
  const int kind = own < 0 ? document.transition : own;
  return kind < Cut || kind > Morph ? Morph : kind;
}

qreal Presentation::transitionSeconds(const Document &document, int index) {
  if (transitionKind(document, index) == Cut) return 0;
  const qreal own = index >= 0 && index < document.slides.size()
                        ? document.slides.at(index).transitionSeconds : -1;
  const qreal seconds = own < 0 ? document.transitionDuration : own;
  return qBound(qreal(0), seconds, qreal(10));
}

qreal Presentation::hold(const Document &document, int index) {
  if (index < 0 || index >= document.slides.size()) return kHold;
  const qreal advance = document.slides.at(index).advanceAfter;
  return advance < 0 ? kHold : qBound(qreal(0), advance, qreal(3600));
}

qreal Presentation::slideDuration(const Document &document, int slideIndex) {
  if (slideIndex < 0 || slideIndex >= document.slides.size())
    return 0.0;
  return document.slides.at(slideIndex).timeline.duration() + hold(document, slideIndex);
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
  const auto indices = slideIndices(document, includeSkipped);
  for (int n = 0; n < indices.size(); ++n) {
    const int i = indices.at(n);
    if (i >= index)
      return start;
    start += slideDuration(document, i);
    if (n + 1 < indices.size()) start += transitionSeconds(document, indices.at(n + 1));
  }
  return duration(document, includeSkipped);
}
qreal Presentation::duration(const Document &document, bool includeSkipped) {
  const auto indices = slideIndices(document, includeSkipped);
  qreal total = 0;
  for (int n = 0; n < indices.size(); ++n) {
    total += slideDuration(document, indices.at(n));
    if (n > 0) total += transitionSeconds(document, indices.at(n));
  }
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
    const qreal seconds = transitionSeconds(document, indices.at(n + 1));
    if (t < cursor + seconds) {
      frame.inTransition = true;
      frame.fromSlide = i;
      frame.slideIndex = indices.at(n + 1);
      frame.transitionProgress = seconds > 0 ? (t - cursor) / seconds : 1;
      return frame;
    }
    cursor += seconds;
  }
  return frame;
}

QVector<SceneObject> Presentation::blend(const Document &document, const Slide &from,
                                         const Slide &to, const QVector<MorphPair> &pairs,
                                         int kind, int direction, qreal progress) {
  const qreal p = qBound(qreal(0), progress, qreal(1));
  if (kind == Morph) return Morph::stateAt(from, to, pairs, p);
  if (kind == Cut) return p < 1 ? from.objects : to.objects;
  const qreal eased = QEasingCurve(QEasingCurve::InOutCubic).valueForProgress(p);
  QVector<SceneObject> states;
  if (kind == Fade) {
    for (auto object : from.objects) { object.opacity *= 1 - eased; states.append(object); }
    for (auto object : to.objects) { object.opacity *= eased; states.append(object); }
    return states;
  }
  // Push: each slide carries its own background across the screen.
  const QSizeF size = document.size;
  const bool horizontal = direction == 0 || direction == 1;
  const qreal travel = horizontal ? size.width() : size.height();
  const qreal sign = direction == 0 || direction == 2 ? -1 : 1;
  const QPointF leaving = horizontal ? QPointF(travel * eased * sign, 0)
                                     : QPointF(0, travel * eased * sign);
  const QPointF arriving = horizontal ? QPointF(travel * (eased - 1) * sign, 0)
                                      : QPointF(0, travel * (eased - 1) * sign);
  states.append(backdrop(size, from.background, QStringLiteral("@transition/from")));
  states += from.objects;
  const int outgoing = states.size();
  states.append(backdrop(size, to.background, QStringLiteral("@transition/to")));
  states += to.objects;
  for (int i = 0; i < states.size(); ++i)
    states[i].rect.translate(i < outgoing ? leaving : arriving);
  return states;
}

QColor Presentation::blendBackground(const QColor &from, const QColor &to, int kind,
                                     qreal progress) {
  // A push paints its own backgrounds, so what lies behind them is the slide
  // being arrived at from the first frame.
  if (kind == Push) return to;
  if (kind == Cut) return progress < 1 ? from : to;
  const qreal p = QEasingCurve(QEasingCurve::InOutCubic)
                      .valueForProgress(qBound(qreal(0), progress, qreal(1)));
  return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * p,
                          from.greenF() + (to.greenF() - from.greenF()) * p,
                          from.blueF() + (to.blueF() - from.blueF()) * p,
                          from.alphaF() + (to.alphaF() - from.alphaF()) * p);
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
        if (step->startsHidden()) buildsIn = true;
      if (!buildsIn)
        arrivingTo.objects.append(object);
    }
    arrivingTo.timeline = Timeline();

    const int kind = transitionKind(document, frame.slideIndex);
    return blend(document, settledFrom, arrivingTo,
                 kind == Morph ? Morph::match(settledFrom, arrivingTo) : QVector<MorphPair>(),
                 kind, document.slides.at(frame.slideIndex).transitionDirection,
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
  return blendBackground(from, to, transitionKind(document, frame.slideIndex),
                         frame.transitionProgress);
}

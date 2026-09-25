#include "anim/presentation.h"
#include "core/deckresize.h"
#include "core/design.h"

#include "anim/evaluator.h"
#include "anim/morph.h"
#include "core/scene.h"

#include <QEasingCurve>
#include <QtMath>
#include <cmath>
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

// Everything on a slide, grown or shrunk and turned about the middle of it.
void around(QVector<SceneObject> &objects, const QPointF &centre, qreal scale, qreal degrees) {
  const qreal radians = qDegreesToRadians(degrees);
  const qreal c = std::cos(radians), s = std::sin(radians);
  for (auto &object : objects) {
    if (!qFuzzyCompare(scale, 1.0)) DeckResize::scaleObject(object, qMax(0.001, scale), centre);
    if (qFuzzyIsNull(degrees)) continue;
    const QPointF offset = object.rect.center() - centre;
    const QPointF turned(offset.x() * c - offset.y() * s, offset.x() * s + offset.y() * c);
    object.rect.translate(turned - offset);
    object.rotation += degrees;
  }
}

} // namespace

int Presentation::transitionKind(const Document &document, int index) {
  if (index < 0 || index >= document.slides.size()) return document.transition;
  const int own = document.slides.at(index).transition;
  const int kind = own < 0 ? document.transition : own;
  return kind < Cut || kind > LastKind ? Morph : kind;
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
  // A custom show is the deck in a different order, not a different deck.
  if (!document.activeShow.isEmpty()) {
    for (const auto &id : document.activeShow)
      for (int i = 0; i < document.slides.size(); ++i)
        if (document.slides.at(i).id == id &&
            (includeSkipped || !document.slides.at(i).skipped))
          indices.append(i);
    return indices;
  }
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
    // A custom show can put the slides in any order, so this looks for the
    // slide rather than assuming it comes after the ones already counted.
    if (indices.at(n) == index || (document.activeShow.isEmpty() && indices.at(n) >= index))
      return start;
    start += slideDuration(document, indices.at(n));
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
  const QSizeF size = document.size;
  const QPointF centre(size.width() / 2, size.height() / 2);
  if (kind == FadeThroughBlack) {
    // Out to black over the first half, in from it over the second.
    const qreal out = qBound(qreal(0), p * 2, qreal(1)), in = qBound(qreal(0), p * 2 - 1, qreal(1));
    const QEasingCurve curve(QEasingCurve::InOutQuad);
    for (auto object : from.objects) { object.opacity *= 1 - curve.valueForProgress(out); states.append(object); }
    for (auto object : to.objects) { object.opacity *= curve.valueForProgress(in); states.append(object); }
    return states;
  }
  if (kind == Zoom) {
    // The next slide grows out of the middle as this one fades behind it.
    for (auto object : from.objects) { object.opacity *= 1 - eased; states.append(object); }
    QVector<SceneObject> arriving = to.objects;
    around(arriving, centre, 0.25 + 0.75 * eased, 0);
    for (auto &object : arriving) { object.opacity *= eased; states.append(object); }
    return states;
  }
  if (kind == Whirl) {
    // The next slide, background and all, spins in from nothing over this one.
    states.append(backdrop(size, from.background, QStringLiteral("@transition/from")));
    states += from.objects;
    QVector<SceneObject> arriving{backdrop(size, to.background, QStringLiteral("@transition/to"))};
    arriving += to.objects;
    around(arriving, centre, qMax(0.001, eased), 360.0 * (1 - eased));
    states += arriving;
    return states;
  }
  // Push, Cover and Uncover: slides that travel, each carrying its own
  // background across the screen.
  const bool horizontal = direction == 0 || direction == 1;
  const qreal travel = horizontal ? size.width() : size.height();
  const qreal sign = direction == 0 || direction == 2 ? -1 : 1;
  const QPointF leaving = horizontal ? QPointF(travel * eased * sign, 0)
                                     : QPointF(0, travel * eased * sign);
  const QPointF arriving = horizontal ? QPointF(travel * (eased - 1) * sign, 0)
                                      : QPointF(0, travel * (eased - 1) * sign);
  QVector<SceneObject> leavingSlide{backdrop(size, from.background, QStringLiteral("@transition/from"))};
  leavingSlide += from.objects;
  QVector<SceneObject> arrivingSlide{backdrop(size, to.background, QStringLiteral("@transition/to"))};
  arrivingSlide += to.objects;
  // Cover leaves this slide still and brings the next one over it; Uncover
  // leaves the next one still beneath and takes this one away.
  if (kind != Cover) for (auto &object : leavingSlide) object.rect.translate(leaving);
  if (kind != Uncover) for (auto &object : arrivingSlide) object.rect.translate(arriving);
  if (kind == Uncover) return arrivingSlide + leavingSlide;
  return leavingSlide + arrivingSlide;
}

QColor Presentation::blendBackground(const QColor &from, const QColor &to, int kind,
                                     qreal progress) {
  // A push paints its own backgrounds, so what lies behind them is the slide
  // being arrived at from the first frame.
  if (kind == Push || kind == Cover || kind == Uncover) return to;
  if (kind == Cut || kind == Whirl) return progress < 1 ? from : to;
  if (kind == FadeThroughBlack) {
    const qreal p = qBound(qreal(0), progress, qreal(1));
    const QColor black(0, 0, 0);
    const auto mix = [](const QColor &a, const QColor &b, qreal t) {
      return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                              a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
    };
    return p < 0.5 ? mix(from, black, p * 2) : mix(black, to, p * 2 - 1);
  }
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

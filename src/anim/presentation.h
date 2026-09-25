#pragma once

// Global deck time -> what is on screen.
//
// This is where "a deck is a pure function of t" becomes true across slide
// boundaries and not just within one slide. Every consumer — the live view, the
// scrubber, the PNG shot harness, and later the video encoder — asks this one
// question and gets the same answer.

#include <QColor>
#include <QPointF>
#include <QVector>

#include "anim/morph.h"

struct Document;
struct Slide;
struct SceneObject;

struct Frame {
  int slideIndex =
      -1; // the slide being shown, or the incoming one mid-transition
  qreal slideTime = 0.0; // seconds into that slide's timeline
  bool inTransition = false;
  int fromSlide = 0;              // valid when inTransition
  qreal transitionProgress = 0.0; // 0..1, valid when inTransition
};

namespace Presentation {

// Seconds a slide holds after its builds finish, before the next transition,
// unless the slide asks for its own.
constexpr qreal kHold = 0.8;

// How a show gets from one slide to the next. Cut has no frames of its own;
// Push carries each slide's background with it; Morph interpolates the objects
// the two slides have in common. Cover slides the next slide over this one,
// Uncover slides this one away to show the next beneath; FadeThroughBlack
// fades out to black and the next slide in from it; Zoom grows the next slide
// out of the middle; Whirl spins it in from nothing. Each is one PowerPoint
// also has, so a deck keeps its transitions both ways.
enum Kind { Cut = 0, Fade = 1, Push = 2, Morph = 3, Cover = 4, Uncover = 5,
            FadeThroughBlack = 6, Zoom = 7, Whirl = 8, LastKind = Whirl };

// Whether a transition travels, and so has a direction to choose.
inline bool hasDirection(int kind) { return kind == Push || kind == Cover || kind == Uncover; }

// Always about the slide being arrived at, as Keynote and PowerPoint both read.
int transitionKind(const Document &document, int index);
// Moves the object as if it were scaled about origin and draws it that much
// bigger, leaving its size and type as they are (see SceneObject::paintScale).
void scaleAbout(SceneObject &object, qreal scale, const QPointF &origin);
qreal transitionSeconds(const Document &document, int index);
qreal hold(const Document &document, int index);

// One composition of two prepared slides, shared by the direct evaluator and
// the cached one so a transition looks the same wherever it is drawn from.
QVector<SceneObject> blend(const Document &document, const Slide &from, const Slide &to,
                           const QVector<MorphPair> &pairs, int kind, int direction,
                           qreal progress);
QColor blendBackground(const QColor &from, const QColor &to, int kind, qreal progress);

qreal slideDuration(const Document &document, int slideIndex);
QVector<int> slideIndices(const Document &document,
                          bool includeSkipped = false);
qreal slideStart(const Document &document, int index,
                 bool includeSkipped = false);
qreal duration(const Document &document, bool includeSkipped = false);

Frame frameAt(const Document &document, qreal t, bool includeSkipped = false);
QColor backgroundAt(const Document &document, qreal t,
                    bool includeSkipped = false);
QVector<SceneObject> stateAt(const Document &document, qreal t,
                             bool includeSkipped = false);

} // namespace Presentation

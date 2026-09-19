#pragma once

// Global deck time -> what is on screen.
//
// This is where "a deck is a pure function of t" becomes true across slide
// boundaries and not just within one slide. Every consumer — the live view, the
// scrubber, the PNG shot harness, and later the video encoder — asks this one
// question and gets the same answer.

#include <QColor>
#include <QVector>

struct Document;
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

// Seconds a slide holds after its builds finish, before the next transition.
constexpr qreal kHold = 0.8;

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

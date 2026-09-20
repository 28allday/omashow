#pragma once

// A deck, as JSON, for something that cannot look at it.
//
// Everything an agent needs to aim at: what the slides are, what is on them and
// where, what each object is called, what builds and transitions are set, and
// what the review makes of the whole thing. It is a reading of the same
// resolved state the canvas draws, so what it says is what would be shown.

#include <QVariantMap>

#include "core/scene.h"

class Backend;

namespace Cli {

// `full` keeps the heavy properties — path data, table and chart contents,
// per-range formatting — which are left out by default so a deck can be read
// at a glance.
QVariantMap describeDeck(const Document &document, bool full = false);
QVariantMap describeSlide(const Document &document, int index, bool full = false);
QVariantMap describeReview(const Document &document, bool includeDismissed = false);
} // namespace Cli

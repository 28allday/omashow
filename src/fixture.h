#pragma once

// The Gate 0 fixture deck.
//
// Two slides built so that all three Morph tiers fire at once: the title and
// the metric card carry the same ids across both slides (identity), the caption
// keeps its words but changes id (scored), and one object exists on each slide
// alone (fade). Colours here are *document* colours — interface tokens must
// never leak into slide content.

struct Document;

namespace Fixture {

Document twoSlideMorph();

} // namespace Fixture

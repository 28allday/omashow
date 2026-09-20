#pragma once

// The marks people mean rather than the ones on the keyboard.
//
// Straight quotes become the quotes that face the right way, a pair of hyphens
// becomes an en dash and three an em dash, and three full stops become an
// ellipsis. It runs when words are committed, never while they are being typed
// into, so nothing changes under the cursor; an equation is left exactly as it
// was written.

#include <QString>

namespace Punctuation {
QString smarten(const QString &text);
}

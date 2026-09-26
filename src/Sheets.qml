pragma Singleton

import QtQuick

// How many sheets (and menus) are in front of the deck.
//
// The shell's own shortcuts stand down while one is open: a sheet that has the
// keyboard has to keep it, or Escape, Delete and the single letters that insert
// things would reach the deck behind it instead of the dialog in front.
QtObject {
    property int count: 0
    function opened() { count += 1 }
    function closed() { if (count > 0) count -= 1 }
}

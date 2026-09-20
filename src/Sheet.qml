import QtQuick
import QtQuick.Controls
import Omashow 1.0

// The one kind of dialog this app puts in front of someone.
//
// It is a Dialog that also takes the keyboard while it is open, and says so, so
// Escape dismisses it, Tab stays inside it, and the shortcuts behind it stand
// down. Counting on `visible` rather than `opened` keeps the books straight for
// a sheet closed before its opening transition finished.
Dialog {
    id: sheet
    focus: true
    property bool counted: false
    onVisibleChanged: {
        if (visible && !counted) { counted = true; Sheets.opened() }
        else if (!visible && counted) { counted = false; Sheets.closed() }
    }
    Component.onDestruction: if (counted) Sheets.closed()
}

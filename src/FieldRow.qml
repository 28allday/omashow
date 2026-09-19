import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Inspector row as drawn in the concept: a label in a fixed left column, the
// controls filling the rest. Children go after the label.
RowLayout {
    property alias label: caption.text
    Layout.fillWidth: true
    spacing: Theme.s2
    Label {
        id: caption
        Layout.preferredWidth: Theme.wFieldLabel
        Layout.alignment: Qt.AlignVCenter
        color: Theme.textSecondary
        elide: Text.ElideRight
    }
}

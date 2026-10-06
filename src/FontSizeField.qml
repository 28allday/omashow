import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Type size in points, the way Office does it: half points, and grow / shrink
// buttons that walk the usual ladder (1 pt steps to 30, 2 pt to 70, then 5 pt).
// Works in points only; the caller converts to and from slide units.
RowLayout {
    id: root
    property real value: 0
    signal committed(real value)
    spacing: Theme.s1

    // Hundredths absorb the float noise of the unit conversion (55.5314 pt).
    function clean(pt) { return Math.round(pt * 100) / 100 }
    // Nearest half point, never under 1.
    function snap(pt) { return Math.max(1, Math.round(pt * 2) / 2) }
    // The next ladder value above / below, from on or off the ladder.
    function grow(pt) {
        const p = clean(pt)
        if (p < 30) return Math.floor(p) + 1
        if (p < 70) return 30 + (Math.floor((p - 30) / 2) + 1) * 2
        return 70 + (Math.floor((p - 70) / 5) + 1) * 5
    }
    function shrink(pt) {
        const p = clean(pt)
        if (p > 70) return 70 + (Math.ceil((p - 70) / 5) - 1) * 5
        if (p > 30) return 30 + (Math.ceil((p - 30) / 2) - 1) * 2
        return Math.max(1, Math.ceil(p) - 1)
    }

    component SizeStep: Button {
        id: stepButton
        property string tip: ""
        property string caret: ""
        property bool mark: true
        rightPadding: Theme.s1 + caretMark.implicitWidth
        ToolTip.visible: hovered && tip !== ""
        ToolTip.delay: Theme.tooltipDelay
        ToolTip.text: tip
        Accessible.name: tip
        Text {
            id: caretMark
            anchors.right: parent.right
            anchors.rightMargin: Theme.s1 - 1
            anchors.verticalCenter: parent.verticalCenter
            text: stepButton.caret
            color: Theme.textSecondary
            font.pixelSize: 8
        }
    }

    NumField {
        id: field
        objectName: "textSize"
        Layout.preferredWidth: Theme.s5 * 3 + Theme.s3
        suffix: " pt"
        decimals: 1
        value: root.snap(root.value)
        onCommitted: v => root.committed(root.snap(v))
    }
    SizeStep { objectName: "textSizeGrow"; text: "A"; caret: "\u25B4"; font.pixelSize: Theme.fsControl + 3
               tip: qsTr("Increase font size"); onClicked: root.committed(root.grow(root.value)) }
    SizeStep { objectName: "textSizeShrink"; text: "A"; caret: "\u25BE"; font.pixelSize: Theme.fsControl - 2
               tip: qsTr("Decrease font size"); onClicked: root.committed(root.shrink(root.value)) }
}

import QtQuick
import QtQuick.Controls
import Omashow 1.0

// A numeric inspector field. Commits on editing-finished rather than on every
// keystroke, so typing "120" is one undo step and not three.
Item {
    id: root
    property string label
    property real value: 0
    property real step: 1
    property string suffix: ""
    signal committed(real value)

    // Places shown; past whole numbers, trailing zeros are dropped ("55.5", "24").
    property int decimals: root.step < 1 ? 2 : 0
    function shown() {
        const t = root.value.toFixed(root.decimals)
        return root.decimals > 0 ? String(Number(t)) : t
    }

    implicitWidth: Theme.s5*4
    implicitHeight: Theme.hControl

    Text {
        id: caption
        anchors.verticalCenter: parent.verticalCenter
        width: root.label.length > 0 ? Math.max(Theme.s4,implicitWidth+Theme.s2) : 0
        text: root.label
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fsControl
    }

    Rectangle {
        anchors.left: caption.right
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: Theme.hControl
        color: Theme.controlBg
        radius: Theme.rControl
        border.width: Theme.hairline
        border.color: input.activeFocus ? Theme.accent : hover.hovered ? Theme.borderStrong : Theme.border
        HoverHandler { id: hover }

        // The unit sits beside the number, not in it: with the unit inside the
        // edited text the validator never saw an acceptable number, so typing
        // "150" in front of " pt" was never committed.
        TextInput {
            id: input
            anchors.left: parent.left
            anchors.right: parent.right
            // Room for the unit; its own width is left implicit (setting it
            // made a width binding loop on the Text).
            anchors.rightMargin: Theme.s2 + (unit.visible ? unit.implicitWidth + Theme.s1 : 0)
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.s2
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fsControl
            selectByMouse: true
            clip: true
            text: root.shown()
            validator: DoubleValidator {}
            onEditingFinished: {
                const v = parseFloat(text)
                // Only what was retyped: leaving the field untouched must not
                // commit the rounded figure it shows over the exact value.
                if (!isNaN(v) && text !== root.shown()) root.committed(v)
                // Back to what the value says, whether or not it moved.
                text = Qt.binding(root.shown)
            }
            Keys.onUpPressed: root.committed(root.value + root.step)
            Keys.onDownPressed: root.committed(root.value - root.step)
        }
        Text {
            id: unit
            anchors.right: parent.right
            anchors.rightMargin: Theme.s2
            anchors.verticalCenter: parent.verticalCenter
            text: root.suffix.trim()
            visible: text.length > 0
            horizontalAlignment: Text.AlignRight
            color: Theme.textSecondary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fsControl
        }
    }
}

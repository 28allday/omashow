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

        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: Theme.s2
            anchors.rightMargin: Theme.s2
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fsControl
            selectByMouse: true
            text: root.value.toFixed(root.step < 1 ? 2 : 0) + root.suffix
            validator: DoubleValidator {}
            onEditingFinished: {
                const v = parseFloat(text)
                if (!isNaN(v)) root.committed(v)
            }
            Keys.onUpPressed: root.committed(root.value + root.step)
            Keys.onDownPressed: root.committed(root.value - root.step)
        }
    }
}

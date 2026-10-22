import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// COLOR-PICK-1: a colour setting everywhere in the app: the swatch (click it
// for the colour chooser) and the hex code beside it. `picked` fires with the
// new code from either. The chooser is made when opened and gone when closed.
RowLayout {
    id: root
    property string value: ""
    property string placeholder: ""
    property string accessibleName: qsTr("Colour")
    property alias field: hex
    signal picked(string value)

    spacing: Theme.s1
    Layout.fillWidth: true

    readonly property bool valid: /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(root.value)

    Rectangle {
        objectName: "colorSwatch"
        implicitWidth: Theme.hControl + Theme.s3
        implicitHeight: Theme.hControl
        radius: Theme.rControl
        color: root.valid ? root.value : Theme.controlBg
        border.color: swatchHover.hovered ? Theme.accent : Theme.borderStrong
        border.width: Theme.hairline
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Choose %1").arg(root.accessibleName.toLowerCase())
        HoverHandler { id: swatchHover; cursorShape: Qt.PointingHandCursor }
        ToolTip.visible: swatchHover.hovered; ToolTip.delay: Theme.tooltipDelay
        ToolTip.text: qsTr("Choose a colour")
        TapHandler {
            onTapped: {
                Sheets.holdsTextEdit = true
                const picker = chooser.createObject(root)
                picker.chosen.connect(c => root.picked(c))
                picker.closed.connect(() => { Sheets.holdsTextEdit = false; picker.destroy() })
                picker.openWith(root.value)
            }
        }
    }
    TextField {
        id: hex
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: root.value
        placeholderText: root.placeholder
        font.family: Theme.monoFamily
        Accessible.name: root.accessibleName
        onEditingFinished: root.picked(text)
    }
    Component { id: chooser; ColorPicker {} }
}

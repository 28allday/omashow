import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
ColumnLayout {
    id: root
    property var sel: backend.selection
    spacing: Theme.s2
    SectionLabel { text: qsTr("SHAPE") }
    ComboBox { objectName: "shapeKind"; Layout.fillWidth: true; model: backend.shapeNames; currentIndex: root.sel.shapeKind ?? 0; displayText: root.sel.shapeKind===99?qsTr("Editable path"):currentText; onActivated: backend.setSelectedProperty("shapeKind",currentIndex) }
    SectionLabel { text: qsTr("FILL TYPE") }
    ComboBox { objectName: "shapeFillStyle"; Layout.fillWidth: true; model: [qsTr("Solid"),qsTr("Linear gradient"),qsTr("Radial gradient"),qsTr("Pattern"),qsTr("Picture"),qsTr("None")]; currentIndex: root.sel.fillStyle ?? 0; onActivated: { if(currentIndex===4 && !root.sel.imageId) backend.setShapeImageDialog(); else backend.setSelectedProperty("fillStyle",currentIndex) } }
    TextField { objectName: "shapeSecondaryColor"; Layout.fillWidth: true; visible: (root.sel.fillStyle??0)>=1 && (root.sel.fillStyle??0)<=3; text: root.sel.fillSecondary ?? "#ffffff"; placeholderText: qsTr("Second colour"); Accessible.name: qsTr("Second fill colour"); onEditingFinished: backend.setSelectedProperty("fillSecondary",text) }
    NumField { objectName: "shapeFillAngle"; Layout.fillWidth: true; visible: root.sel.fillStyle===1; label: qsTr("Angle"); value: root.sel.fillAngle ?? 0; onCommitted: v=>backend.setSelectedProperty("fillAngle",v) }
    ComboBox { Layout.fillWidth: true; visible: root.sel.fillStyle===3; model: [qsTr("Dense 1"),qsTr("Dense 2"),qsTr("Dense 3"),qsTr("Dense 4"),qsTr("Dense 5"),qsTr("Dense 6"),qsTr("Dense 7"),qsTr("Horizontal"),qsTr("Vertical"),qsTr("Cross"),qsTr("Backward diagonal"),qsTr("Forward diagonal"),qsTr("Diagonal cross")]; currentIndex: root.sel.patternStyle ?? 9; onActivated: backend.setSelectedProperty("patternStyle",currentIndex) }
    Button { Layout.fillWidth: true; visible: root.sel.fillStyle===4; text: qsTr("Choose picture…"); onClicked: backend.setShapeImageDialog() }
    ComboBox { Layout.fillWidth: true; visible: root.sel.fillStyle===4; model: [qsTr("Fit picture"),qsTr("Fill shape"),qsTr("Stretch picture")]; currentIndex: root.sel.imageMode ?? 0; onActivated: backend.setSelectedProperty("imageMode",currentIndex) }
    SectionLabel { text: qsTr("STROKE") }
    RowLayout {
        Layout.fillWidth: true
        NumField { objectName: "shapeStrokeWidth"; Layout.preferredWidth: 140; Layout.fillWidth: true; label: qsTr("Width"); value: root.sel.strokeWidth ?? 0; onCommitted: v=>backend.setSelectedProperty("strokeWidth",v) }
        TextField { objectName: "shapeStrokeColor"; Layout.minimumWidth: 0; Layout.preferredWidth: 120; Layout.fillWidth: true; text: root.sel.strokeColor ?? "#000000"; Accessible.name: qsTr("Stroke colour"); onEditingFinished: backend.setSelectedProperty("strokeColor",text) }
    }
    ComboBox { objectName: "shapeStrokeStyle"; Layout.fillWidth: true; model: [qsTr("Solid"),qsTr("Dash"),qsTr("Dot"),qsTr("Dash dot"),qsTr("Dash dot dot")]; currentIndex: root.sel.strokeStyle ?? 0; onActivated: backend.setSelectedProperty("strokeStyle",currentIndex) }
    RowLayout {
        ComboBox { Layout.fillWidth: true; model: [qsTr("Miter join"),qsTr("Bevel join"),qsTr("Round join")]; currentIndex: root.sel.strokeJoin ?? 1; onActivated: backend.setSelectedProperty("strokeJoin",currentIndex) }
        ComboBox { Layout.fillWidth: true; model: [qsTr("Flat cap"),qsTr("Square cap"),qsTr("Round cap")]; currentIndex: root.sel.strokeCap ?? 1; onActivated: backend.setSelectedProperty("strokeCap",currentIndex) }
    }
    CheckBox { text: qsTr("Offset shadow"); checked: root.sel.shadowEnabled ?? false; onToggled: backend.setSelectedProperty("shadowEnabled",checked) }
    ColumnLayout {
        visible: root.sel.shadowEnabled ?? false; Layout.fillWidth: true
        TextField { Layout.fillWidth: true; text: root.sel.shadowColor ?? "#64000000"; Accessible.name: qsTr("Shadow colour"); onEditingFinished: backend.setSelectedProperty("shadowColor",text) }
        RowLayout {
            NumField { Layout.fillWidth: true; label: qsTr("X"); value: root.sel.shadowX ?? 8; onCommitted: v=>backend.setSelectedProperty("shadowX",v) }
            NumField { Layout.fillWidth: true; label: qsTr("Y"); value: root.sel.shadowY ?? 8; onCommitted: v=>backend.setSelectedProperty("shadowY",v) }
        }
    }
    Button { objectName: "convertPath"; Layout.fillWidth: true; visible: root.sel.shapeKind!==99; text: qsTr("Convert to editable path"); onClicked: backend.convertToPath() }
    RowLayout {
        visible: root.sel.shapeKind===99
        Button { text: qsTr("Close path"); onClicked: backend.setPathClosed(true) }
        Button { text: qsTr("Open path"); onClicked: backend.setPathClosed(false) }
    }
}

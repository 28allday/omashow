import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
ColumnLayout {
    id: root
    property var sel: backend.selection
    readonly property var targets: [{id:"",name:qsTr("Detached endpoint")}].concat(backend.connectorTargets)
    Layout.fillWidth: true
    SectionLabel { text: qsTr("CONNECTOR") }
    ComboBox { objectName: "connectorRoute"; Layout.fillWidth: true; model: [qsTr("Straight"),qsTr("Elbow"),qsTr("Curve")]; currentIndex: root.sel.connectorRoute??1; onActivated: backend.setSelectedProperty("connectorRoute",currentIndex) }
    Repeater {
        model: [{label:qsTr("FROM"),start:true,key:"connectorFrom",side:"connectorFromSide"},{label:qsTr("TO"),start:false,key:"connectorTo",side:"connectorToSide"}]
        ColumnLayout {
            required property var modelData
            Layout.fillWidth: true
            Label { text: modelData.label; color: Theme.textMuted }
            ComboBox {
                objectName: modelData.start?"connectorFrom":"connectorTo"
                Layout.fillWidth: true; model: root.targets; textRole: "name"; valueRole: "id"
                currentIndex: root.targets.findIndex(t=>t.id===(root.sel[modelData.key]??""))
                displayText: currentIndex<0?qsTr("Missing target"):currentText
                onActivated: backend.attachConnector(modelData.start,currentValue,root.sel[modelData.side]??0)
            }
            ComboBox { Layout.fillWidth: true; visible: !!root.sel[modelData.key]; model: [qsTr("Nearest side"),qsTr("Top"),qsTr("Right"),qsTr("Bottom"),qsTr("Left")]; currentIndex: root.sel[modelData.side]??0; onActivated: backend.setSelectedProperty(modelData.side,currentIndex) }
            RowLayout {
                visible: !root.sel[modelData.key]
                NumField { Layout.fillWidth: true; label: qsTr("X"); value: modelData.start?(root.sel.connectorStartX??0):(root.sel.connectorEndX??0); onCommitted: v=>backend.setSelectedProperty(modelData.start?"connectorStartX":"connectorEndX",v) }
                NumField { Layout.fillWidth: true; label: qsTr("Y"); value: modelData.start?(root.sel.connectorStartY??0):(root.sel.connectorEndY??0); onCommitted: v=>backend.setSelectedProperty(modelData.start?"connectorStartY":"connectorEndY",v) }
            }
        }
    }
    RowLayout {
        CheckBox { text: qsTr("Start arrow"); checked: root.sel.connectorArrowStart??false; onToggled: backend.setSelectedProperty("connectorArrowStart",checked) }
        CheckBox { text: qsTr("End arrow"); checked: root.sel.connectorArrowEnd??true; onToggled: backend.setSelectedProperty("connectorArrowEnd",checked) }
    }
    NumField { Layout.fillWidth: true; label: qsTr("Width"); value: root.sel.strokeWidth??3; onCommitted: v=>backend.setSelectedProperty("strokeWidth",v) }
    TextField { Layout.fillWidth: true; text: root.sel.strokeColor??"#ffffff"; Accessible.name: qsTr("Connector colour"); onEditingFinished: backend.setSelectedProperty("strokeColor",text) }
    ComboBox { Layout.fillWidth: true; model: [qsTr("Solid"),qsTr("Dash"),qsTr("Dot"),qsTr("Dash dot"),qsTr("Dash dot dot")]; currentIndex: root.sel.strokeStyle??0; onActivated: backend.setSelectedProperty("strokeStyle",currentIndex) }
    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; text: qsTr("Attached endpoints follow their objects. Detach into a path to edit its points freely.") }
    Button { objectName: "detachConnector"; text: qsTr("Detach into path"); onClicked: backend.convertToPath() }
}

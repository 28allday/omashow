import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Dialog {
    id: root; objectName: "diagramDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,1040)
    height: Math.min(parent.height-Theme.s4*2,760)
    modal: true; title: qsTr("Insert diagram")
    standardButtons: Dialog.Apply | Dialog.Cancel
    readonly property var preview: backend.diagramPreview
    background: Rectangle {color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl}
    Overlay.modal: Rectangle {color: Theme.withAlpha(Theme.showBg,.65)}
    function example(type) {return type===0 ? "Plan\nCreate\nReview\nDeliver" : "Project\n  Design\n    Research\n    Prototype\n  Delivery\n    Build\n    Verify"}
    function update() {backend.previewDiagram(kind.currentIndex,outline.text,direction.currentIndex===1)}
    function show() {update();open()}
    onOpened: {standardButton(Dialog.Apply).objectName="applyDiagram"; standardButton(Dialog.Apply).enabled=preview.ok ?? false}
    onPreviewChanged: if(visible) standardButton(Dialog.Apply).enabled=preview.ok ?? false
    onApplied: {debounce.stop();update();if(backend.insertDiagram())close()}
    onClosed: {debounce.stop();backend.clearDiagramPreview()}
    Timer {id: debounce; interval: 120; onTriggered: root.update()}
    Connections {target: backend; function onDocumentChanged(){if(root.visible)root.update()}}
    contentItem: ColumnLayout {
        spacing: Theme.s3
        RowLayout {
            ComboBox {id: kind; objectName: "diagramKind"; Layout.fillWidth: true; model: [qsTr("Process"),qsTr("Hierarchy")]; onActivated: {if(outline.text===root.example(1-currentIndex))outline.text=root.example(currentIndex); direction.currentIndex=currentIndex===1?1:0;root.update()}}
            ComboBox {id: direction; objectName: "diagramDirection"; Layout.fillWidth: true; model: [qsTr("Left to right"),qsTr("Top to bottom")]; onActivated: root.update()}
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: Theme.s4
            ColumnLayout {
                Layout.preferredWidth: 280; Layout.fillHeight: true
                Label {Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; text: kind.currentIndex===0 ? qsTr("One step per line, up to 12 steps.") : qsTr("One node per line. Indent children with two spaces or one tab. Up to 40 nodes and six levels.")}
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    TextArea {id: outline; objectName: "diagramOutline"; text: "Plan\nCreate\nReview\nDeliver"; textFormat: TextEdit.PlainText; wrapMode: TextEdit.NoWrap; selectByMouse: true; font.family: Theme.monoFamily; Accessible.name: qsTr("Diagram outline"); onTextChanged: if(root.visible)debounce.restart()}
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true
                Image {objectName: "diagramPreviewImage"; Layout.fillWidth: true; Layout.fillHeight: true; fillMode: Image.PreserveAspectFit; sourceSize.width: 1000; cache: false; source: root.visible ? "image://slides/diagram/"+(root.preview.revision ?? 0) : ""; Accessible.name: qsTr("Diagram preview")}
                Label {Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("%1 nodes · %2 levels").arg(root.preview.nodes ?? 0).arg(root.preview.levels ?? 0); color: Theme.textMuted}
            }
        }
        Label {objectName: "diagramIssue"; Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText; text: root.preview.error || (root.preview.warnings ?? []).join("\n"); visible: text.length>0; color: Theme.accent}
        Label {Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; text: qsTr("Insert creates a group of editable shapes, labels and connectors. Enter the group to move nodes; enter a node group to edit its label. Attached connectors follow the boxes. You can also ungroup with one undo step.")}
    }
}

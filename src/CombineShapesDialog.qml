import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Dialog {
    id: root
    objectName: "combineShapesDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,720)
    modal: true; title: qsTr("Combine shapes")
    standardButtons: Dialog.Apply | Dialog.Cancel
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
    onOpened: standardButton(Dialog.Apply).objectName="applyCombineShapes"
    onApplied: { if(backend.combineShapes(operation.currentIndex)) close(); else issue.text=qsTr("This operation has no filled result. Select overlapping, unlocked shapes.") }
    ColumnLayout {
        width: parent.width; spacing: Theme.s3
        ComboBox { id: operation; objectName: "combineOperation"; Layout.fillWidth: true; model: [qsTr("Union"),qsTr("Intersect"),qsTr("Subtract front from back"),qsTr("Divide overlaps")]; onActivated: issue.text="" }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary; text: qsTr("The preview shows the result on this slide. Pieces use the back shape’s style; its animation remains on the first piece. The result is an editable path.") }
        Image { Layout.fillWidth: true; Layout.preferredHeight: 280; fillMode: Image.PreserveAspectFit; source: "image://slides/combine/"+operation.currentIndex+"/"+backend.revision+"/"+backend.selectedIds.join(","); cache: false; sourceSize.width: 900 }
        Label { id: issue; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.accent }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Dialog {
    id: root
    objectName: "mediaPreflightDialog"
    signal editRequested()
    onOpened: backend.refreshMediaPreflight()
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,800)
    height: Math.min(parent.height-Theme.s5*2,600)
    modal: true
    title: qsTr("Media preflight")
    standardButtons: Dialog.Close
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rMenu }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.65) }
    contentItem: ColumnLayout {
        spacing: Theme.s3
        Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; text: qsTr("Embedded clips travel with this deck. Approve reads the linked file shown below for this session. Relink lets you choose a replacement; Embed includes its bytes in the deck.") }
        Button { text: qsTr("Refresh file status"); onClicked: backend.refreshMediaPreflight() }
        Label { textFormat: Text.PlainText; visible: backend.mediaPreflight.length===0; text: qsTr("This deck has no audio or video."); color: Theme.textMuted }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: Theme.s3
            model: backend.mediaPreflight
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width; height: row.implicitHeight+Theme.s3*2
                color: Theme.panelRaised; radius: Theme.rMenu; border.color: Theme.border
                ColumnLayout {
                    id: row; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.s3; spacing: Theme.s2
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Slide %1 · %2").arg(modelData.slide+1).arg(modelData.name); wrapMode: Text.Wrap; color: Theme.textPrimary }
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: modelData.state+" · "+modelData.codec+" · "+(modelData.bytes/1048576).toFixed(2)+" MiB"; wrapMode: Text.Wrap; color: Theme.accent }
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: !modelData.embedded; text: modelData.path; wrapMode: Text.WrapAnywhere; color: Theme.textMuted; font.pixelSize: Theme.fsLabel }
                    RowLayout {
                        Button { objectName: "approveMedia"+index; visible: !modelData.embedded && !modelData.approved; text: qsTr("Approve file"); enabled: !backend.busy; onClicked: backend.approveMedia(modelData.slideId,modelData.objectId) }
                        Button { objectName: "locateMedia"+index; text: qsTr("Show on slide"); onClicked: { backend.currentSlide=modelData.slide; backend.select(modelData.objectId); root.editRequested(); root.close() } }
                        Button { objectName: "relinkMedia"+index; text: qsTr("Relink…"); enabled: !backend.busy; onClicked: { backend.currentSlide=modelData.slide; backend.select(modelData.objectId); backend.relinkMediaDialog() } }
                        Button { objectName: "packageMedia"+index; visible: !modelData.embedded; enabled: modelData.approved && !backend.busy; text: qsTr("Embed"); onClicked: { backend.currentSlide=modelData.slide; backend.select(modelData.objectId); backend.embedSelectedMedia() } }
                    }
                }
            }
        }
    }
}

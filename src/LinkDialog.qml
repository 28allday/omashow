import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Dialog {
    id: root
    objectName: "linkDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,620)
    modal: true; title: qsTr("Link or presentation action")
    standardButtons: Dialog.Apply | Dialog.Cancel
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
    property string sourceSlide: ""
    property string sourceSelection: ""
    function show() { sourceSlide=backend.navigator[backend.currentSlide]?.id??""; sourceSelection=backend.selectedIds.join("/"); kind.currentIndex=backend.selection.linkKind??0; destination.text=backend.selection.linkTarget??""; slide.currentIndex=Math.max(0,backend.navigator.findIndex(s=>s.id===destination.text)); issue.text=backend.selection.linkIssue??""; open() }
    onOpened: standardButton(Dialog.Apply).objectName="applyObjectLink"
    onApplied: { if(sourceSlide!==(backend.navigator[backend.currentSlide]?.id??"") || sourceSelection!==backend.selectedIds.join("/")) { issue.text=qsTr("Selection changed. Close and reopen this editor for the intended objects."); return } const target=kind.currentIndex===3?slide.currentValue:destination.text; issue.text=backend.setObjectLink(kind.currentIndex,target??""); if(!issue.text) close() }
    ColumnLayout {
        width: parent.width; spacing: Theme.s3
        ComboBox { id: kind; objectName: "objectLinkKind"; Layout.fillWidth: true; model: [qsTr("No action"),qsTr("Web link"),qsTr("Email"),qsTr("Go to slide"),qsTr("Next"),qsTr("Previous"),qsTr("First slide"),qsTr("Last slide"),qsTr("End show")]; onActivated: issue.text="" }
        TextField { id: destination; objectName: "objectLinkTarget"; Layout.fillWidth: true; visible: kind.currentIndex===1 || kind.currentIndex===2; placeholderText: kind.currentIndex===1?"https://example.com":"name@example.com"; Accessible.name: qsTr("Link destination") }
        ComboBox { id: slide; objectName: "objectLinkSlide"; Layout.fillWidth: true; visible: kind.currentIndex===3; model: backend.navigator; textRole: "title"; valueRole: "id" }
        Label { Layout.fillWidth: true; visible: kind.currentIndex===1 || kind.currentIndex===2; wrapMode: Text.Wrap; color: Theme.textSecondary; text: qsTr("Clicking this object during the show asks the presenter to open the destination. The show pauses while the link is reviewed.") }
        Label { id: issue; objectName: "objectLinkIssue"; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.warning }
    }
}

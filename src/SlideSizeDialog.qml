import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Sheet {
    id: root
    objectName: "slideSizeDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,Theme.wInspector*2)
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
    modal: true; title: qsTr("Slide size")
    standardButtons: Dialog.Apply | Dialog.Cancel
    property int deckWidth: 1920
    property int deckHeight: 1080
    function show() { deckWidth=backend.slideSize.width; deckHeight=backend.slideSize.height; strategy.currentIndex=0; open() }
    onOpened: {
        standardButton(Dialog.Apply).objectName = "applySlideSize"
        standardButton(Dialog.Apply).enabled = Qt.binding(() => root.deckWidth !== backend.slideSize.width || root.deckHeight !== backend.slideSize.height)
    }
    onApplied: { if(backend.resizeDeck(deckWidth,deckHeight,strategy.currentIndex===0)) close() }
    ColumnLayout {
        width: parent.width; spacing: Theme.s3
        RowLayout {
            Label { text: qsTr("Width"); color: Theme.textSecondary }
            SpinBox { objectName: "resizeWidth"; from: 240; to: 10000; value: root.deckWidth; editable: true; onValueModified: root.deckWidth=value; Layout.fillWidth: true }
            Label { text: qsTr("Height"); color: Theme.textSecondary }
            SpinBox { objectName: "resizeHeight"; from: 240; to: 10000; value: root.deckHeight; editable: true; onValueModified: root.deckHeight=value; Layout.fillWidth: true }
        }
        ComboBox { id: strategy; objectName: "resizeStrategy"; Layout.fillWidth: true; model: [qsTr("Fit content proportionally and centre"),qsTr("Keep content size and position")] }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: strategy.currentIndex===0 ? qsTr("All slides, masters and layouts scale together. Content keeps its proportions.") : qsTr("The slide edges move. Content outside the new edges will be cropped in the show and exports."); color: Theme.textSecondary }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Current"); color: Theme.textMuted }
                Image { Layout.fillWidth: true; Layout.preferredHeight: Theme.wInspector/2; fillMode: Image.PreserveAspectFit; source: "image://slides/"+backend.currentSlide+"/"+(backend.navigator[backend.currentSlide]?.stamp ?? ""); sourceSize.width: 640; cache: false }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Preview · %1 × %2").arg(root.deckWidth).arg(root.deckHeight); color: Theme.textMuted }
                Image { objectName: "resizePreview"; Layout.fillWidth: true; Layout.preferredHeight: Theme.wInspector/2; fillMode: Image.PreserveAspectFit; source: "image://slides/resize/"+root.deckWidth+"/"+root.deckHeight+"/"+strategy.currentIndex+"/"+backend.currentSlide+"/"+backend.revision; sourceSize.width: 640; cache: false }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Dialog {
    id: root
    objectName: "mediaOptimisationDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,980)
    readonly property bool isPicture: backend.selection.type!=="media"
    height: Math.min(parent.height-Theme.s5*2,root.isPicture ? 660 : 760)
    modal: true; title: root.isPicture ? qsTr("Optimise picture") : qsTr("Optimise audio or video")
    standardButtons: Dialog.Apply | Dialog.Cancel
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rMenu }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.65) }
    readonly property var info: backend.mediaOptimisation
    property string sourceSlide: ""
    property string sourceObject: ""
    function show() { sourceSlide=backend.navigator[backend.currentSlide]?.id??""; sourceObject=backend.selectedId; preset.currentIndex=0; open(); backend.previewMediaOptimisation(preset.currentIndex) }
    onOpened: {
        standardButton(Dialog.Apply).objectName="applyMediaOptimisation"
        standardButton(Dialog.Apply).enabled=Qt.binding(()=>root.info.ready && !backend.busy)
    }
    onApplied: if(backend.applyMediaOptimisation(keepOriginal.checked)) close()
    onClosed: backend.discardMediaOptimisation()
    Connections { target: backend; function onSelectionChanged() { if(root.visible && (root.sourceObject!==backend.selectedId || root.sourceSlide!==(backend.navigator[backend.currentSlide]?.id??""))) root.close() } }
    contentItem: ScrollView {
        id: scroll; implicitWidth: 0; implicitHeight: 0; clip: true; contentWidth: availableWidth
    ColumnLayout {
        width: scroll.availableWidth; spacing: Theme.s3
        RowLayout {
            ComboBox { id: preset; objectName: "mediaCompressionPreset"; Layout.fillWidth: true; model: root.isPicture ? [qsTr("Photo · JPEG 85 · up to 1920 px"),qsTr("Compact · JPEG 70 · up to 1280 px"),qsTr("Transparency · PNG · up to 1920 px")] : [qsTr("Balanced · H.264/AAC · up to 1080p"),qsTr("Compact · H.264/AAC · up to 720p")]; enabled: !backend.busy; onActivated: backend.previewMediaOptimisation(currentIndex) }
            Button { text: qsTr("Regenerate"); enabled: !backend.busy; onClicked: backend.previewMediaOptimisation(preset.currentIndex) }
        }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1
                Label { text: qsTr("Original · %1 MiB").arg((Number(root.info.beforeBytes??0)/1048576).toFixed(2)); color: Theme.textPrimary }
                Image { objectName: "mediaBeforePreview"; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1; Layout.preferredHeight: 220; fillMode: Image.PreserveAspectFit; cache: false; sourceSize.width: 640; source: root.info.hasSource ? "image://slides/media-comparison/before/"+root.info.time+"/"+root.info.sourceId : "" }
                Button { visible: !root.isPicture; objectName: "playOriginalMedia"; text: qsTr("Play original"); enabled: root.info.ready && !backend.busy; onClicked: backend.playMediaComparison(false) }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1
                Label { text: root.info.ready ? qsTr("Preview · %1 MiB").arg((Number(root.info.afterBytes)/1048576).toFixed(2)) : qsTr("Preparing preview…"); color: Theme.textPrimary }
                Image { objectName: "mediaAfterPreview"; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.preferredWidth: 1; Layout.preferredHeight: 220; fillMode: Image.PreserveAspectFit; cache: false; sourceSize.width: 640; source: root.info.ready ? "image://slides/media-comparison/after/"+root.info.time+"/"+root.info.previewId : "" }
                Button { visible: !root.isPicture; objectName: "playCompressedMedia"; text: qsTr("Play preview"); enabled: root.info.ready && !backend.busy; onClicked: backend.playMediaComparison(true) }
            }
        }
        RowLayout {
            visible: !root.isPicture
            Layout.fillWidth: true
            Slider { objectName: "mediaComparisonTime"; Layout.fillWidth: true; from: 0; to: root.info.duration??1; value: root.info.time??0; enabled: root.info.ready; onMoved: backend.seekMediaComparison(value) }
            Label { text: Number(root.info.time??0).toFixed(2)+qsTr(" s"); color: Theme.textMuted; font.family: Theme.monoFamily }
            Button { text: qsTr("Stop"); enabled: root.info.playing??false; onClicked: backend.seekMediaComparison(root.info.time) }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText; color: Theme.textMuted; text: root.info.ready ? (root.isPicture ? qsTr("%1 · %2 × %3. Full source comparison. Crop, colour adjustments and object layout are retained.") : qsTr("%1 · %2 × %3. Compare the picture and sound before applying. Trim, repeats and object layout are retained.")).arg(root.info.codec).arg(root.info.width).arg(root.info.height) : qsTr("Encoding a complete preview. Your slide changes when you choose Apply.") }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; visible: root.info.ready && root.info.afterBytes>=root.info.beforeBytes; color: Theme.warning; text: qsTr("This preview is larger than its source. Keeping the current version will use less space.") }
        CheckBox { id: keepOriginal; objectName: "keepOriginalMedia"; text: qsTr("Keep original for Restore"); checked: true }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; text: keepOriginal.checked ? (root.info.originalLinked ? qsTr("The original stays linked. Restoring it needs the source file.") : qsTr("Keeping both versions increases the deck size. You can discard the original later.")) : qsTr("The saved deck will contain only the compressed version. Undo remains available in this editing session.") }
    }
    }
}

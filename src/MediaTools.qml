import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
ColumnLayout {
    id: root
    property var sel: backend.selection
    function update(key, value) {
        const options = {start: sel.mediaTrimStart, end: sel.mediaTrimEnd, loops: sel.mediaLoops, volume: sel.mediaVolume, click: sel.mediaOnClick}
        options[key] = value
        backend.setMediaPlayback(options.start, options.end, options.loops, options.volume, options.click)
    }
    spacing: Theme.s2
    Label { textFormat: Text.PlainText; text: root.sel.mediaVideo ? qsTr("VIDEO") : qsTr("AUDIO"); color: Theme.accent }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: root.sel.mediaName ?? ""; wrapMode: Text.Wrap; color: Theme.textPrimary }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: (root.sel.mediaCodec ?? "") + " · " + Number(root.sel.mediaDuration ?? 0).toFixed(2) + qsTr(" seconds"); wrapMode: Text.Wrap; color: Theme.textMuted }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: root.sel.mediaState ?? ""; wrapMode: Text.Wrap; color: Theme.textMuted }
    SectionLabel { text: qsTr("TRIM (SECONDS)") }
    RowLayout {
        NumField { objectName: "mediaTrimStart"; Layout.fillWidth: true; label: qsTr("In"); value: root.sel.mediaTrimStart ?? 0; step: .01; onCommitted: v => root.update("start",v) }
        NumField { objectName: "mediaTrimEnd"; Layout.fillWidth: true; label: qsTr("Out"); value: root.sel.mediaTrimEnd ?? 0; step: .01; onCommitted: v => root.update("end",v) }
    }
    NumField { objectName: "mediaLoops"; Layout.fillWidth: true; label: qsTr("Plays"); value: root.sel.mediaLoops ?? 1; step: 1; onCommitted: v => root.update("loops",v) }
    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("1 plays once. Repeat up to 100 times."); wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel }
    NumField { objectName: "mediaVolume"; Layout.fillWidth: true; label: qsTr("Volume %"); value: Math.round((root.sel.mediaVolume ?? 1)*100); step: 5; onCommitted: v => root.update("volume",v/100) }
    ComboBox { objectName: "mediaStartMode"; Layout.fillWidth: true; model: [qsTr("Automatically"),qsTr("On next click")]; currentIndex: root.sel.mediaOnClick ? 1 : 0; onActivated: root.update("click",currentIndex===1) }
    RowLayout {
        Button { objectName: "previewMedia"; Layout.fillWidth: true; text: qsTr("Preview clip"); onClicked: backend.previewMedia() }
        Button { objectName: "stopMedia"; text: qsTr("Stop"); onClicked: backend.stopMedia() }
    }
    Button { objectName: "replaceMedia"; Layout.fillWidth: true; text: qsTr("Relink or replace…"); onClicked: backend.relinkMediaDialog() }
    Button { objectName: "embedMedia"; Layout.fillWidth: true; visible: (root.sel.mediaPath ?? "") !== ""; text: qsTr("Embed in deck"); onClicked: backend.embedSelectedMedia() }
    Button { objectName: "optimiseMedia"; Layout.fillWidth: true; text: qsTr("Optimise…"); onClicked: backend.optimiseSelectedMedia() }
    Button { objectName: "restoreOriginalMedia"; Layout.fillWidth: true; visible: root.sel.mediaHasOriginal??false; text: qsTr("Restore original"); onClicked: backend.restoreOriginalMedia() }
    Button { objectName: "discardOriginalMedia"; Layout.fillWidth: true; visible: root.sel.mediaHasOriginal??false; text: qsTr("Discard original to reduce deck size"); onClicked: backend.discardOriginalMedia() }
    Rectangle { Layout.fillWidth: true; height: Theme.hairline; color: Theme.border }
}

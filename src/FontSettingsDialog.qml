import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Everything about the free fonts behind one menu item (Format ▸ Free fonts…):
// whether decks are checked on opening, the Extra fonts pack, what has been
// downloaded. The Extra fonts pack is one checkbox and nothing else: ticking
// it asks, then downloads with progress; unticking asks, then removes what
// was downloaded with progress. No second button for either.
Sheet {
    id: root
    objectName: "fontSettingsDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width - Theme.s5 * 2, 560)
    modal: true
    title: qsTr("Free fonts")
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, .65) }

    // Opens the Extra fonts pack view (the download, its progress, cancel).
    signal packRequested()

    property var rows: []
    property bool listed: false
    readonly property real megabytes: rows.reduce((n, r) => n + (r.bytes || 0), 0) / 1048576

    function show() { rows = backend.downloadedFonts(); listed = false; open() }
    Connections {
        target: backend
        enabled: root.visible
        function onPackFinished() { root.rows = backend.downloadedFonts() }
        function onFontRemoveFinished() { root.rows = backend.downloadedFonts() }
    }

    ColumnLayout {
        width: parent.width; spacing: Theme.s3

        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
            text: qsTr("Presentations made in PowerPoint use Microsoft Office fonts. OmaShow draws them with free, openly licensed look-alikes (not Microsoft's fonts), kept in its own folder. Your system fonts are not changed.")
        }

        ColumnLayout {
            spacing: 0
            CheckBox {
                objectName: "fontSettingsCheck"
                text: qsTr("Check fonts when a presentation opens")
                checked: backend.fontCheckOnOpen
                onToggled: backend.fontCheckOnOpen = checked
            }
            CheckBox {
                objectName: "fontSettingsAuto"
                enabled: backend.fontCheckOnOpen
                text: qsTr("Always download missing free fonts")
                checked: backend.fontAutoDownload
                onToggled: backend.fontAutoDownload = checked
            }
            CheckBox {
                objectName: "fontSettingsPack"
                enabled: backend.fontCheckOnOpen && !backend.fontRemoveRunning
                text: qsTr("Extra fonts pack")
                checked: backend.extraFontsPack
                // The tick follows the setting; a click only asks. Yes to the
                // download turns it on, yes to the removal turns it off.
                onClicked: {
                    const want = checked
                    checked = Qt.binding(() => backend.extraFontsPack)
                    if (want) { root.close(); root.packRequested() } else removeSheet.open()
                }
            }
            Label {
                Layout.leftMargin: Theme.s5; Layout.fillWidth: true; wrapMode: Text.Wrap
                color: Theme.textSecondary; font.pixelSize: Theme.fsBase - 1
                text: backend.packStatus !== "" ? backend.packStatus
                      : qsTr("Free look-alikes for every Office font, so nothing has to be fetched when a presentation opens.")
            }
        }

        // What is here, and the list of it (the list's room is always kept).
        Label {
            objectName: "fontSettingsListToggle"
            text: (root.listed ? "▾ " : "▸ ") + (root.rows.length === 1 ? qsTr("1 downloaded font") : qsTr("%1 downloaded fonts").arg(root.rows.length)) + qsTr(" · %1 MB").arg(root.megabytes.toFixed(0))
            color: Theme.accent
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.listed = !root.listed }
        }
        ListView {
            objectName: "fontSettingsList"
            visible: root.listed
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            clip: true
            model: root.rows
            spacing: Theme.s1
            ScrollBar.vertical: ScrollBar {}
            delegate: Label {
                required property var modelData
                width: ListView.view.width - Theme.s2
                elide: Text.ElideRight
                text: qsTr("%1 · %2 MB").arg(modelData.family).arg((modelData.bytes / 1048576).toFixed(1))
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            Label {
                objectName: "fontSettingsFolder"
                text: qsTr("Fonts folder")
                font.pixelSize: Theme.fsBase - 1; color: Theme.textSecondary; font.underline: true
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: backend.openFontSandbox() }
            }
            Item { Layout.fillWidth: true }
            Button { objectName: "fontSettingsClose"; text: qsTr("Close"); highlighted: true; onClicked: root.close() }
        }
    }

    // Unticking the pack: asked first, then removed with progress.
    Sheet {
        id: removeSheet
        objectName: "fontRemoveDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(parent.width - Theme.s5 * 2, 440)
        modal: true
        closePolicy: backend.fontRemoveRunning ? Popup.NoAutoClose : Popup.CloseOnEscape
        title: qsTr("Remove the Extra fonts pack?")
        background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
        Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, .65) }
        Connections {
            target: backend
            enabled: removeSheet.visible
            function onFontRemoveFinished() { removeSheet.close() }
        }
        ColumnLayout {
            width: parent.width; spacing: Theme.s3
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap
                text: backend.fontRemoveRunning
                      ? qsTr("Removing fonts… %1 of %2").arg(backend.fontRemoveDone).arg(backend.fontRemoveTotal)
                      : (root.rows.length === 1 ? qsTr("This deletes the 1 free font OmaShow has downloaded") : qsTr("This deletes the %1 free fonts OmaShow has downloaded").arg(root.rows.length)) + qsTr(" (%1 MB). Presentations will use fallback fonts, or ask to download again.").arg(root.megabytes.toFixed(0))
            }
            ProgressBar {
                Layout.fillWidth: true
                // Its room is always kept, so the buttons do not move.
                opacity: backend.fontRemoveRunning ? 1 : 0
                from: 0; to: Math.max(1, backend.fontRemoveTotal); value: backend.fontRemoveDone
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { objectName: "fontRemoveKeep"; text: qsTr("Keep them"); enabled: !backend.fontRemoveRunning; onClicked: removeSheet.close() }
                Button { objectName: "fontRemoveYes"; text: qsTr("Remove"); highlighted: true; enabled: !backend.fontRemoveRunning; onClicked: backend.removeDownloadedFonts() }
            }
        }
    }
}

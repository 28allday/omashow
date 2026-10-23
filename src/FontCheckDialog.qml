import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Free fonts. Two modes, one dialog:
// deck: a deck names Office fonts whose free look-alikes are not here. One
//       click fetches them into OmaShow's own font folder (never the
//       system's) and the open deck is redrawn with them in place; or carry
//       on with sized fallbacks.
// pack: the Extra fonts pack: free matches for every
//       Office font. Runs in the background; this dialog can be closed and
//       reopened from the status line to watch or cancel.
Sheet {
    id: root
    objectName: "fontCheckDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width - Theme.s5 * 2, 600)
    modal: true
    closePolicy: mode === "deck" && phase === "progress" ? Popup.NoAutoClose : Popup.CloseOnEscape
    title: texts.title
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, .65) }

    property string mode: "deck"           // deck | pack
    property string phase: "offer"         // offer | progress | result
    property var rows: []
    property string source: ""
    property var failed: ({})
    property bool offline: false
    property bool details: false
    property var heldDeck: null            // a deck opened while the pack view was up
    readonly property var todo: rows.filter(r => r.downloadable && !r.installed)
    readonly property real megabytes: todo.reduce((n, r) => n + (r.bytes || 0), 0) / 1048576
    readonly property bool running: mode === "pack" ? backend.packRunning : backend.fontDownloadRunning
    readonly property real doneBytes: mode === "pack" ? backend.packDone : backend.fontDownloadDone
    readonly property real totalBytes: mode === "pack" ? backend.packBytes : backend.fontDownloadTotal
    function mb(b) { return (b / 1048576).toFixed(1) }

    // Every text per mode in one place.
    readonly property var texts: mode === "pack" ? ({
        title: qsTr("Extra fonts pack"),
        offer: qsTr("Free, openly licensed look-alikes for all %1 Microsoft Office fonts OmaShow knows (they are not Microsoft's fonts). %2 are here already. They go into OmaShow's own font folder; your system fonts are not changed.").arg(rows.length).arg(rows.length - todo.length),
        done: qsTr("All %1 fonts of the pack are ready.").arg(rows.length),
        progress: qsTr("%1 of %2 fonts ready · %3 of %4 MB. You can close this; it carries on.").arg(backend.packReady).arg(backend.packTotal).arg(mb(doneBytes)).arg(mb(totalBytes)),
        download: qsTr("Download Extra fonts pack (%1 MB)").arg(megabytes.toFixed(0)),
        secondary: qsTr("Close")
    }) : ({
        title: qsTr("Free fonts for this presentation"),
        offer: qsTr("This presentation uses Microsoft Office fonts that aren't on this computer. OmaShow can download free, openly licensed look-alikes (not Microsoft's fonts) so it looks and fits as intended. Only OmaShow uses them; your system fonts are not changed."),
        done: "",
        progress: qsTr("Downloading free fonts… %1 of %2 MB").arg(mb(doneBytes)).arg(mb(totalBytes)),
        download: qsTr("Download and install needed fonts (%1 MB)").arg(megabytes.toFixed(1)),
        secondary: qsTr("Use fallback fonts")
    })

    // From backend.fontCheckRequested (a deck was opened). Never during a show:
    // the window holds it until the show ends.
    function check(src, needed) {
        // The deck waits (backend.fontGate) until this resolves: every way out
        // ends in backend.finishFontGate(), which displays it already correct.
        // backend.gateOpen has already taken every exit without a popup
        // (pack on, always-download, hidden for this deck, nothing to fetch).
        const fetchable = needed.filter(r => r.downloadable)
        if (!src || fetchable.length === 0) { backend.finishFontGate(); return }
        if (visible && mode === "pack") { heldDeck = { source: src, rows: needed }; return }   // shown when the pack view closes
        mode = "deck"; source = src; rows = needed
        // A short list is shown at once (each free font and the Office font it replaces).
        phase = "offer"; failed = {}; details = needed.length <= 4; hideAgain.checked = false
        open()
    }
    function showPack() {
        mode = "pack"; source = ""; rows = backend.fontPack(); failed = {}; details = false
        phase = backend.packRunning ? "progress" : todo.length === 0 ? "result" : "offer"
        open()
    }
    function download() {
        phase = "progress"
        // Saying yes to the pack is what turns it on.
        if (mode === "pack") backend.extraFontsPack = true
        const ok = mode === "pack" ? backend.downloadPack() : backend.downloadFonts(todo.map(r => r.family))
        if (!ok) { phase = "result"; failed = ({}); offline = false }
    }

    onClosed: {
        if (hideAgain.checked && source) backend.setFontCheckHidden(source, true)
        if (mode === "deck" && backend.fontGate) backend.finishFontGate()   // fonts in, or fallbacks chosen
        if (heldDeck) { const held = heldDeck; heldDeck = null; Qt.callLater(() => check(held.source, held.rows)) }
    }

    Connections {
        target: backend
        function onFontDownloadFinished(installed, failedMap, wasOffline, cancelled) {
            // Only its own download: an older deck's finish never closes a fresh offer.
            if (!root.visible || root.mode !== "deck" || root.phase !== "progress") return
            if (cancelled) { root.phase = "offer"; return }
            root.failed = failedMap; root.offline = wasOffline
            root.rows = backend.fontsToInstall()
            if (Object.keys(failedMap).length === 0) root.close()
            else root.phase = "result"
        }
        function onPackFinished(ready, total, failedMap, wasOffline) {
            if (root.mode !== "pack") return
            root.failed = failedMap; root.offline = wasOffline
            root.rows = backend.fontPack()
            // Cancelled with nothing failed: back to the offer (the rest can come any time).
            root.phase = Object.keys(failedMap).length === 0 && root.todo.length > 0 ? "offer" : "result"
            if (Object.keys(failedMap).length > 0) root.details = true
        }
    }

    function rowState(row) {
        if (root.failed[row.family]) return qsTr("not installed: %1").arg(root.failed[row.family])
        if (row.installed) return qsTr("ready")
        if (!row.downloadable) return qsTr("no free download; a similar font is used")
        return row.metric ? qsTr("same letter widths") : qsTr("similar look, sized to fit")
    }

    ColumnLayout {
        width: parent.width; spacing: Theme.s3

        Label {
            objectName: "fontCheckIntro"
            Layout.fillWidth: true; wrapMode: Text.Wrap
            text: root.phase === "progress" ? root.texts.progress
                  : root.phase === "result"
                  ? (root.offline ? qsTr("Couldn't reach the font server. Check the internet connection and try again, or carry on with similar fonts.")
                     : Object.keys(root.failed).length === 0 ? (root.mode === "pack" ? root.texts.done : qsTr("A download is already running; try again in a moment."))
                     : qsTr("Some fonts could not be installed (%1). Similar fonts are used for those.").arg(Object.keys(root.failed).length))
                  : root.texts.offer
        }
        ProgressBar {
            objectName: "fontCheckProgress"
            visible: root.phase === "progress"
            Layout.fillWidth: true
            from: 0; to: Math.max(1, root.totalBytes); value: root.doneBytes
        }

        Label {
            objectName: "fontCheckDetailsToggle"
            text: (root.details ? "▾ " : "▸ ") + (root.rows.length === 1 ? qsTr("1 font") : qsTr("%1 fonts").arg(root.rows.length))
            color: Theme.accent
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.details = !root.details }
        }
        ListView {
            objectName: "fontCheckList"
            visible: root.details
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 240)
            clip: true
            model: root.rows
            spacing: Theme.s1
            ScrollBar.vertical: ScrollBar {}
            delegate: Label {
                required property var modelData
                required property int index
                objectName: "fontCheckRow" + index
                width: ListView.view.width - Theme.s2
                wrapMode: Text.Wrap
                color: root.failed[modelData.family] ? Theme.danger : modelData.installed ? Theme.textSecondary : Theme.textPrimary
                text: qsTr("<b>%1</b> (free) for %2 · %3").arg(modelData.family)
                      .arg(modelData.offices.slice(0, 3).join(", ") + (modelData.offices.length > 3 ? "…" : ""))
                      .arg(root.rowState(modelData))
            }
        }

        ColumnLayout {
            visible: root.phase === "offer" && root.mode === "deck"
            spacing: 0
            CheckBox { id: hideAgain; objectName: "fontCheckHide"; text: qsTr("Don't show again for this presentation") }
            CheckBox {
                objectName: "fontCheckAuto"
                text: qsTr("Always download missing free fonts")
                checked: backend.fontAutoDownload
                onToggled: backend.fontAutoDownload = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            Label {
                objectName: "fontCheckOpenFolder"
                visible: root.phase !== "progress"
                text: qsTr("Fonts folder")
                font.pixelSize: Theme.fsBase - 1; color: Theme.textSecondary; font.underline: true
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: backend.openFontSandbox() }
            }
            Item { Layout.fillWidth: true }
            Button {
                objectName: "fontCheckFallback"
                visible: root.phase !== "progress" || root.mode === "pack"
                text: root.phase === "progress" ? qsTr("Hide") : root.texts.secondary
                onClicked: root.close()
            }
            Button {
                objectName: "fontCheckCancel"
                visible: root.phase === "progress"
                text: qsTr("Cancel")
                onClicked: root.mode === "pack" ? backend.cancelPack() : backend.cancelFontDownload()
            }
            Button {
                objectName: "fontCheckDownload"
                visible: root.phase !== "progress" && root.todo.length > 0
                highlighted: true
                text: root.phase === "result" ? qsTr("Try again") : root.texts.download
                onClicked: root.download()
            }
        }
    }
}

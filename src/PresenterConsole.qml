import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

ApplicationWindow {
    id: presenterWindow
    objectName: "presenterConsole"
    title: qsTr("OmaShow · Presenter")
    transientParent: null
    visible: false
    width: Theme.wPresenterWindow; height: Theme.hPresenterWindow
    minimumWidth: Theme.wPresenterMinimum; minimumHeight: Theme.hPresenterMinimum
    color: Theme.windowBg
    font.family: Theme.fontFamily; font.pixelSize: Theme.fsControl
    PresenterPanel { anchors.fill: parent }
    Connections {
        target: presenter
        function onExternalLinkRequested(url) { externalLink.open(); presenterWindow.requestActivate() }
        function onStateChanged() { if(!presenter.running) externalLink.close() }
    }
    Dialog {
        id: externalLink; objectName: "externalLinkDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(parent.width-Theme.s5*2,600)
        modal: true; title: qsTr("Open external link?")
        standardButtons: Dialog.Open | Dialog.Cancel
        background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
        Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
        onOpened: standardButton(Dialog.Open).objectName="openExternalLink"
        onAccepted: presenter.openPendingLink()
        onRejected: presenter.cancelPendingLink()
        ColumnLayout {
            width: parent.width; spacing: Theme.s3
            Label { Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; textFormat: Text.PlainText; text: presenter.pendingExternalLink.toString(); color: Theme.textPrimary }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("This opens the destination in its desktop application. The show is paused; resume when ready."); color: Theme.textSecondary }
        }
    }
    Shortcut { sequence: "Escape"; context: Qt.WindowShortcut; enabled: !externalLink.visible; onActivated: presenter.stop() }
    Shortcut { sequences: ["Space", "Right", "PgDown"]; context: Qt.WindowShortcut; enabled: !externalLink.visible; onActivated: presenter.next() }
    Shortcut { sequences: ["Left", "PgUp"]; context: Qt.WindowShortcut; enabled: !externalLink.visible; onActivated: presenter.previous() }
    Shortcut { sequence: "B"; context: Qt.WindowShortcut; enabled: !externalLink.visible; onActivated: presenter.blankMode = presenter.blankMode === 1 ? 0 : 1 }
    Shortcut { sequence: "F"; context: Qt.WindowShortcut; enabled: !externalLink.visible; onActivated: presenter.frozen = !presenter.frozen }
    onClosing: event => { if (presenter.running) { event.accepted = false; presenter.stop() } }
}

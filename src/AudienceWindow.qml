import QtQuick
import QtQuick.Window
import Omashow 1.0

Window {
    id: audience
    objectName: "audienceWindow"
    title: qsTr("OmaShow · Audience")
    transientParent: null
    visible: false
    color: Theme.showBg
    width: Theme.wAudienceWindow; height: Theme.hAudienceWindow
    flags: Qt.Window | Qt.FramelessWindowHint
    SlideView { id: audienceView; objectName: "audienceSlideView"; anchors.fill: parent; deck: backend; time: presenter.audienceTime; editSlide: -1 }
    Rectangle { anchors.fill: parent; visible: presenter.blankMode !== 0;
                color: presenter.blankMode === 2 ? Theme.whiteout : Theme.showBg }
    MouseArea { objectName: "audienceClickArea"; anchors.fill: parent; cursorShape: Qt.BlankCursor; onClicked: mouse=> { const p=audienceView.toDocument(mouse.x,mouse.y); if(!presenter.activateAt(p.x,p.y)) presenter.next() } }
    Shortcut { sequence: "Escape"; context: Qt.WindowShortcut; onActivated: presenter.stop() }
    Shortcut { sequences: ["Space", "Right", "PgDown"]; context: Qt.WindowShortcut; onActivated: presenter.next() }
    Shortcut { sequences: ["Left", "PgUp"]; context: Qt.WindowShortcut; onActivated: presenter.previous() }
    Shortcut { sequence: "B"; context: Qt.WindowShortcut; onActivated: presenter.blankMode = presenter.blankMode === 1 ? 0 : 1 }
    Shortcut { sequence: "W"; context: Qt.WindowShortcut; onActivated: presenter.blankMode = presenter.blankMode === 2 ? 0 : 2 }
    Shortcut { sequence: "F"; context: Qt.WindowShortcut; onActivated: presenter.frozen = !presenter.frozen }
    onClosing: event => { if (presenter.running) { event.accepted = false; presenter.stop() } }
}

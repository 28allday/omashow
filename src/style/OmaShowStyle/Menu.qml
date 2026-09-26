import QtQuick
import QtQuick.Templates as T
import QtQuick.Window
import Omashow 1.0

T.Menu {
    id: control

    // A menu in front of the deck owns the keyboard as a sheet does (see
    // Sheet.qml): counted open, the shell's own Escape, Delete and letter
    // shortcuts stand down, so Escape closes the menu and "S" in a menu does
    // not add a rectangle to the slide behind it.
    property bool counted: false
    onVisibleChanged: {
        if (visible && !counted) { counted = true; Sheets.opened() }
        else if (!visible && counted) { counted = false; Sheets.closed() }
    }
    Component.onDestruction: if (counted) Sheets.closed()

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 0
    padding: Theme.s1
    overlap: 1
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl

    delegate: MenuItem { }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex
        T.ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: Rectangle {
        implicitWidth: Theme.wInspector - Theme.s5 * 2
        implicitHeight: Theme.hRow
        color: Theme.panelRaised
        radius: Theme.rMenu
        border.width: Theme.hairline
        border.color: Theme.borderStrong
    }

    T.Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.5) }
    T.Overlay.modeless: Rectangle { color: "transparent" }
}

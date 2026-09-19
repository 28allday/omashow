import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.MenuBar {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    spacing: 0
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    delegate: MenuBarItem { }
    contentItem: Row {
        spacing: control.spacing
        Repeater { model: control.contentModel }
    }
    background: Item { implicitHeight: Theme.hTitleBar }
}

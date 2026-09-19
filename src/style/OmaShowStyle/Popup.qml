import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.Popup {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.s3
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    background: Rectangle {
        color: Theme.panelRaised
        radius: Theme.rMenu
        border.width: Theme.hairline
        border.color: Theme.borderStrong
    }
    T.Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.6) }
    T.Overlay.modeless: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.12) }
}

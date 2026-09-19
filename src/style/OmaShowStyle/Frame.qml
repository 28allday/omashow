import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.Frame {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    padding: Theme.s3
    background: Rectangle {
        color: Theme.panelBg
        radius: Theme.rCard
        border.width: Theme.hairline
        border.color: Theme.border
    }
}

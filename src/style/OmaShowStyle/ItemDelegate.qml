import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.ItemDelegate {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.s2
    leftPadding: Theme.s3
    rightPadding: Theme.s3
    spacing: Theme.s2
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: control.highlighted ? Theme.textPrimary : Theme.textSecondary
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        implicitHeight: Theme.hRow
        radius: Theme.rControl
        color: control.down ? Theme.pressedOn(Theme.controlHover)
             : control.highlighted || control.hovered ? Theme.controlHover : "transparent"
    }
}

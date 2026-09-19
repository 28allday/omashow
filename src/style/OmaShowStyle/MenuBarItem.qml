import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.MenuBarItem {
    id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.s1
    leftPadding: Theme.s2 + 2
    rightPadding: Theme.s2 + 2
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: control.highlighted || control.down ? Theme.textPrimary : Theme.textSecondary
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        implicitHeight: Theme.hTitleBar - Theme.s2
        radius: Theme.rControl
        color: control.down || control.highlighted ? Theme.controlHover : "transparent"
    }
}

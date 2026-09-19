import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.ToolTip {
    id: control
    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: -implicitHeight - 4
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    margins: Theme.s2
    padding: Theme.s2
    leftPadding: Theme.s2 + 2
    rightPadding: Theme.s2 + 2
    delay: Theme.tooltipDelay
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsLabel
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent
    contentItem: Text {
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        wrapMode: Text.Wrap
        color: Theme.textPrimary
    }
    background: Rectangle {
        color: Theme.panelRaised
        radius: Theme.rControl
        border.width: Theme.hairline
        border.color: Theme.borderStrong
    }
}

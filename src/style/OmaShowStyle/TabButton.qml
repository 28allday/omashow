import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.TabButton {
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
    icon.width: Theme.szIcon
    icon.height: Theme.szIcon
    opacity: enabled ? 1 : Theme.disabledOpacity

    readonly property color ink: checked ? Theme.accent : hovered ? Theme.textPrimary : Theme.textSecondary

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: control.spacing
            Icon {
                visible: control.icon.name !== ""
                name: control.icon.name
                size: control.icon.width
                color: control.ink
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: control.text
                textFormat: Text.PlainText
                font.family: control.font.family
                font.pixelSize: control.font.pixelSize
                font.letterSpacing: control.font.letterSpacing
                font.capitalization: control.font.capitalization
                font.weight: control.checked ? Theme.wHeading : Theme.wNormal
                color: control.ink
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    background: Rectangle {
        implicitHeight: Theme.hTab
        color: control.hovered && !control.checked ? Theme.withAlpha(Theme.controlHover, 0.5) : "transparent"
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: Theme.activeUnderline
            color: Theme.accent
            visible: control.checked
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: control.visualFocus ? Theme.focusRing : 0
            border.color: Theme.accent
        }
    }
}

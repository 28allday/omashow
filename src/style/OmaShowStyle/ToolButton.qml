import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

// Chrome-less small button for panel headers and rows (section chevrons, the
// navigator's actions). Same content rules as Button.
T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    readonly property bool iconOnly: display === T.AbstractButton.IconOnly || (text === "" && icon.name !== "")
    padding: Theme.s1
    leftPadding: iconOnly ? Theme.s1 : Theme.s2
    rightPadding: leftPadding
    spacing: Theme.s1
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    icon.width: Theme.szIcon
    icon.height: Theme.szIcon
    opacity: enabled ? 1 : Theme.disabledOpacity

    readonly property color ink: checked || highlighted ? Theme.accent
                                : hovered ? Theme.textPrimary : Theme.textSecondary

    contentItem: Row {
        spacing: control.spacing
        Icon {
            visible: control.icon.name !== ""
            name: control.icon.name
            size: control.icon.width
            color: control.ink
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: control.text !== "" && !control.iconOnly
            text: control.text
            textFormat: Text.PlainText
            font: control.font
            color: control.ink
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    background: Rectangle {
        implicitWidth: Theme.szIconHit
        implicitHeight: Theme.szIconHit
        radius: Theme.rControl
        color: control.down ? Theme.pressedOn(Theme.controlBg)
             : control.checked ? Theme.withAlpha(Theme.accent, 0.12)
             : control.hovered ? Theme.controlHover : "transparent"
        border.width: control.visualFocus ? Theme.focusRing : 0
        border.color: Theme.accent
    }
}

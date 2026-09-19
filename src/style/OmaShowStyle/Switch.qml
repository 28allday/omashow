import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.s1
    spacing: Theme.s2
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    opacity: enabled ? 1 : Theme.disabledOpacity

    indicator: Rectangle {
        implicitWidth: Theme.szIcon * 2
        implicitHeight: Theme.szIcon + 2
        x: control.text ? (control.mirrored ? control.leftPadding : control.width - width - control.rightPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: control.checked ? Theme.withAlpha(Theme.accent, 0.35) : Theme.controlBg
        border.width: Theme.hairline
        border.color: control.checked ? Theme.accent : Theme.borderStrong
        Rectangle {
            x: Math.max(2, Math.min(parent.width - width - 2, control.visualPosition * parent.width - width / 2))
            y: (parent.height - height) / 2
            width: parent.height - 4; height: width; radius: width / 2
            color: control.checked ? Theme.accent : Theme.textSecondary
            Behavior on x { enabled: !control.down; NumberAnimation { duration: Theme.dFast; easing.type: Theme.easing } }
        }
    }

    contentItem: Text {
        rightPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        text: control.text
        textFormat: Text.PlainText
        font: control.font
        color: Theme.textSecondary
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}

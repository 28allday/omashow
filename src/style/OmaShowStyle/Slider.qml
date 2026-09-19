import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

// The concept's opacity slider: a thin rail, an accent fill, a round thumb.
T.Slider {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitHandleWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitHandleHeight + topPadding + bottomPadding)

    padding: Theme.s1
    hoverEnabled: true
    opacity: enabled ? 1 : Theme.disabledOpacity

    handle: Rectangle {
        x: control.leftPadding + (control.horizontal ? control.visualPosition * (control.availableWidth - width) : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : control.visualPosition * (control.availableHeight - height))
        implicitWidth: Theme.szIcon - 2
        implicitHeight: Theme.szIcon - 2
        radius: width / 2
        color: control.pressed ? Theme.accentPressed : Theme.accent
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.textPrimary
    }

    background: Rectangle {
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
        implicitWidth: control.horizontal ? Theme.wInspector / 2 : 4
        implicitHeight: control.horizontal ? 4 : Theme.wInspector / 2
        width: control.horizontal ? control.availableWidth : implicitWidth
        height: control.horizontal ? implicitHeight : control.availableHeight
        radius: 2
        color: Theme.borderStrong
        scale: control.horizontal && control.mirrored ? -1 : 1

        Rectangle {
            y: control.horizontal ? 0 : control.visualPosition * parent.height
            width: control.horizontal ? control.position * parent.width : parent.width
            height: control.horizontal ? parent.height : control.position * parent.height
            radius: 2
            color: Theme.accent
        }
    }
}

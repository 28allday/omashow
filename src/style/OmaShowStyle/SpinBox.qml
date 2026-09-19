import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

// Number well with a stacked up/down chevron pair on the right, as in the
// concept's Delay / Duration fields.
T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    leftPadding: Theme.s2 + 2
    rightPadding: Theme.szIcon + Theme.s2
    font.family: Theme.monoFamily
    font.pixelSize: Theme.fsControl
    hoverEnabled: true
    opacity: enabled ? 1 : Theme.disabledOpacity

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: Theme.textPrimary
        selectionColor: Theme.withAlpha(Theme.accent, 0.35)
        selectedTextColor: Theme.textPrimary
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        clip: width < implicitWidth
    }

    up.indicator: Item {
        x: control.width - width - 2
        y: 2
        implicitWidth: Theme.szIcon
        height: (control.height - 4) / 2
        Icon { anchors.centerIn: parent; name: "chevron-up"; size: Theme.szIcon - 4
               color: control.up.pressed ? Theme.accent : control.up.hovered ? Theme.textPrimary : Theme.textMuted }
    }
    down.indicator: Item {
        x: control.width - width - 2
        y: control.height / 2
        implicitWidth: Theme.szIcon
        height: (control.height - 4) / 2
        Icon { anchors.centerIn: parent; name: "chevron-down"; size: Theme.szIcon - 4
               color: control.down.pressed ? Theme.accent : control.down.hovered ? Theme.textPrimary : Theme.textMuted }
    }

    background: Rectangle {
        implicitWidth: Theme.s5 * 4
        implicitHeight: Theme.hControl
        radius: Theme.rControl
        color: Theme.controlBg
        border.width: Theme.hairline
        border.color: control.activeFocus ? Theme.accent : control.hovered ? Theme.borderStrong : Theme.border
    }
}

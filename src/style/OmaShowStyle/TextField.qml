import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.TextField {
    id: control

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
                   || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    leftPadding: Theme.s2 + 2
    rightPadding: Theme.s2 + 2
    topPadding: Theme.s1
    bottomPadding: Theme.s1
    verticalAlignment: TextInput.AlignVCenter
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.withAlpha(Theme.accent, 0.35)
    selectedTextColor: Theme.textPrimary
    selectByMouse: true
    hoverEnabled: true
    opacity: enabled ? 1 : Theme.disabledOpacity

    Text {
        id: placeholder
        x: control.leftPadding
        y: control.topPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height - (control.topPadding + control.bottomPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        visible: !control.length && !control.preeditText && (!control.activeFocus || control.horizontalAlignment !== Qt.AlignHCenter)
        elide: Text.ElideRight
        renderType: control.renderType
    }

    background: Rectangle {
        implicitWidth: Theme.wInspector / 2
        implicitHeight: Theme.hControl
        radius: Theme.rControl
        color: control.readOnly ? "transparent" : Theme.controlBg
        border.width: Theme.hairline
        border.color: control.activeFocus ? Theme.accent : control.hovered ? Theme.borderStrong : Theme.border
    }
}

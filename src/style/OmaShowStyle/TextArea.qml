import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.TextArea {
    id: control

    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding,
                            implicitBackgroundWidth + leftInset + rightInset,
                            placeholder.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(contentHeight + topPadding + bottomPadding,
                             implicitBackgroundHeight + topInset + bottomInset,
                             placeholder.implicitHeight + topPadding + bottomPadding)

    padding: Theme.s2 + 2
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.withAlpha(Theme.accent, 0.35)
    selectedTextColor: Theme.textPrimary
    selectByMouse: true
    hoverEnabled: true

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
        wrapMode: Text.Wrap
    }

    // A TextArea inside a ScrollView takes the view's frame; a bare one draws
    // its own well.
    background: Rectangle {
        implicitWidth: Theme.wInspector / 2
        implicitHeight: Theme.hControl * 2
        radius: Theme.rControl
        color: control.readOnly ? "transparent" : Theme.controlBg
        border.width: Theme.hairline
        border.color: control.activeFocus && !control.readOnly ? Theme.accent : Theme.border
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

// Dropdown: a filled well with a hairline and a chevron — the Fill / Border /
// Shadow selects in the concept inspector.
T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    leftPadding: Theme.s2 + 2
    rightPadding: Theme.s2 + (indicator ? indicator.width + Theme.s2 : 0)
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    hoverEnabled: true
    opacity: enabled ? 1 : Theme.disabledOpacity

    delegate: T.ItemDelegate {
        id: option
        required property int index
        required property var model
        width: ListView.view ? ListView.view.width : control.width
        implicitHeight: Theme.hRow
        leftPadding: Theme.s2 + 2
        rightPadding: Theme.s2
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
        text: model[control.textRole]
        font: control.font
        contentItem: Row {
            spacing: Theme.s2
            Icon {
                name: "check"
                size: Theme.szIcon - 2
                color: Theme.accent
                opacity: control.currentIndex === option.index ? 1 : 0
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: option.text
                textFormat: Text.PlainText
                font: option.font
                color: option.highlighted ? Theme.textPrimary : Theme.textSecondary
                elide: Text.ElideRight
                width: option.availableWidth - Theme.szIcon - Theme.s2
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        background: Rectangle {
            radius: Theme.rControl
            color: option.highlighted ? Theme.controlHover : "transparent"
        }
    }

    indicator: Icon {
        name: "chevron-down"
        size: Theme.szIcon
        color: control.hovered ? Theme.textSecondary : Theme.textMuted
        x: control.width - width - Theme.s2
        y: control.topPadding + (control.availableHeight - height) / 2
    }

    contentItem: T.TextField {
        leftPadding: 0
        rightPadding: 0
        text: control.editable ? control.editText : control.displayText
        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse
        font: control.font
        color: control.flat ? Theme.textSecondary : Theme.textPrimary
        selectionColor: Theme.withAlpha(Theme.accent, 0.35)
        selectedTextColor: Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: Theme.wInspector / 2
        implicitHeight: Theme.hControl
        radius: Theme.rControl
        color: control.down ? Theme.pressedOn(Theme.controlBg)
             : control.hovered ? Theme.controlHover
             : control.flat ? "transparent" : Theme.controlBg
        border.width: control.flat && !control.visualFocus ? 0 : Theme.hairline
        border.color: control.visualFocus || control.activeFocus && control.editable ? Theme.accent : Theme.border
    }

    popup: T.Popup {
        y: control.height + 2
        width: Math.max(control.width, Theme.wInspector / 2)
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding,
                         control.Window.height - topMargin - bottomMargin)
        topMargin: Theme.s2
        bottomMargin: Theme.s2
        padding: Theme.s1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            color: Theme.panelRaised
            radius: Theme.rMenu
            border.width: Theme.hairline
            border.color: Theme.borderStrong
        }
    }
}

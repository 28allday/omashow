import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

// The one button. Four tones, all from the concept art:
//   default      outlined well — Previous, Pause, Restart, Apply to all slides
//   flat         no chrome until hovered — toolbar actions
//   highlighted  accent outline and label — the forward action (Next, Play)
//   checked      accent tint — the active mode (Pointer, Snap)
// `icon.name` takes a Lucide name; `display` picks beside/under/icon-only.
T.Button {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    readonly property bool iconOnly: display === T.AbstractButton.IconOnly || (text === "" && icon.name !== "")
    readonly property bool under: display === T.AbstractButton.TextUnderIcon
    readonly property bool accentTone: highlighted || checked
    // A trailing chevron for buttons that open a menu. Read loosely so any
    // Button can opt in with `property bool opensMenu: true`.
    readonly property bool menuChevron: control["opensMenu"] === true
    // Icon after the label, for forward actions ("Next →"). Opt in with
    // `property bool iconTrailing: true`.
    readonly property bool trailing: control["iconTrailing"] === true && !under
    // A square letter mark (B / I / U). Opt in with `property bool mark: true`.
    // Same hit size as an icon button; the letter stays visible.
    readonly property bool mark: control["mark"] === true

    padding: 0
    leftPadding: iconOnly || mark ? Theme.s1 : under ? Theme.s2 : Theme.s3
    rightPadding: leftPadding
    topPadding: under ? Theme.s2 : Theme.s1
    bottomPadding: topPadding
    spacing: under ? Theme.s1 + 2 : Theme.s2
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    icon.width: under ? Theme.szIconLarge : Theme.szIcon
    icon.height: under ? Theme.szIconLarge : Theme.szIcon
    opacity: enabled ? 1 : Theme.disabledOpacity

    readonly property color ink: accentTone ? Theme.accent
                                : control.flat && !control.hovered ? Theme.textSecondary
                                : Theme.textPrimary

    contentItem: Item {
        readonly property bool hasIcon: control.icon.name !== "" && control.display !== T.AbstractButton.TextOnly
        readonly property bool hasText: control.text !== "" && !control.iconOnly
        readonly property real chevronWidth: control.menuChevron ? Theme.szIcon - 2 + control.spacing : 0
        implicitWidth: control.under ? Math.max(glyph.width, label.implicitWidth)
                     : (hasIcon ? glyph.width : 0) + (hasIcon && hasText ? control.spacing : 0)
                       + (hasText ? label.implicitWidth : 0) + chevronWidth
        implicitHeight: control.under ? glyph.height + control.spacing + label.implicitHeight
                      : Math.max(hasIcon ? glyph.height : 0, hasText ? label.implicitHeight : 0)

        Icon {
            id: glyph
            visible: parent.hasIcon
            name: control.icon.name
            size: control.icon.width
            color: control.ink
            x: control.under ? (parent.width - width) / 2
             : !parent.hasText ? (parent.width - width) / 2
             : control.trailing ? label.x + label.width + control.spacing
             : (parent.width - parent.implicitWidth) / 2
            y: control.under ? (parent.height - parent.implicitHeight) / 2 : (parent.height - height) / 2
        }
        Text {
            id: label
            visible: parent.hasText
            text: control.text
            textFormat: Text.PlainText
            font: control.font
            color: control.ink
            elide: Text.ElideRight
            width: Math.min(implicitWidth, parent.width - (control.under || !parent.hasIcon ? 0 : glyph.width + control.spacing) - parent.chevronWidth)
            x: control.under ? (parent.width - width) / 2
             : control.trailing || !parent.hasIcon ? (parent.width - parent.implicitWidth) / 2
             : glyph.x + glyph.width + control.spacing
            y: control.under ? glyph.y + glyph.height + control.spacing : (parent.height - implicitHeight) / 2
        }
        Icon {
            visible: control.menuChevron
            name: "chevron-down"
            size: Theme.szIcon - 2
            color: Theme.textMuted
            x: (parent.hasText ? label.x + label.width : glyph.x + glyph.width) + control.spacing
            y: (parent.height - height) / 2
        }
    }

    background: Rectangle {
        implicitWidth: control.iconOnly || control.mark ? Theme.szIconHit : Theme.hControl * 2
        implicitHeight: control.under ? Theme.hToolTile : Theme.hControl
        radius: Theme.rControl
        color: {
            if (control.accentTone)
                return Theme.withAlpha(Theme.accent, control.down ? 0.22 : control.hovered ? 0.16 : 0.10)
            if (control.down)
                return Theme.pressedOn(Theme.controlBg)
            if (control.hovered)
                return Theme.controlHover
            return control.flat ? "transparent" : Theme.withAlpha(Theme.controlBg, 0.55)
        }
        border.width: control.flat && !control.accentTone && !control.visualFocus ? 0 : Theme.hairline
        border.color: control.visualFocus || control.accentTone ? Theme.accent : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Theme.dFast; easing.type: Theme.easing } }
    }
}

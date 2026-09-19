import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

// A raised card over a dimmed window: title row, body, button row on the right.
T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    padding: Theme.s4
    topPadding: Theme.s2
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl

    background: Rectangle {
        color: Theme.panelBg
        radius: Theme.rCard
        border.width: Theme.hairline
        border.color: Theme.borderStrong
    }

    header: Text {
        text: control.title
        visible: control.title !== ""
        textFormat: Text.PlainText
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fsSection
        font.weight: Theme.wHeading
        color: Theme.textPrimary
        leftPadding: Theme.s4
        rightPadding: Theme.s4
        topPadding: Theme.s4
        bottomPadding: Theme.s2
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    T.Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.6) }
    T.Overlay.modeless: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.12) }
}

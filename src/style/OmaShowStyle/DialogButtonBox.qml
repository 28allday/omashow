import QtQuick
import QtQuick.Templates as T
import Omashow 1.0

T.DialogButtonBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    contentWidth: (contentItem as ListView)?.contentWidth

    spacing: Theme.s2
    padding: Theme.s4
    topPadding: Theme.s2
    alignment: Qt.AlignRight

    // The affirmative button carries the accent, as Next does in the console.
    delegate: Button {
        highlighted: DialogButtonBox.buttonRole === DialogButtonBox.AcceptRole
                     || DialogButtonBox.buttonRole === DialogButtonBox.YesRole
        implicitWidth: Math.max(Theme.s5 * 3 + Theme.s2, implicitContentWidth + leftPadding + rightPadding)
    }

    contentItem: ListView {
        implicitWidth: contentWidth
        model: control.contentModel
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
    }

    background: Item { implicitHeight: Theme.hControl + Theme.s4 }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// A framed panel with a title strip — the presenter console's CURRENT, NEXT,
// SPEAKER NOTES and PRESENTATION CONTROLS blocks. Children fill the body;
// `actions` sit at the right of the title strip.
Rectangle {
    id: card
    property string title: ""
    property string detail: ""
    property real bodyPadding: Theme.s3
    default property alias content: body.data
    property alias actions: actionRow.data

    color: Theme.panelBg
    radius: Theme.rCard
    border.width: Theme.hairline
    border.color: Theme.border

    RowLayout {
        id: header
        visible: card.title !== ""
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s2
        height: visible ? Theme.hCardHeader + Theme.s1 : 0
        spacing: Theme.s2
        Label {
            text: card.title
            font.pixelSize: Theme.fsHeading
            font.weight: Theme.wHeading
            font.letterSpacing: Theme.capsTracking
            color: Theme.textPrimary
        }
        Label {
            visible: card.detail !== ""
            text: "—  " + card.detail
            color: Theme.textSecondary
            font.pixelSize: Theme.fsHeading
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Item { Layout.fillWidth: card.detail === "" }
        RowLayout { id: actionRow; spacing: 0 }
    }
    Item {
        id: body
        anchors.top: header.bottom; anchors.bottom: parent.bottom
        anchors.left: parent.left; anchors.right: parent.right
        anchors.margins: card.bodyPadding
        anchors.topMargin: card.title !== "" ? 0 : card.bodyPadding
    }
}

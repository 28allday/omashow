import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Dialog {
    id: root
    objectName: "shapeGallery"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,640)
    height: Math.min(parent.height-Theme.s5*2,620)
    modal: true; title: qsTr("Shapes")
    standardButtons: Dialog.Close
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
    ScrollView {
        id: galleryScroll
        anchors.fill: parent; clip: true; contentWidth: availableWidth
        GridLayout {
            width: galleryScroll.availableWidth; columns: 4; uniformCellWidths: true; columnSpacing: Theme.s2; rowSpacing: Theme.s2
            Repeater {
                model: backend.shapeNames
                Button {
                    required property int index
                    required property string modelData
                    objectName: "shapeChoice"+index
                    Layout.fillWidth: true; Layout.preferredHeight: 100
                    Accessible.name: modelData
                    contentItem: Column {
                        spacing: Theme.s1
                        Image { anchors.horizontalCenter: parent.horizontalCenter; width: 64; height: 60; source: "image://slides/shape/"+index; fillMode: Image.PreserveAspectFit }
                        Label { width: parent.width; text: modelData; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; color: Theme.textPrimary; font.pixelSize: Theme.fsCaption }
                    }
                    onClicked: { backend.addShape(index); root.close() }
                }
            }
        }
    }
}

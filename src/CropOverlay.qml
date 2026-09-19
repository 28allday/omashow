import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Item {
    id: root
    required property var canvas
    property bool active: false
    property bool dragging: false
    property string imageId: ""
    property string slideId: ""
    visible: active
    function start() {
        if(backend.selectionCount!==1 || backend.selection.type!=="image") return
        canvas.commitTextEdit(); canvas.cancelPathTool(); imageId=backend.selectedId; slideId=backend.navigator[backend.currentSlide]?.id??""; active=true; canvas.forceActiveFocus()
    }
    function cancel() { if(dragging) backend.cancelEdit(); dragging=false; active=false }
    function finish() { if(dragging) backend.endEdit(); dragging=false; active=false }
    onVisibleChanged: if(!visible && active) cancel()
    Connections {
        target: backend
        function onSelectionChanged() { if(root.active && (backend.selectedId!==root.imageId || backend.selectionCount!==1 || (backend.navigator[backend.currentSlide]?.id??"")!==root.slideId)) root.cancel() }
    }
    MouseArea { anchors.fill: parent; enabled: root.active; onClicked: canvas.forceActiveFocus(); onWheel: wheel=>wheel.accepted=false }
    Item {
        id: frame
        x: canvas.docX(backend.selection.cropFrameX??0); y: canvas.docY(backend.selection.cropFrameY??0)
        width: (backend.selection.cropFrameW??0)*canvas.s; height: (backend.selection.cropFrameH??0)*canvas.s
        rotation: backend.selection.rotation??0
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: Theme.accent; border.width: 1 }
        Repeater {
            model: [{x:0,y:0},{x:.5,y:0},{x:1,y:0},{x:1,y:.5},{x:1,y:1},{x:.5,y:1},{x:0,y:1},{x:0,y:.5}]
            Rectangle {
                required property int index
                required property var modelData
                x: modelData.x*frame.width-width/2; y: modelData.y*frame.height-height/2
                width: 12; height: 12; color: Theme.showBg; border.color: Theme.accent; border.width: 2
                MouseArea {
                    objectName: "cropHandle"+index
                    anchors.fill: parent; anchors.margins: -5; preventStealing: true
                    cursorShape: index===1 || index===5?Qt.SizeVerCursor:index===3 || index===7?Qt.SizeHorCursor:index===0 || index===4?Qt.SizeFDiagCursor:Qt.SizeBDiagCursor
                    onPressed: { root.dragging=true; backend.beginEdit(qsTr("Crop picture")); canvas.forceActiveFocus() }
                    onPositionChanged: mouse=> { if(!pressed || !root.dragging) return; const point=mapToItem(canvas,mouse.x,mouse.y); const doc=canvas.documentPoint(point.x,point.y); backend.resizeImageCrop(index,doc.x,doc.y,(mouse.modifiers&Qt.ShiftModifier)!==0) }
                    onReleased: { if(root.dragging) backend.endEdit(); root.dragging=false }
                    onCanceled: { if(root.dragging) backend.cancelEdit(); root.dragging=false }
                }
            }
        }
    }
    Rectangle {
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: Theme.s3
        color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl
        width: tools.implicitWidth+Theme.s3; height: tools.implicitHeight+Theme.s1
        RowLayout {
            id: tools; anchors.centerIn: parent
            Label { text: qsTr("Crop · Shift keeps proportions"); color: Theme.textSecondary; font.pixelSize: Theme.fsCaption }
            Button { objectName: "finishImageCrop"; text: qsTr("Done"); onClicked: root.finish() }
        }
    }
}

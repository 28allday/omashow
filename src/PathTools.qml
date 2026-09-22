import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Item {
    id: root
    required property var canvas
    property int mode: 0 // select, pen, freehand, nodes
    property var points: []
    property var nodes: []
    property bool draggingNode: false
    property int chosenCommand: -1
    property string selectedIdentity: ""
    function refreshNodes() {
        const sel=backend.selection
        const result=[]
        if(sel.shapeKind===99 && !sel.connector && backend.selectionCount===1) {
            const data=sel.pathData ?? []
            for(let i=0;i<data.length;++i) for(let j=1;j+1<data[i].length;j+=2)
                result.push({command:i,coordinate:j,control:data[i][0]==="C" && j<5})
        }
        nodes=result
    }
    function cancel() { if(draggingNode) backend.cancelEdit(); draggingNode=false; mode=0; points=[]; chosenCommand=-1; ghost.requestPaint() }
    function start(next) { canvas.commitTextEdit(); cancel(); mode=next; selectedIdentity=backend.selectedId; refreshNodes(); canvas.forceActiveFocus() }
    function finish(closed) { if(points.length>=2) backend.addPath(points,closed,mode===2); points=[]; mode=0; ghost.requestPaint(); canvas.forceActiveFocus() }
    function removeNode() { if(mode===3 && chosenCommand>=0) { backend.removePathNode(chosenCommand); chosenCommand=-1; refreshNodes() } }
    Connections {
        target: backend
        function onSelectionChanged() {
            if(root.selectedIdentity!==backend.selectedId) { if(root.mode===3) root.mode=0; root.selectedIdentity=backend.selectedId; root.chosenCommand=-1 }
            if(!root.draggingNode) root.refreshNodes()
        }
        function onCurrentSlideChanged() { root.cancel() }
    }
    onVisibleChanged: if(!visible) cancel()
    Canvas {
        id: ghost; anchors.fill: parent
        onPaint: {
            const ctx=getContext("2d"); ctx.clearRect(0,0,width,height)
            if(root.points.length<1) return
            ctx.strokeStyle=Theme.accent; ctx.lineWidth=2; ctx.beginPath()
            for(let i=0;i<root.points.length;++i) {
                const p=root.points[i]; const x=canvas.docX(p.x), y=canvas.docY(p.y)
                if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y)
            }
            ctx.stroke()
        }
    }
    MouseArea {
        objectName: "pathDrawingArea"
        anchors.fill: parent; enabled: root.mode===1 || root.mode===2; cursorShape: Qt.CrossCursor; preventStealing: true
        function add(mouse) { const p=canvas.documentPoint(mouse.x,mouse.y); const last=root.points.length?root.points[root.points.length-1]:null; if(!last || Math.hypot(p.x-last.x,p.y-last.y)*canvas.s>=3) { if(root.points.length<10000) root.points=root.points.concat([{x:p.x,y:p.y}]); ghost.requestPaint() } }
        onPressed: mouse=> { if(root.mode===2) root.points=[]; add(mouse) }
        onPositionChanged: mouse=> { if(pressed && root.mode===2) add(mouse) }
        onReleased: { if(root.mode===2) root.finish(false) }
        onDoubleClicked: { if(root.mode===1) root.finish(false) }
        onCanceled: root.cancel()
    }
    Item {
        visible: root.mode===3
        x: canvas.docX(backend.selection.x ?? 0); y: canvas.docY(backend.selection.y ?? 0)
        width: (backend.selection.w ?? 0)*canvas.s; height: (backend.selection.h ?? 0)*canvas.s
        rotation: backend.selection.rotation ?? 0
        Repeater {
            model: root.nodes
            Rectangle {
                id: node
                required property var modelData
                readonly property var row: (backend.selection.pathData ?? [])[modelData.command] ?? []
                x: (row[modelData.coordinate] ?? 0)*parent.width-width/2
                y: (row[modelData.coordinate+1] ?? 0)*parent.height-height/2
                width: modelData.control?8:12; height: width; radius: modelData.control?width/2:1
                color: root.chosenCommand===modelData.command?Theme.accent:Theme.handleBg; border.color: Theme.accent
                MouseArea {
                    objectName: "pathNode"+modelData.command+"_"+modelData.coordinate
                    anchors.fill: parent; anchors.margins: -4; preventStealing: true; cursorShape: Qt.CrossCursor
                    onPressed: { root.draggingNode=true; root.chosenCommand=modelData.command; backend.beginEdit(qsTr("Move path node")); canvas.forceActiveFocus() }
                    onPositionChanged: mouse=> { if(!pressed || !root.draggingNode) return; const v=mapToItem(root,mouse.x,mouse.y); const p=canvas.documentPoint(v.x,v.y); backend.movePathNode(modelData.command,modelData.coordinate,p.x,p.y) }
                    onReleased: { if(root.draggingNode) backend.endEdit(); root.draggingNode=false; root.refreshNodes() }
                    onCanceled: { if(root.draggingNode) backend.cancelEdit(); root.draggingNode=false; root.refreshNodes() }
                }
            }
        }
    }
    // Pen and freehand start from the toolbar's Shape menu. This strip only
    // appears when there is something for it to do: finishing a drawing,
    // editing a path's nodes, or cropping the selected picture.
    readonly property bool pathSelected: backend.selectionCount===1 && backend.selection.shapeKind===99 && !backend.selection.connector
    readonly property bool pictureSelected: backend.selectionCount===1 && backend.selection.type==="image"
    Rectangle {
        objectName: "drawingStrip"
        anchors.top: parent.top; anchors.left: parent.left; anchors.margins: Theme.s3
        color: Theme.panelBg; border.color: Theme.border; radius: Theme.rCard
        visible: !canvas.cropMode && (root.mode!==0 || root.pathSelected || root.pictureSelected)
        width: toolbar.implicitWidth + Theme.s1 * 2; height: toolbar.implicitHeight + Theme.s1 * 2
        RowLayout {
            id: toolbar; anchors.centerIn: parent; spacing: Theme.s1
            Label { visible: root.mode===1 || root.mode===2; leftPadding: Theme.s2; rightPadding: Theme.s2; color: Theme.textSecondary
                    text: root.mode===1 ? qsTr("Pen") : qsTr("Freehand") }
            Button { objectName: "nodeTool"; visible: (root.mode===0 && root.pathSelected) || root.mode===3; text: qsTr("Edit nodes"); icon.name: "spline"; flat: true; checkable: true; checked: root.mode===3; onClicked: root.start(checked?3:0) }
            Button { objectName: "cropPictureTool"; text: qsTr("Crop"); icon.name: "crop"; flat: true; visible: root.mode===0 && backend.selectionCount===1 && backend.selection.type==="image"; onClicked: backend.editSelectedImageCrop() }
            Button { objectName: "finishPath"; text: qsTr("Finish"); icon.name: "check"; highlighted: true; visible: root.mode===1; enabled: root.points.length>=2; onClicked: root.finish(false) }
            Button { objectName: "closeDrawnPath"; text: qsTr("Close"); visible: root.mode===1; enabled: root.points.length>=3; onClicked: root.finish(true) }
            Button { objectName: "removePathNode"; text: qsTr("Remove node"); icon.name: "trash-2"; visible: root.mode===3; enabled: root.chosenCommand>=0; onClicked: root.removeNode() }
            Button { text: qsTr("Done"); visible: root.mode!==0; onClicked: root.cancel() }
        }
    }
    Label {
        anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.topMargin: 60
        visible: root.mode===1 || root.mode===2; color: Theme.accent
        text: root.mode===1?qsTr("Click points · Finish or Close · Esc cancels"):qsTr("Drag to draw · release to finish · Esc cancels")
    }
}

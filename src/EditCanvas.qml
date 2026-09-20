import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// The Edit canvas: the slide, plus selection. The slide itself is painted by
// SlideView (the same renderer as the export); everything here is chrome drawn
// on top, in the document-to-item transform the view publishes.
Item {
    id: root
    objectName: "editCanvas"

    readonly property bool cropMode: cropper.active
    function startCrop() { cropper.start() }
    function cancelCrop() { cropper.cancel() }
    function finishCrop() { cropper.finish() }
    CropOverlay { id: cropper; canvas: root; anchors.fill: parent; z: 16 }
    readonly property int pathMode: pathTools.mode
    function cancelPathTool() { pathTools.cancel() }
    function deletePathNode() { pathTools.removeNode() }
    PathTools { id: pathTools; canvas: root; anchors.fill: parent; z: 15 }

    readonly property real s: view.contentScale
    readonly property real zoom: view.zoom
    clip: true
    readonly property point origin: Qt.point(view.x + view.contentOrigin.x, view.y + view.contentOrigin.y)
    function documentPoint(x,y) { const p = view.mapFromItem(root,x,y); return view.toDocument(p.x,p.y) }
    function zoomBy(factor) { root.commitTextEdit(); view.zoomAt(view.zoom*factor,view.width/2,view.height/2) }
    function fitSelection() { root.commitTextEdit(); view.fitRect(backend.selectionVisualBounds()) }
    function fit() { root.commitTextEdit(); view.fit() }
    function actualSize() { root.commitTextEdit(); view.zoomAt(1,view.width/2,view.height/2) }
    Connections {
        target: backend
        function onStartChanged() { if (!backend.startVisible) view.fit() }
        function onSelectionChanged() {
            if (root.editingText && (backend.selectedId !== root.textObjectId || backend.navigator[backend.currentSlide].id !== root.textSlideId)) root.commitTextEdit()
        }
    }

    function docX(x) { return origin.x + x * s }
    function docY(y) { return origin.y + y * s }
    // A constant number of screen pixels, converted through the current zoom,
    // so snapping feels identical however far in you are.
    readonly property real snapTolerance: root.s > 0 ? Theme.snapPixels / root.s : 0

    // Typing happens on the slide, not in a side panel. A text object is edited
    // where it lives, at the size it will be.
    property bool editingText: false
    property string textObjectId
    property string textSlideId

    function beginTextEdit() {
        if (!backend.hasSelection || backend.selection.type !== "text")
            return
        root.textObjectId = backend.selectedId
        root.textSlideId = backend.navigator[backend.currentSlide].id
        root.editingText = true
        textEditor.text = backend.selection.editHtml
        textEditor.forceActiveFocus()
        textEditor.selectAll()
    }

    function commitTextEdit() {
        if (!root.editingText)
            return
        root.editingText = false
        backend.setTextSelection(0, 0)
        backend.commitTextDocument(root.textSlideId,root.textObjectId,textEditor.textDocument)
        root.forceActiveFocus()
    }

    SlideView {
        id: view
        objectName: "editSlideView"
        anchors.fill: parent
        // Clear of the floating tool and snap pills at the top.
        anchors.margins: Theme.s5 + Theme.s2
        anchors.topMargin: Theme.hToolButton + Theme.s3 * 2 + Theme.s2
        deck: backend
        editSlide: backend.currentSlide
        cropObject: cropper.active?cropper.imageId:""
        hiddenObject: root.editingText ? root.textObjectId : ""
        // The revision is the one signal meaning "these pixels are stale".
        property int rev: backend.revision
        onRevChanged: view.update()
    }

    MouseArea {
        id: area
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        cursorShape: (dragging || panning) ? Qt.ClosedHandCursor : Qt.ArrowCursor

        objectName: "canvasMouseArea"
        property bool panning: false
        property point panGrab
        property bool dragging: false
        property bool boxSelecting: false
        property bool extendBox: false
        property point boxEnd
        preventStealing: true
        property point grabDoc
        property rect startRect

        onPressed: (mouse) => {
            // Clicking away from the text being edited commits it, the way
            // every other editor behaves.
            root.commitTextEdit()
            root.forceActiveFocus()
            if (mouse.button === Qt.MiddleButton) { panning = true; panGrab = Qt.point(mouse.x,mouse.y); return }
            const p = root.documentPoint(mouse.x, mouse.y)
            const extend = (mouse.modifiers & Qt.ShiftModifier) !== 0
            const hit = backend.selectAt(p.x, p.y, extend, (mouse.modifiers & Qt.AltModifier) !== 0)
            if (!hit) {
                boxSelecting = true; extendBox = extend; grabDoc = p; boxEnd = p
                return
            }
            if (extend || !backend.hasSelection) return
            dragging = true
            grabDoc = p
            startRect = Qt.rect(backend.selection.x, backend.selection.y,
                                backend.selection.w, (backend.selection.h ?? 0))
            // One gesture, one undo step: begin here, commit on release, and
            // the positions in between never reach the stack.
            backend.beginEdit(qsTr("Move object"))
        }
        onPositionChanged: (mouse) => {
            if (panning) { view.panBy(mouse.x-panGrab.x,mouse.y-panGrab.y); panGrab = Qt.point(mouse.x,mouse.y); return }
            const p = root.documentPoint(mouse.x, mouse.y)
            if (boxSelecting) { boxEnd = p; return }
            if (!dragging) return
            backend.setSelectedRect(startRect.x + (p.x - grabDoc.x),
                                    startRect.y + (p.y - grabDoc.y),
                                    startRect.width, startRect.height,
                                    root.snapTolerance, true)
        }
        onReleased: {
            if (panning) { panning = false; return }
            if (boxSelecting) {
                boxSelecting = false
                backend.selectRegion(grabDoc.x,grabDoc.y,boxEnd.x-grabDoc.x,boxEnd.y-grabDoc.y,extendBox)
                return
            }
            if (!dragging)
                return
            dragging = false
            backend.endEdit()
        }
        onCanceled: { if (dragging) backend.cancelEdit(); dragging = false; boxSelecting = false; panning = false }
        onWheel: wheel => {
            root.commitTextEdit()
            if (wheel.modifiers & Qt.ControlModifier) {
                const p = view.mapFromItem(root,wheel.x,wheel.y)
                view.zoomAt(view.zoom * Math.pow(1.2,wheel.angleDelta.y/120),p.x,p.y)
            } else {
                const d = wheel.pixelDelta.x || wheel.pixelDelta.y ? wheel.pixelDelta : Qt.point(wheel.angleDelta.x/2,wheel.angleDelta.y/2)
                view.panBy(d.x,d.y)
            }
            wheel.accepted = true
        }
        onDoubleClicked: {
            if (backend.selection.groupId) backend.enterGroup()
            else if ((backend.selection.type === "table" || backend.selection.type === "chart")) backend.editSelectedTable()
            else root.beginTextEdit()
        }
    }

    Rectangle {
        visible: area.boxSelecting
        x: root.docX(Math.min(area.grabDoc.x,area.boxEnd.x))
        y: root.docY(Math.min(area.grabDoc.y,area.boxEnd.y))
        width: Math.abs(area.boxEnd.x-area.grabDoc.x)*root.s
        height: Math.abs(area.boxEnd.y-area.grabDoc.y)*root.s
        color: Theme.withAlpha(Theme.accent,.12)
        border.color: Theme.accent; border.width: Theme.hairline
    }
    Label {
        anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter
        visible: backend.groupDepth > 0
        text: qsTr("Inside group · Esc to leave")
        color: Theme.accent
    }

    // ── Smart guides ────────────────────────────────────────────────────────
    Repeater {
        model: backend.guides
        Rectangle {
            required property var modelData
            color: Theme.guide
            opacity: 0.8
            x: modelData.vertical ? root.docX(modelData.position) - width / 2
                                  : root.docX(modelData.from)
            y: modelData.vertical ? root.docY(modelData.from)
                                  : root.docY(modelData.position) - height / 2
            width: modelData.vertical ? Theme.hairline
                                      : (modelData.to - modelData.from) * root.s
            height: modelData.vertical ? (modelData.to - modelData.from) * root.s
                                       : Theme.hairline
        }
    }

    // ── Selection ───────────────────────────────────────────────────────────
    Item {
        id: selection
        rotation: backend.selectionCount === 1 ? (backend.selection.rotation ?? 0) : 0
        visible: backend.hasSelection && !root.editingText && !cropper.active && pathTools.mode!==3 && !(backend.selectionCount===1 && backend.selection.connector)
        x: root.docX((backend.selection.x ?? 0))
        y: root.docY((backend.selection.y ?? 0))
        width: (backend.selection.w ?? 0) * root.s
        height: (backend.selection.h ?? 0) * root.s

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Theme.selectionOutline
            border.width: Theme.selectionRing
        }

        Rectangle { width: Theme.hairline; height: Theme.s5; anchors.horizontalCenter: parent.horizontalCenter; y: -height; color: Theme.accent }
        Rectangle {
            width: Theme.szHandle*1.5; height: width; radius: width/2; color: Theme.handleBg; border.color: Theme.accent
            anchors.horizontalCenter: parent.horizontalCenter; y: -Theme.s5-height
            MouseArea {
                objectName: "rotationHandle"
                anchors.fill: parent; anchors.margins: -Theme.szHandleHit; preventStealing: true
                cursorShape: Qt.CrossCursor
                property point pivot
                property real lastAngle
                property real totalAngle: 0
                function angle(mouse) { const g=mapToItem(root,mouse.x,mouse.y); const p=root.documentPoint(g.x,g.y); return Math.atan2(p.y-pivot.y,p.x-pivot.x)*180/Math.PI }
                onPressed: mouse => {
                    root.forceActiveFocus(); pivot=Qt.point(backend.selection.x+backend.selection.w/2,backend.selection.y+backend.selection.h/2)
                    lastAngle=angle(mouse); totalAngle=0; backend.beginEdit(qsTr("Rotate objects"))
                }
                onPositionChanged: mouse => {
                    if(!pressed) return
                    const next=angle(mouse); let delta=next-lastAngle
                    if(delta>180) delta-=360; if(delta< -180) delta+=360
                    totalAngle+=delta; lastAngle=next
                    backend.rotateSelection(totalAngle,(mouse.modifiers & Qt.ShiftModifier)!==0)
                }
                onReleased: backend.endEdit()
                onCanceled: backend.cancelEdit()
            }
        }
        // Eight handles, each resizing from its own corner or edge.
        Repeater {
            model: [
                { hx: 0,   hy: 0,   dx: 1, dy: 1, dw: -1, dh: -1, cur: Qt.SizeFDiagCursor },
                { hx: 0.5, hy: 0,   dx: 0, dy: 1, dw:  0, dh: -1, cur: Qt.SizeVerCursor },
                { hx: 1,   hy: 0,   dx: 0, dy: 1, dw:  1, dh: -1, cur: Qt.SizeBDiagCursor },
                { hx: 1,   hy: 0.5, dx: 0, dy: 0, dw:  1, dh:  0, cur: Qt.SizeHorCursor },
                { hx: 1,   hy: 1,   dx: 0, dy: 0, dw:  1, dh:  1, cur: Qt.SizeFDiagCursor },
                { hx: 0.5, hy: 1,   dx: 0, dy: 0, dw:  0, dh:  1, cur: Qt.SizeVerCursor },
                { hx: 0,   hy: 1,   dx: 1, dy: 0, dw: -1, dh:  1, cur: Qt.SizeBDiagCursor },
                { hx: 0,   hy: 0.5, dx: 1, dy: 0, dw: -1, dh:  0, cur: Qt.SizeHorCursor }
            ]

            Rectangle {
                required property var modelData
                width: Theme.szHandle
                height: Theme.szHandle
                radius: Theme.rHandle
                color: Theme.handleBg
                border.color: Theme.accent
                border.width: Theme.selectionRing
                x: parent.width * modelData.hx - width / 2
                y: parent.height * modelData.hy - height / 2

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -Theme.szHandleHit
                    cursorShape: modelData.cur
                    property point grabDoc
                    property rect startRect

                    onPressed: (mouse) => {
                        const g = mapToItem(root, mouse.x, mouse.y)
                        grabDoc = root.documentPoint(g.x, g.y)
                        startRect = Qt.rect(backend.selection.x, backend.selection.y,
                                            backend.selection.w, (backend.selection.h ?? 0))
                        backend.beginEdit(qsTr("Resize object"))
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return
                        const g = mapToItem(root, mouse.x, mouse.y)
                        const p = root.documentPoint(g.x, g.y)
                        const ddx = p.x - grabDoc.x
                        const ddy = p.y - grabDoc.y
                        if(backend.selectionCount === 1 && Math.abs((backend.selection.rotation ?? 0) % 360) > .00001) { backend.resizeSelectionHandle(modelData.hx,modelData.hy,ddx,ddy); return }
                        backend.setSelectedRect(startRect.x + ddx * modelData.dx,
                                                startRect.y + ddy * modelData.dy,
                                                startRect.width + ddx * modelData.dw,
                                                startRect.height + ddy * modelData.dh,
                                                root.snapTolerance, false)
                    }
                    onReleased: backend.endEdit()
                    onCanceled: backend.cancelEdit()
                }
            }
        }
    }

    Label {
        visible: backend.hasSelection && (backend.selection.textOverflow ?? false) && !root.editingText
        x: root.docX((backend.selection.x ?? 0) + (backend.selection.w ?? 0)) - width
        y: root.docY((backend.selection.y ?? 0) + (backend.selection.h ?? 0)) + Theme.s1
        text: qsTr("Text overflows"); color: Theme.warning
    }

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            root.commitTextEdit()
            if (drop.hasUrls) for (const url of drop.urls) backend.insertImageAsync(url)
        }
    }

    // ── Inline text editing ─────────────────────────────────────────────────
    Item {
        visible: root.editingText
        rotation: backend.selection.rotation ?? 0
        x: root.docX(backend.selection.x ?? 0)
        y: root.docY(backend.selection.y ?? 0)
        width: (backend.selection.w ?? 0) * root.s
        height: (backend.selection.h ?? 0) * root.s
        z: 10

        Rectangle {
            anchors.fill: parent
            anchors.margins: -Theme.s1
            color: "transparent"
            border.color: Theme.accent
            border.width: Theme.selectionRing
        }

        TextEdit {
            id: textEditor
            objectName: "inlineTextEditor"
            width: backend.selection.w ?? 0
            height: backend.selection.h ?? 0
            scale: root.s
            transformOrigin: Item.TopLeft
            // Drawn at the size it will really be, so what you type is what the
            // slide gets — not an approximation in a panel.
            font.family: backend.selection.fontFamily ?? Theme.fontFamily
            font.pixelSize: Math.max(1, (backend.selection.effectiveFontSize ?? 48))
            font.weight: backend.selection.fontWeight ?? 400
            color: backend.selection.textColor ?? Theme.textPrimary
            selectionColor: Theme.accent
            selectedTextColor: Theme.accentText
            wrapMode: TextEdit.Wrap
            verticalAlignment: backend.selection.verticalAlign === 0 ? TextEdit.AlignTop : backend.selection.verticalAlign === 2 ? TextEdit.AlignBottom : TextEdit.AlignVCenter
            // Generated by the shared text layout; pasted input stays plain text.
            textFormat: TextEdit.RichText
            Keys.onPressed: event => {
                if (event.matches(StandardKey.Paste)) {
                    cursorPosition = backend.pasteEditorText(textDocument,selectionStart,selectionEnd)
                    event.accepted = true
                }
            }
            selectByMouse: true
            // The inspector formats whatever is selected here, so it needs to
            // know what that is — and to be rebuilt when the looks change.
            onSelectionStartChanged: backend.setTextSelection(selectionStart, selectionEnd)
            onSelectionEndChanged: backend.setTextSelection(selectionStart, selectionEnd)
            Connections {
                target: backend
                function onTextFormattingChanged() {
                    if (!textEditor.activeFocus) return
                    const from = textEditor.selectionStart, to = textEditor.selectionEnd
                    textEditor.text = backend.selection.editHtml
                    textEditor.select(from, to)
                }
            }

            Keys.onEscapePressed: root.commitTextEdit()
            onActiveFocusChanged: if (!activeFocus) root.commitTextEdit()
        }
    }
}

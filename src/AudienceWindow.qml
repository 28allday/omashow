import QtQuick
import QtQuick.Shapes
import QtQuick.Window
import Omashow 1.0

Window {
    id: audience
    objectName: "audienceWindow"
    title: qsTr("OmaShow · Audience")
    transientParent: null
    visible: false
    color: Theme.showBg
    width: Theme.wAudienceWindow; height: Theme.hAudienceWindow
    flags: Qt.Window | Qt.FramelessWindowHint
    SlideView { id: audienceView; objectName: "audienceSlideView"; anchors.fill: parent; deck: backend; time: presenter.audienceTime; editSlide: -1 }
    Rectangle { anchors.fill: parent; visible: presenter.blankMode !== 0;
                color: presenter.blankMode === 2 ? Theme.whiteout : Theme.showBg }
    // What the speaker draws over the show. It lives in the presenter, never in
    // the deck, unless they ask for it to be kept.
    Item {
        id: overlay
        objectName: "audienceOverlay"
        anchors.fill: parent
        visible: presenter.annotation !== 0

        // A spotlight is the slide dimmed everywhere but here.
        Shape {
            anchors.fill: parent
            visible: presenter.annotation === 2 && presenter.pointerVisible
            ShapePath {
                fillColor: Theme.withAlpha(Theme.showBg, 0.78)
                strokeWidth: 0
                fillRule: ShapePath.OddEvenFill
                PathRectangle { width: overlay.width; height: overlay.height }
                PathAngleArc {
                    centerX: audienceView.fromDocument(presenter.pointer.x, presenter.pointer.y).x
                    centerY: audienceView.fromDocument(presenter.pointer.x, presenter.pointer.y).y
                    radiusX: Math.max(60, overlay.height * 0.18); radiusY: Math.max(60, overlay.height * 0.18)
                    startAngle: 0; sweepAngle: 360
                }
            }
        }
        Canvas {
            id: inkCanvas
            objectName: "audienceInk"
            anchors.fill: parent
            renderStrategy: Canvas.Immediate
            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.lineWidth = Math.max(2, overlay.height * 0.006)
                context.strokeStyle = Theme.accent
                context.lineCap = "round"
                context.lineJoin = "round"
                for (const stroke of presenter.ink) {
                    context.beginPath()
                    for (let i = 0; i < stroke.points.length; ++i) {
                        const at = audienceView.fromDocument(stroke.points[i].x, stroke.points[i].y)
                        if (i === 0) context.moveTo(at.x, at.y)
                        else context.lineTo(at.x, at.y)
                    }
                    context.stroke()
                }
            }
            Connections {
                target: presenter
                function onAnnotationChanged() { inkCanvas.requestPaint() }
                function onTimeChanged() { inkCanvas.requestPaint() }
            }
        }
        Rectangle {
            objectName: "audiencePointer"
            visible: presenter.annotation === 1 && presenter.pointerVisible
            width: Math.max(14, overlay.height * 0.02); height: width; radius: width / 2
            color: Theme.withAlpha(Theme.accent, 0.85)
            border.color: Theme.accentText; border.width: Theme.hairline
            x: audienceView.fromDocument(presenter.pointer.x, presenter.pointer.y).x - width / 2
            y: audienceView.fromDocument(presenter.pointer.x, presenter.pointer.y).y - height / 2
        }
    }
    MouseArea {
        objectName: "audienceClickArea"
        anchors.fill: parent
        hoverEnabled: presenter.annotation !== 0
        cursorShape: presenter.annotation === 3 ? Qt.CrossCursor
                   : presenter.annotation === 0 ? Qt.BlankCursor : Qt.ArrowCursor
        onPositionChanged: mouse => { const p=audienceView.toDocument(mouse.x,mouse.y); presenter.movePointer(p.x,p.y) }
        onExited: presenter.hidePointer()
        onPressed: mouse => { if (presenter.annotation === 3) { const p=audienceView.toDocument(mouse.x,mouse.y); presenter.beginStroke(p.x,p.y) } }
        onReleased: presenter.endStroke()
        onClicked: mouse=> {
            if (presenter.annotation === 3) return
            const p=audienceView.toDocument(mouse.x,mouse.y)
            if(!presenter.activateAt(p.x,p.y)) presenter.next()
        }
    }
    Shortcut { sequence: "Escape"; context: Qt.WindowShortcut; onActivated: presenter.stop() }
    Shortcut { sequences: ["Space", "Right", "PgDown"]; context: Qt.WindowShortcut; onActivated: presenter.next() }
    Shortcut { sequences: ["Left", "PgUp"]; context: Qt.WindowShortcut; onActivated: presenter.previous() }
    Shortcut { sequence: "B"; context: Qt.WindowShortcut; onActivated: presenter.blankMode = presenter.blankMode === 1 ? 0 : 1 }
    Shortcut { sequence: "W"; context: Qt.WindowShortcut; onActivated: presenter.blankMode = presenter.blankMode === 2 ? 0 : 2 }
    Shortcut { sequence: "F"; context: Qt.WindowShortcut; onActivated: presenter.frozen = !presenter.frozen }
    Shortcut { sequence: "P"; context: Qt.WindowShortcut; onActivated: presenter.annotation = presenter.annotation === 1 ? 0 : 1 }
    Shortcut { sequence: "S"; context: Qt.WindowShortcut; onActivated: presenter.annotation = presenter.annotation === 2 ? 0 : 2 }
    Shortcut { sequence: "D"; context: Qt.WindowShortcut; onActivated: presenter.annotation = presenter.annotation === 3 ? 0 : 3 }
    Shortcut { sequence: "E"; context: Qt.WindowShortcut; onActivated: presenter.clearInk() }
    onClosing: event => { if (presenter.running) { event.accepted = false; presenter.stop() } }
}

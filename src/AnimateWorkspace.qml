import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Animate: the slide, its build timeline under it, and the selected build's
// settings on the right — the layout of the Animate concept. Everything here
// edits real build steps; the view is the same pure-function-of-t evaluator
// Present and export use.
RowLayout {
    id: root
    objectName: "animateWorkspace"
    readonly property var transition: backend.slideTransition
    spacing: 0
    property int selectedBuild: 0
    property real pixelsPerSecond: 120
    readonly property var build: backend.builds[selectedBuild] ?? ({})
    readonly property real tickStep: pixelsPerSecond >= 160 ? 0.25 : pixelsPerSecond >= 70 ? 0.5 : 1
    readonly property var effects: [qsTr("None"), qsTr("Fade"), qsTr("Rise"), qsTr("Media")]
    readonly property var triggers: [qsTr("At time"), qsTr("On click"), qsTr("With previous"), qsTr("After previous")]
    function update(key,value) { backend.setBuildProperty(selectedBuild,key,value) }
    function add(phase) {
        selectedBuild = selectionBuilds.visible && selectionBuilds.checked ? backend.addBuildForSelection(phase,1) : backend.addBuild(target.currentValue,phase,1)
    }
    function snapTime(value, ownIndex) {
        let best = Math.max(0,value), distance = Theme.snapPixels / root.pixelsPerSecond
        const candidates = [0,backend.localTime]
        backend.builds.forEach((build,index) => { if (index !== ownIndex) candidates.push(build.start,build.start+build.duration) })
        candidates.forEach(time => { const d = Math.abs(time-value); if (d < distance) { best = time; distance = d } })
        return best
    }
    // Build-in bars take the accent; build-out and media bars the motion colour,
    // so the two directions read apart at a glance.
    function barColour(step) { return step.effect === 3 ? Theme.success : step.phase === 1 ? Theme.aiAccent : Theme.accent }
    Connections { target: backend; function onCurrentSlideChanged() { root.selectedBuild = 0; backend.setLocalTime(0) } }

    component ToolAction: Button {
        property string tip: ""
        flat: true
        implicitHeight: Theme.hToolButton
        ToolTip.visible: hovered && tip !== ""
        ToolTip.delay: Theme.tooltipDelay
        ToolTip.text: tip
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0

        // ── Toolbar ─────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hToolbar
            color: Theme.panelBg
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s3
                anchors.rightMargin: Theme.s3
                spacing: Theme.s1
                Label { text: qsTr("Object"); color: Theme.textSecondary; Layout.rightMargin: Theme.s1 }
                ComboBox {
                    id: target
                    Layout.preferredWidth: Theme.wInspector * 0.7
                    model: backend.slideObjects; textRole: "name"; valueRole: "id"
                    currentIndex: Math.max(0,model.findIndex(o => o.id === backend.selectedId))
                    Accessible.name: qsTr("Object to animate")
                }
                CheckBox { id: selectionBuilds; visible: backend.selectionCount > 1; checked: true
                           text: qsTr("All %1 selected").arg(backend.selectionCount) }
                Rectangle { implicitWidth: Theme.hairline; Layout.fillHeight: true; Layout.margins: Theme.s3; color: Theme.border }
                ToolAction { objectName: "addBuildIn"; icon.name: "plus"; text: qsTr("Build In"); enabled: target.currentIndex >= 0; tip: qsTr("Add a build that brings the object in"); onClicked: root.add(0) }
                ToolAction { icon.name: "plus"; text: qsTr("Build Out"); enabled: target.currentIndex >= 0; tip: qsTr("Add a build that takes the object away"); onClicked: root.add(1) }
                Item { Layout.fillWidth: true }
                ToolAction { icon.name: "eye"; text: qsTr("Preview"); enabled: !!root.build.targetId; tip: qsTr("Play the selected build"); onClicked: backend.previewBuild(root.selectedBuild) }
                // "Play Slide", not "Play": the toolbar's Play (the show) sits right above this one (#16).
                Button { objectName: "playSlide"; icon.name: backend.playing ? "pause" : "play"; text: backend.playing ? qsTr("Pause") : qsTr("Play Slide"); highlighted: true
                         implicitHeight: Theme.hToolButton; onClicked: backend.previewSlide()
                         ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Play this slide's builds  Space") }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.hairline; color: Theme.border }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.pasteboard
            SlideView { anchors.fill: parent; anchors.margins: Theme.s5; deck: backend; time: backend.time; editSlide: -1 }
        }

        // ── Timeline ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.hTimeline
            color: Theme.panelBg
            Rectangle { anchors.top: parent.top; width: parent.width; height: Theme.hairline; color: Theme.border }

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: Theme.hairline
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.hCardHeader
                    Layout.leftMargin: Theme.s4
                    Layout.rightMargin: Theme.s3
                    spacing: Theme.s1
                    Label { text: qsTr("Animation Timeline"); font.weight: Theme.wHeading }
                    Item { Layout.fillWidth: true }
                    ToolButton { icon.name: "zoom-out"; onClicked: root.pixelsPerSecond = Math.max(40, root.pixelsPerSecond / 1.25)
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Zoom out") }
                    Slider { Layout.preferredWidth: Theme.s5 * 4; from: 40; to: 240; value: root.pixelsPerSecond; onMoved: root.pixelsPerSecond = value
                             Accessible.name: qsTr("Timeline zoom") }
                    ToolButton { icon.name: "zoom-in"; onClicked: root.pixelsPerSecond = Math.min(240, root.pixelsPerSecond * 1.25)
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Zoom in") }
                    Label { text: Math.round(root.pixelsPerSecond / 1.2) + "%"; color: Theme.textSecondary; font.family: Theme.monoFamily
                            Layout.preferredWidth: Theme.s5 * 2; horizontalAlignment: Text.AlignRight }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }

                ScrollView {
                    id: scroll
                    implicitWidth: 0; implicitHeight: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: Math.max(0, width-leftPadding-rightPadding, Theme.wTrackLabel + backend.slideDuration * root.pixelsPerSecond + Theme.s5 * 2)
                    contentHeight: tracks.height
                    Item {
                        id: tracks
                        width: scroll.contentWidth
                        height: Math.max(scroll.height, Theme.hRuler + Theme.hTrack * backend.builds.length)

                        // Label column and ruler backgrounds.
                        Rectangle { width: Theme.wTrackLabel; height: parent.height; color: Theme.windowBg
                                    Rectangle { anchors.right: parent.right; width: Theme.hairline; height: parent.height; color: Theme.border } }
                        Rectangle { x: Theme.wTrackLabel; width: parent.width - x; height: Theme.hRuler; color: Theme.windowBg
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.hairline; color: Theme.border } }

                        // Ruler: labelled ticks, minor ticks between them, and a
                        // faint grid line down through the tracks.
                        Repeater {
                            model: Math.floor(Math.max(backend.slideDuration, (tracks.width - Theme.wTrackLabel) / root.pixelsPerSecond) / root.tickStep) + 1
                            Item {
                                required property int index
                                x: Theme.wTrackLabel + Theme.s3 + index * root.tickStep * root.pixelsPerSecond
                                height: tracks.height
                                Label { y: Theme.s1; x: -width / 2; text: (index * root.tickStep).toFixed(root.tickStep < 1 ? 1 : 0)
                                        color: Theme.textMuted; font.pixelSize: Theme.fsCaption; font.family: Theme.monoFamily }
                                Rectangle { y: Theme.hRuler - Theme.s2; width: Theme.hairline; height: Theme.s2; color: Theme.borderStrong }
                                Rectangle { y: Theme.hRuler; width: Theme.hairline; height: tracks.height - Theme.hRuler; color: Theme.withAlpha(Theme.border, 0.6) }
                                Rectangle { x: root.tickStep * root.pixelsPerSecond / 2; y: Theme.hRuler - Theme.s1; width: Theme.hairline; height: Theme.s1; color: Theme.border }
                            }
                        }
                        Label { x: Theme.s4; height: Theme.hRuler; verticalAlignment: Text.AlignVCenter; text: qsTr("BUILDS")
                                color: Theme.textMuted; font.pixelSize: Theme.fsCaption; font.letterSpacing: Theme.capsTracking }

                        Repeater {
                            model: backend.builds
                            Item {
                                id: track
                                required property var modelData
                                required property int index
                                readonly property bool chosen: root.selectedBuild === index
                                y: Theme.hRuler + Theme.hTrack * index
                                width: tracks.width; height: Theme.hTrack
                                property bool moving: false
                                property bool trimming: false
                                property real draftStart: modelData.start
                                property real draftDuration: modelData.duration
                                Rectangle { anchors.fill: parent; color: track.chosen ? Theme.withAlpha(Theme.accent, 0.06) : "transparent" }
                                Rectangle { anchors.bottom: parent.bottom; height: Theme.hairline; width: parent.width; color: Theme.border }
                                MouseArea { width: Theme.wTrackLabel; height: parent.height; onClicked: root.selectedBuild = track.index }
                                Row {
                                    x: Theme.s3; height: parent.height; spacing: Theme.s2
                                    Icon { name: track.modelData.trigger === 1 ? "mouse-pointer-2" : track.modelData.effect === 3 ? "clapperboard" : "sparkles"
                                           color: track.chosen ? Theme.accent : Theme.textMuted; anchors.verticalCenter: parent.verticalCenter }
                                    Label { text: track.index + 1; color: Theme.textMuted; font.family: Theme.monoFamily; anchors.verticalCenter: parent.verticalCenter }
                                    Label { width: Theme.wTrackLabel - Theme.s5 * 3; elide: Text.ElideRight; maximumLineCount: 1; text: String(track.modelData.label).replace(/\s+/g, " ")
                                            color: track.chosen ? Theme.accent : Theme.textPrimary; anchors.verticalCenter: parent.verticalCenter }
                                }
                                Rectangle {
                                    id: clip
                                    objectName: "buildClip" + track.index
                                    readonly property color tone: root.barColour(track.modelData)
                                    x: Theme.wTrackLabel + Theme.s3 + (track.moving ? track.draftStart : track.modelData.start) * root.pixelsPerSecond
                                    y: Theme.s1 + 2
                                    width: Math.max(Theme.szHandle * 2, (track.trimming ? track.draftDuration : track.modelData.duration) * root.pixelsPerSecond)
                                    height: Theme.hTrack - Theme.s2 - 4
                                    radius: Theme.rControl
                                    color: Theme.withAlpha(tone, track.chosen ? 0.55 : 0.32)
                                    border.color: tone
                                    border.width: track.chosen ? Theme.selectionRing : Theme.hairline
                                    // Diamond keys at either end, as in the concept.
                                    Rectangle { width: Theme.s2 + 2; height: width; rotation: 45; color: Theme.textPrimary; border.color: clip.tone
                                                x: -width / 2; anchors.verticalCenter: parent.verticalCenter }
                                    Rectangle { width: Theme.s2 + 2; height: width; rotation: 45; color: Theme.textPrimary; border.color: clip.tone
                                                x: parent.width - width / 2; anchors.verticalCenter: parent.verticalCenter }
                                    Label { anchors.fill: parent; anchors.leftMargin: Theme.s3; anchors.rightMargin: Theme.s3; elide: Text.ElideRight
                                            verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fsLabel
                                            text: root.effects[track.modelData.effect] + (track.modelData.effect===3 ? "" : track.modelData.phase === 1 ? qsTr(" out") : qsTr(" in"))
                                            color: Theme.textPrimary }
                                    MouseArea {
                                        anchors.fill: parent
                                        anchors.rightMargin: Theme.szHandle
                                        enabled: track.modelData.effect!==3
                                        cursorShape: Qt.SizeHorCursor
                                        property real grab: 0
                                        onPressed: mouse => {
                                            root.selectedBuild = track.index
                                            backend.pause()
                                            grab = mapToItem(tracks,mouse.x,mouse.y).x
                                            track.draftStart = track.modelData.start
                                            track.moving = true
                                        }
                                        onPositionChanged: mouse => { if (pressed) track.draftStart = root.snapTime(track.modelData.start + (mapToItem(tracks,mouse.x,mouse.y).x - grab)/root.pixelsPerSecond,track.index) }
                                        onReleased: {
                                            const index = track.index, start = Math.round(track.draftStart * 100)/100
                                            track.moving = false
                                            if (start !== track.modelData.start) backend.setBuildProperty(index,"start",start)
                                        }
                                        onCanceled: track.moving = false
                                    }
                                    MouseArea {
                                        anchors.right: parent.right
                                        height: parent.height; width: Theme.szHandle
                                        enabled: track.modelData.effect!==3
                                        cursorShape: Qt.SizeHorCursor
                                        property real grab: 0
                                        onPressed: mouse => { root.selectedBuild = track.index; backend.pause(); grab = mapToItem(tracks,mouse.x,mouse.y).x; track.draftDuration = track.modelData.duration; track.trimming = true }
                                        onPositionChanged: mouse => { if (pressed) track.draftDuration = Math.max(.01,root.snapTime(track.modelData.start + track.modelData.duration + (mapToItem(tracks,mouse.x,mouse.y).x - grab)/root.pixelsPerSecond,track.index)-track.modelData.start) }
                                        onReleased: { const index = track.index, duration = Math.round(track.draftDuration*100)/100; track.trimming = false; backend.setBuildProperty(index,"duration",duration) }
                                        onCanceled: track.trimming = false
                                    }
                                }
                            }
                        }

                        // Scrub by clicking or dragging the ruler.
                        MouseArea {
                            x: Theme.wTrackLabel; width: parent.width - x; height: Theme.hRuler
                            function seek(mx) { backend.pause(); backend.setLocalTime(Math.max(0, Math.min(backend.slideDuration, (mx - Theme.s3) / root.pixelsPerSecond))) }
                            onPressed: mouse => seek(mouse.x)
                            onPositionChanged: mouse => { if (pressed) seek(mouse.x) }
                        }

                        // Playhead with its time flag.
                        Item {
                            x: Theme.wTrackLabel + Theme.s3 + backend.localTime * root.pixelsPerSecond
                            height: tracks.height
                            Rectangle { x: -width / 2; y: Theme.hRuler - Theme.s1; width: Theme.selectionRing; height: parent.height - y; color: Theme.accent }
                            Rectangle {
                                x: -width / 2; y: 1
                                width: flag.implicitWidth + Theme.s2; height: Theme.hRuler - Theme.s2
                                radius: Theme.rControl; color: Theme.accent
                                Label { id: flag; anchors.centerIn: parent; text: backend.localTime.toFixed(1) + "s"
                                        color: Theme.accentText; font.pixelSize: Theme.fsCaption; font.family: Theme.monoFamily }
                            }
                        }
                    }
                }
                Label { visible: backend.builds.length === 0; Layout.leftMargin: Theme.wTrackLabel + Theme.s4; Layout.bottomMargin: Theme.s2
                        text: qsTr("Choose an object and add its first build."); color: Theme.textMuted }

                // Transport.
                Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.hCardHeader + Theme.s1
                    Layout.leftMargin: Theme.s3
                    Layout.rightMargin: Theme.s3
                    spacing: Theme.s1
                    ToolButton { icon.name: "skip-back"; onClicked: backend.setLocalTime(0)
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Back to the start") }
                    ToolButton { icon.name: backend.playing ? "pause" : "play"; onClicked: backend.previewSlide()
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: backend.playing ? qsTr("Pause  Space") : qsTr("Play slide  Space") }
                    ToolButton { icon.name: "step-back"; onClicked: backend.step(-0.1)
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Back a tenth  ←") }
                    ToolButton { icon.name: "step-forward"; onClicked: backend.step(0.1)
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Forward a tenth  →") }
                    Label { Layout.leftMargin: Theme.s3; text: backend.localTime.toFixed(2) + " / " + backend.slideDuration.toFixed(2) + " s"
                            color: Theme.textSecondary; font.family: Theme.monoFamily }
                    Item { Layout.fillWidth: true }
                    Label { text: qsTr("Speed"); color: Theme.textMuted }
                    ComboBox { model: [0.5, 1, 2]; implicitWidth: Theme.s5 * 3
                               currentIndex: model.indexOf(backend.playbackRate); displayText: backend.playbackRate + "×"
                               onActivated: backend.playbackRate = model[currentIndex] }
                }
            }
        }
    }

    // ── Build panel ─────────────────────────────────────────────────────────
    Rectangle {
        Layout.preferredWidth: Theme.wInspector
        Layout.fillHeight: true
        color: Theme.panelBg
        Rectangle { anchors.left: parent.left; width: Theme.hairline; height: parent.height; color: Theme.border }
        ScrollView {
            anchors.fill: parent
            anchors.margins: Theme.s4
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: Theme.s3
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Build"); font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading }
                    Item { Layout.fillWidth: true }
                    Label { visible: !!root.build.targetId; text: qsTr("%1 of %2").arg(root.selectedBuild + 1).arg(backend.builds.length); color: Theme.textMuted }
                }
                Label { visible: !root.build.targetId; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                        text: qsTr("No build selected. Pick an object in the toolbar and add a build in or out.") }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s3
                    visible: !!root.build.targetId
                    FieldRow {
                        label: qsTr("Object")
                        Label { Layout.fillWidth: true; text: String(root.build.label ?? "").replace(/\s+/g, " "); elide: Text.ElideRight; maximumLineCount: 1 }
                    }
                    FieldRow {
                        label: qsTr("Direction")
                        visible: root.build.effect!==3
                        ComboBox { Layout.fillWidth: true; model: [qsTr("Build in"),qsTr("Build out")]; currentIndex: root.build.phase ?? 0; onActivated: root.update("phase",currentIndex) }
                    }
                    FieldRow {
                        label: qsTr("Effect")
                        visible: root.build.effect!==3
                        // Media keeps index 3 to itself, so the list skips it.
                        ComboBox {
                            objectName: "buildEffect"
                            Layout.fillWidth: true
                            property var effects: [1,2,4,5,6,7,8,9]
                            model: [qsTr("Fade"),qsTr("Rise"),qsTr("Move"),qsTr("Scale"),qsTr("Spin"),qsTr("Emphasis"),qsTr("Reveal text"),qsTr("Along a path")]
                            currentIndex: Math.max(0,effects.indexOf(root.build.effect ?? 1))
                            onActivated: root.update("effect",effects[currentIndex])
                        }
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                        visible: root.build.effect===8 && !root.build.text
                        text: qsTr("Reveal only has an effect on text. This object will simply appear.")
                    }
                    FieldRow {
                        label: qsTr("Travels")
                        visible: root.build.effect===4
                        NumField { objectName: "buildMoveX"; Layout.fillWidth: true; step: 10; suffix: " x"
                                   value: root.build.amountX ?? 0; onCommitted: v => root.update("amountX",v) }
                        NumField { objectName: "buildMoveY"; Layout.fillWidth: true; step: 10; suffix: " y"
                                   value: root.build.amountY ?? 0; onCommitted: v => root.update("amountY",v) }
                    }
                    FieldRow {
                        label: root.build.effect===5 ? qsTr("Starts at") : root.build.effect===6 ? qsTr("Turns") : qsTr("Swells by")
                        visible: root.build.effect===5 || root.build.effect===6 || root.build.effect===7
                        NumField {
                            objectName: "buildAmount"
                            Layout.fillWidth: true
                            step: root.build.effect===6 ? 15 : .05
                            suffix: root.build.effect===6 ? "°" : "×"
                            value: root.build.amount || (root.build.effect===5 ? .5 : root.build.effect===6 ? 180 : .15)
                            onCommitted: v => root.update("amount",v)
                        }
                    }
                    FieldRow {
                        label: qsTr("Follows")
                        visible: root.build.effect===9
                        ComboBox {
                            objectName: "buildPath"
                            Layout.fillWidth: true
                            textRole: "name"
                            valueRole: "id"
                            // Any other object on the slide can be the guide; a
                            // drawn path hidden in Edit is the usual one.
                            model: [{ id: "", name: qsTr("Choose a shape or path…") }]
                                   .concat(backend.slideObjects.filter(o => o.id !== root.build.targetId))
                            currentIndex: Math.max(0, model.findIndex(o => o.id === (root.build.pathId ?? "")))
                            onActivated: root.update("pathId", currentValue)
                        }
                    }
                    CheckBox {
                        objectName: "buildPathReverse"
                        visible: root.build.effect===9
                        text: qsTr("Travel the other way")
                        checked: root.build.pathReverse ?? false
                        onToggled: root.update("pathReverse", checked)
                    }
                    CheckBox {
                        objectName: "buildPathOrient"
                        visible: root.build.effect===9
                        text: qsTr("Turn with the path")
                        checked: root.build.orient ?? false
                        onToggled: root.update("orient", checked)
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                        visible: root.build.effect===9
                        text: qsTr("Draw the route with the Pen in Edit, then hide it there — the object still follows it, and editing its nodes changes the motion. The object lands where you placed it.")
                    }
                    FieldRow {
                        label: qsTr("One step is")
                        visible: root.build.effect===8
                        ComboBox { objectName: "buildUnit"; Layout.fillWidth: true
                                   model: [qsTr("A paragraph"),qsTr("A word"),qsTr("A character")]
                                   currentIndex: root.build.unit ?? 0; onActivated: root.update("unit",currentIndex) }
                    }
                    FieldRow {
                        label: qsTr("Order")
                        Button { icon.name: "arrow-up"; text: qsTr("Earlier"); enabled: root.selectedBuild > 0; onClicked: { backend.moveBuild(root.selectedBuild,root.selectedBuild-1); root.selectedBuild-- } }
                        Button { icon.name: "arrow-down"; text: qsTr("Later"); enabled: root.selectedBuild < backend.builds.length-1; onClicked: { backend.moveBuild(root.selectedBuild,root.selectedBuild+1); root.selectedBuild++ } }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        Label { text: qsTr("Start"); color: Theme.textSecondary; Layout.preferredWidth: Theme.wFieldLabel; Layout.alignment: Qt.AlignTop; Layout.topMargin: Theme.s1 }
                        ColumnLayout {
                            spacing: 0
                            Repeater {
                                model: root.triggers
                                RadioButton { required property string modelData; required property int index
                                              text: modelData; checked: (root.build.trigger ?? 0) === index
                                              onClicked: root.update("trigger", index) }
                            }
                        }
                    }
                    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            visible: root.build.trigger === 1; text: qsTr("On-click groups pause for the speaker in Present. Preview and export use the timing shown here.") }
                    FieldRow {
                        label: root.build.trigger === 0 ? qsTr("At") : qsTr("Delay")
                        NumField { Layout.fillWidth: true; step: .01; suffix: " s"; value: root.build.trigger === 0 ? root.build.start ?? 0 : root.build.delay ?? 0
                                   onCommitted: v => root.update(root.build.trigger === 0 ? "start" : "delay",v) }
                    }
                    FieldRow {
                        label: qsTr("Duration")
                        NumField { objectName: "buildDuration"; enabled: root.build.effect!==3; Layout.fillWidth: true; step: .01; suffix: " s"; value: root.build.duration ?? .6; onCommitted: v => root.update("duration",v) }
                    }
                    FieldRow {
                        label: qsTr("Easing")
                        visible: root.build.effect!==3
                        ComboBox { Layout.fillWidth: true; model: [qsTr("Linear"),qsTr("Ease out"),qsTr("Ease in and out")]
                                   property var curves: [Easing.Linear,Easing.OutCubic,Easing.InOutCubic]
                                   currentIndex: Math.max(0,curves.indexOf(root.build.easing)); onActivated: root.update("easing",curves[currentIndex]) }
                    }
                    Button { Layout.fillWidth: true; icon.name: "play"; text: qsTr("Preview"); onClicked: backend.previewBuild(root.selectedBuild) }
                    Button { Layout.fillWidth: true; icon.name: "trash-2"; text: qsTr("Remove build"); onClicked: { backend.removeBuild(root.selectedBuild); root.selectedBuild = Math.max(0,root.selectedBuild-1) } }
                }

                Rectangle { Layout.fillWidth: true; Layout.topMargin: Theme.s2; implicitHeight: Theme.hairline; color: Theme.border }
                Label { text: qsTr("Slide Transition"); font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading; Layout.topMargin: Theme.s1 }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    text: root.transition.first ? qsTr("The first slide has nothing to arrive from; this sets what the deck does elsewhere.")
                                                : qsTr("How the show arrives at this slide.")
                }
                // The gallery: what each transition does, in the words it does it.
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.s2
                    rowSpacing: Theme.s2
                    Repeater {
                        model: [{ kind: 0, icon: "scissors", detail: qsTr("No transition at all") },
                                { kind: 1, icon: "sun", detail: qsTr("One slide fades into the next") },
                                { kind: 2, icon: "arrow-left-right", detail: qsTr("The next slide pushes this one off") },
                                { kind: 3, icon: "sparkles", detail: qsTr("Matching objects move into place") },
                                { kind: 4, icon: "layers", detail: qsTr("The next slide slides in over this one") },
                                { kind: 5, icon: "eye", detail: qsTr("This slide slides away to show the next beneath") },
                                { kind: 6, icon: "moon", detail: qsTr("Fades out to black, then the next slide fades in") },
                                { kind: 7, icon: "zoom-in", detail: qsTr("The next slide grows out of the middle") },
                                { kind: 8, icon: "rotate-cw", detail: qsTr("The next slide spins in from nothing") }]
                        Button {
                            required property var modelData
                            objectName: "transitionKind" + modelData.kind
                            Layout.fillWidth: true
                            icon.name: modelData.icon
                            text: root.transition.names[modelData.kind]
                            checked: root.transition.effective === modelData.kind
                            ToolTip.visible: hovered
                            ToolTip.delay: Theme.tooltipDelay
                            ToolTip.text: modelData.detail
                            onClicked: backend.setSlideTransition("kind", modelData.kind)
                        }
                    }
                }
                FieldRow {
                    label: qsTr("Towards")
                    visible: root.transition.travels ?? false
                    ComboBox {
                        objectName: "transitionDirection"
                        Layout.fillWidth: true
                        model: root.transition.directions
                        currentIndex: root.transition.direction ?? 0
                        onActivated: backend.setSlideTransition("direction", currentIndex)
                    }
                }
                FieldRow {
                    label: qsTr("Takes")
                    visible: root.transition.effective !== 0
                    NumField {
                        objectName: "transitionSeconds"
                        Layout.fillWidth: true
                        step: .1; suffix: " s"
                        value: root.transition.effectiveSeconds ?? 0.9
                        onCommitted: v => backend.setSlideTransition("seconds", v)
                    }
                }
                CheckBox {
                    objectName: "transitionAutoAdvance"
                    text: qsTr("Move on by itself")
                    checked: (root.transition.advanceAfter ?? -1) >= 0
                    onToggled: backend.setSlideTransition("advanceAfter", checked ? 5 : -1)
                }
                FieldRow {
                    label: qsTr("After")
                    visible: (root.transition.advanceAfter ?? -1) >= 0
                    NumField {
                        objectName: "transitionAdvanceSeconds"
                        Layout.fillWidth: true
                        step: 1; suffix: " s"
                        value: root.transition.advanceAfter ?? 5
                        onCommitted: v => backend.setSlideTransition("advanceAfter", v)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        objectName: "transitionApplyAll"
                        Layout.fillWidth: true
                        text: qsTr("Use on every slide")
                        onClicked: {
                            backend.setSlideTransition("kind", root.transition.effective, true)
                            backend.setSlideTransition("direction", root.transition.direction ?? 0, true)
                            backend.setSlideTransition("seconds", root.transition.effectiveSeconds ?? 0.9, true)
                            backend.setSlideTransition("advanceAfter", root.transition.advanceAfter ?? -1, true)
                        }
                    }
                    Button {
                        objectName: "transitionFollowDeck"
                        Layout.fillWidth: true
                        text: qsTr("Follow the deck")
                        enabled: (root.transition.kind ?? -1) >= 0 || (root.transition.seconds ?? -1) >= 0
                        onClicked: { backend.setSlideTransition("kind", -1); backend.setSlideTransition("seconds", -1) }
                    }
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    text: qsTr("Preview, present and export all read the same clock, so a transition looks the same everywhere.")
                }
            }
        }
    }
}

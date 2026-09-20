import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// The presenter console, laid out as in its concept: a status bar of show
// controls, the current slide with its build progress, the next slide, the
// notes, the clocks and the audience tools, and a strip of every slide.
// The same panel is the Present workspace before a show starts.
Rectangle {
    id: root
    color: Theme.windowBg
    signal starting()
    property int notesSize: Theme.fsPresenterNotes
    property string wallClock: Qt.formatTime(new Date(), "hh:mm")
    function clockText(value) {
        const seconds = Math.abs(Math.floor(value))
        const h = Math.floor(seconds/3600), m = Math.floor(seconds/60)%60, s = seconds%60
        return (value < 0 ? "−" : "") + (h > 0 ? h.toString().padStart(2,"0") + ":" : "00:") + m.toString().padStart(2,"0") + ":" + s.toString().padStart(2,"0")
    }
    Timer { interval: 10000; running: root.visible; repeat: true; triggeredOnStart: true; onTriggered: root.wallClock = Qt.formatTime(new Date(), "hh:mm") }

    // A big square tool, icon over label: Black, White, Freeze.
    component ToolTile: Button {
        display: AbstractButton.TextUnderIcon
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.hToolTile
    }
    component BarRule: Rectangle {
        implicitWidth: Theme.hairline
        Layout.fillHeight: true
        Layout.topMargin: Theme.s3
        Layout.bottomMargin: Theme.s3
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Show bar ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hToolbar + Theme.s1
            color: Theme.panelBg
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.hairline; color: Theme.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s4
                anchors.rightMargin: Theme.s4
                spacing: Theme.s3

                Label {
                    text: presenter.running ? qsTr("Presenter Console") : qsTr("Present")
                    font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading
                }
                Item { Layout.fillWidth: true }
                Row {
                    spacing: Theme.s2
                    Rectangle { width: Theme.szStatusDot + 2; height: width; radius: width / 2; anchors.verticalCenter: parent.verticalCenter
                                color: !presenter.running ? Theme.textMuted : presenter.rehearsal ? Theme.warning : Theme.success }
                    Label { anchors.verticalCenter: parent.verticalCenter
                            text: !presenter.running ? qsTr("Ready") : presenter.rehearsal ? qsTr("Rehearsal") : qsTr("Live")
                            color: !presenter.running ? Theme.textSecondary : presenter.rehearsal ? Theme.warning : Theme.success
                            font.weight: Theme.wHeading }
                }
                BarRule {}
                Icon { name: "monitor"; color: Theme.textSecondary }
                ComboBox {
                    Layout.preferredWidth: Theme.wInspector * 0.75
                    model: presenter.displays; textRole: "label"; currentIndex: presenter.audienceIndex
                    onActivated: presenter.audienceIndex = currentIndex
                    Accessible.name: qsTr("Audience display")
                    ToolTip.visible: hovered && !popup.visible; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Where the audience sees the show")
                }
                Button { icon.name: "arrow-left-right"; text: qsTr("Swap Displays"); enabled: presenter.displays.length > 1; onClicked: presenter.swapDisplays() }
                Button {
                    objectName: "endShowButton"; visible: presenter.running
                    icon.name: "circle-stop"; text: qsTr("End Show"); onClicked: presenter.stop()
                    // Destructive tone: the one red control in the console.
                    contentItem: Row {
                        spacing: Theme.s2
                        Icon { name: "circle-stop"; color: Theme.danger; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: qsTr("End Show"); color: Theme.danger; anchors.verticalCenter: parent.verticalCenter }
                    }
                    background: Rectangle {
                        implicitHeight: Theme.hControl; radius: Theme.rControl
                        color: Theme.withAlpha(Theme.danger, parent.down ? 0.25 : parent.hovered ? 0.18 : 0.10)
                        border.width: Theme.hairline; border.color: Theme.danger
                    }
                    ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("End the show  Esc")
                }
                Button { objectName: "rehearseButton"; visible: !presenter.running; icon.name: "timer"; text: qsTr("Rehearse"); onClicked: { root.starting(); presenter.start(true,true) }
                         ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Run the show in windows, with the clock") }
                Button { visible: !presenter.running; text: qsTr("From This Slide"); onClicked: { root.starting(); presenter.start(true,false) } }
                Button { visible: !presenter.running; icon.name: "play"; text: qsTr("Start Show"); highlighted: true; onClicked: { root.starting(); presenter.start(false,false) }
                         ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Present from the first slide  F5") }
                BarRule {}
                Label { text: root.wallClock; font.pixelSize: Theme.fsSection; font.family: Theme.monoFamily; color: Theme.textPrimary }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.s3
            spacing: Theme.s3

            // ── Left: current slide and the navigator ───────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.s3

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("CURRENT")
                    detail: qsTr("Slide %1 of %2").arg(backend.currentSlide+1).arg(backend.slideCount)
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.s3
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        // Sized to the slide, so the card never letterboxes it in black.
                        Rectangle {
                            readonly property real ratio: backend.slideSize.width / Math.max(1, backend.slideSize.height)
                            anchors.centerIn: parent
                            width: Math.min(parent.width, parent.height * ratio)
                            height: width / ratio
                            color: Theme.showBg
                            SlideView { anchors.fill: parent; deck: backend
                                        time: presenter.running ? presenter.audienceTime : backend.time
                                        editSlide: presenter.running ? -1 : backend.currentSlide }
                            Rectangle { anchors.fill: parent; visible: presenter.running && presenter.blankMode !== 0
                                        color: presenter.blankMode === 2 ? Theme.whiteout : Theme.showBg }
                            Label { anchors.centerIn: parent; visible: presenter.running && presenter.blankMode !== 0
                                    text: presenter.blankMode === 1 ? qsTr("Audience screen is black") : qsTr("Audience screen is white"); color: Theme.textMuted }
                        }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.s3
                            Label {
                                text: presenter.buildCount > 0 ? qsTr("Build %1 of %2 — %3").arg(presenter.buildNumber).arg(presenter.buildCount).arg(presenter.status)
                                                               : presenter.status
                                color: Theme.textSecondary; elide: Text.ElideRight
                                Layout.maximumWidth: Theme.wInspector
                            }
                            // Build progress: a rail with a stop per build.
                            Item {
                                visible: presenter.buildCount > 0
                                Layout.fillWidth: true
                                Layout.maximumWidth: Theme.wInspector * 0.8
                                implicitHeight: Theme.szIcon
                                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 2; radius: 1; color: Theme.borderStrong }
                                Rectangle { anchors.verticalCenter: parent.verticalCenter; height: 2; radius: 1; color: Theme.accent
                                            width: presenter.buildCount > 1 ? parent.width * Math.max(0, presenter.buildNumber - 1) / (presenter.buildCount - 1) : 0 }
                                Repeater {
                                    model: presenter.buildCount
                                    Rectangle {
                                        required property int index
                                        readonly property bool done: index < presenter.buildNumber
                                        width: index === presenter.buildNumber - 1 ? Theme.s3 : Theme.s2; height: width; radius: width / 2
                                        x: (presenter.buildCount > 1 ? parent.width * index / (presenter.buildCount - 1) : 0) - width / 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: done ? Theme.accent : Theme.panelBg
                                        border.width: Theme.hairline; border.color: done ? Theme.accent : Theme.borderStrong
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Button { icon.name: "arrow-left"; text: qsTr("Previous"); implicitHeight: Theme.hToolButton
                                     onClicked: presenter.running ? presenter.previous() : backend.currentSlide = Math.max(0,backend.currentSlide-1) }
                            Button { objectName: "nextCueButton"; text: qsTr("Next"); highlighted: true; implicitHeight: Theme.hToolButton
                                     property bool iconTrailing: true; icon.name: "arrow-right"
                                     onClicked: presenter.running ? presenter.next() : backend.currentSlide = Math.min(backend.slideCount-1,backend.currentSlide+1) }
                        }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.hCardHeader + Theme.s1 + stripHeight + Theme.s3
                    readonly property real stripHeight: Theme.s5 * 5 + Theme.hRow
                    title: qsTr("SLIDE NAVIGATOR")
                    ListView {
                        anchors.fill: parent
                        orientation: ListView.Horizontal
                        spacing: Theme.s3
                        clip: true
                        model: backend.navigator
                        currentIndex: backend.currentSlide
                        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                        ScrollBar.horizontal: ScrollBar {}
                        delegate: Item {
                            required property int index
                            required property var modelData
                            readonly property bool current: index === backend.currentSlide
                            enabled: !presenter.running || !modelData.skipped
                            width: Theme.s5 * 7
                            height: ListView.view.height
                            Rectangle {
                                id: thumb
                                width: parent.width
                                height: width * backend.slideSize.height / Math.max(1, backend.slideSize.width)
                                radius: Theme.rControl
                                color: Theme.showBg
                                border.width: parent.current ? Theme.selectionRing : Theme.hairline
                                border.color: parent.current ? Theme.accent : Theme.border
                                Image { anchors.fill: parent; anchors.margins: parent.border.width; fillMode: Image.PreserveAspectFit
                                        opacity: modelData.skipped ? .4 : 1
                                        source: "image://slides/"+index+"/"+backend.revision; sourceSize.width: 320; cache: false }
                                Rectangle {
                                    visible: modelData.skipped
                                    anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.s1 + 2
                                    width: Theme.szIcon + Theme.s1; height: width; radius: Theme.rHandle
                                    color: Theme.withAlpha(Theme.showBg, 0.7)
                                    Icon { anchors.centerIn: parent; name: "eye-off"; size: Theme.szIcon - 4; color: Theme.danger }
                                }
                                Rectangle {
                                    visible: modelData.buildCount > 0
                                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: Theme.s1 + 2
                                    width: Theme.szIcon + Theme.s1; height: width; radius: Theme.rHandle
                                    color: Theme.withAlpha(Theme.showBg, 0.7)
                                    Icon { anchors.centerIn: parent; name: "play"; size: Theme.szIcon - 4; color: Theme.textPrimary }
                                }
                            }
                            Column {
                                anchors.top: thumb.bottom; anchors.topMargin: Theme.s1 + 2
                                width: parent.width
                                Label { text: index + 1; color: parent.parent.current ? Theme.accent : Theme.textSecondary; font.weight: Theme.wHeading }
                                Label { width: parent.width; text: String(modelData.title || qsTr("Untitled")).replace(/\s+/g, " ").toUpperCase()
                                        elide: Text.ElideRight; maximumLineCount: 1; font.pixelSize: Theme.fsCaption; font.letterSpacing: Theme.capsTracking
                                        color: parent.parent.current ? Theme.accent : modelData.skipped ? Theme.textMuted : Theme.textSecondary }
                            }
                            TapHandler { onTapped: presenter.running ? presenter.jump(index) : backend.currentSlide = index }
                        }
                    }
                }
            }

            // ── Right: next, notes, controls ────────────────────────────────
            ColumnLayout {
                Layout.preferredWidth: Theme.wPresenterSide
                Layout.maximumWidth: Theme.wPresenterSide
                Layout.minimumWidth: Theme.wInspector
                Layout.fillHeight: true
                spacing: Theme.s3

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.hCardHeader + Theme.s1 + nextView.height + Theme.s3
                    title: qsTr("NEXT")
                    detail: presenter.nextSlideIndex >= 0 ? qsTr("Slide %1 of %2").arg(presenter.nextSlideIndex + 1).arg(backend.slideCount) : ""
                    Rectangle {
                        id: nextView
                        width: parent.width
                        height: width * backend.slideSize.height / Math.max(1, backend.slideSize.width)
                        radius: Theme.rControl
                        color: Theme.showBg
                        Image { anchors.fill: parent; fillMode: Image.PreserveAspectFit; cache: false
                                visible: presenter.nextSlideIndex >= 0
                                source: presenter.nextSlideIndex >= 0 ? "image://slides/" + presenter.nextSlideIndex + "/" + backend.revision : ""; sourceSize.width: 640 }
                        Label { anchors.centerIn: parent; visible: presenter.nextSlideIndex < 0; text: qsTr("End of show"); color: Theme.textMuted }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("SPEAKER NOTES")
                    actions: [
                        ToolButton { text: qsTr("A−"); onClicked: root.notesSize = Math.max(Theme.fsBase,root.notesSize-Theme.s1)
                                     ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Smaller notes") },
                        ToolButton { text: qsTr("A"); onClicked: root.notesSize = Theme.fsPresenterNotes
                                     ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Default size") },
                        ToolButton { text: qsTr("A+"); onClicked: root.notesSize = Math.min(Theme.fsPresenterNotes*2,root.notesSize+Theme.s1)
                                     ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Larger notes") }
                    ]
                    ScrollView {
                        id: notesScroll
                        anchors.fill: parent
                        contentWidth: availableWidth
                        clip: true
                        TextArea { width: notesScroll.availableWidth; objectName: "speakerNotes"; text: backend.slideNotes; readOnly: presenter.running
                                   placeholderText: text.length > 0 ? "" : presenter.running ? qsTr("No notes for this slide") : qsTr("Speaker notes…")
                                   textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; font.pixelSize: root.notesSize
                                   onEditingFinished: if (!presenter.running) backend.setSlideNotes(text) }
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitControlsHeight
                    readonly property real implicitControlsHeight: Theme.hCardHeader + Theme.s1 + controls.implicitHeight + Theme.s3
                    title: qsTr("PRESENTATION CONTROLS")
                    ColumnLayout {
                        id: controls
                        width: parent.width
                        spacing: Theme.s3
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.s4
                            ColumnLayout {
                                spacing: 0
                                Label { text: qsTr("Elapsed"); color: Theme.textSecondary }
                                Label { text: root.clockText(presenter.elapsed); font.pixelSize: Theme.fsPresenterClock; font.family: Theme.monoFamily }
                            }
                            Rectangle { implicitWidth: Theme.hairline; Layout.fillHeight: true; color: Theme.border }
                            ColumnLayout {
                                spacing: 0
                                Label { text: qsTr("Countdown"); color: Theme.textSecondary }
                                Label { text: root.clockText(presenter.targetMinutes*60-presenter.elapsed); font.pixelSize: Theme.fsPresenterClock; font.family: Theme.monoFamily
                                        color: presenter.elapsed > presenter.targetMinutes*60 ? Theme.danger : Theme.textPrimary }
                            }
                            Item { Layout.fillWidth: true }
                            ColumnLayout {
                                spacing: Theme.s2
                                Button { Layout.fillWidth: true; icon.name: presenter.paused ? "play" : "pause"; text: presenter.paused ? qsTr("Resume") : qsTr("Pause")
                                         enabled: presenter.running && (backend.playing || presenter.paused); onClicked: presenter.pauseResume() }
                                Button { Layout.fillWidth: true; icon.name: "rotate-ccw"; text: qsTr("Restart"); onClicked: presenter.restartClock()
                                         ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Restart the clock") }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.s2
                            ToolTile { icon.name: "square"; text: qsTr("Black"); checkable: true; checked: presenter.blankMode === 1; enabled: presenter.running
                                       onClicked: presenter.blankMode = checked ? 1 : 0
                                       ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Black the audience screen  B") }
                            ToolTile { icon.name: "sun"; text: qsTr("White"); checkable: true; checked: presenter.blankMode === 2; enabled: presenter.running
                                       onClicked: presenter.blankMode = checked ? 2 : 0
                                       ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("White the audience screen") }
                            ToolTile { icon.name: "snowflake"; text: presenter.frozen ? qsTr("Frozen") : qsTr("Freeze"); checkable: true; checked: presenter.frozen; enabled: presenter.running
                                       onClicked: presenter.frozen = checked
                                       ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Hold the audience view while you move on  F") }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.s2
                            ToolTile { objectName: "annotatePointer"; icon.name: "mouse-pointer-2"; text: qsTr("Pointer")
                                       checkable: true; checked: presenter.annotation === 1; enabled: presenter.running
                                       onClicked: presenter.annotation = checked ? 1 : 0
                                       ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("A dot the audience can follow  P") }
                            ToolTile { objectName: "annotateSpotlight"; icon.name: "scan-search"; text: qsTr("Spotlight")
                                       checkable: true; checked: presenter.annotation === 2; enabled: presenter.running
                                       onClicked: presenter.annotation = checked ? 2 : 0
                                       ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Dim everything but where you point  S") }
                            ToolTile { objectName: "annotateInk"; icon.name: "pencil-line"; text: qsTr("Draw")
                                       checkable: true; checked: presenter.annotation === 3; enabled: presenter.running
                                       onClicked: presenter.annotation = checked ? 3 : 0
                                       ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Draw over the slide  D") }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: presenter.ink.length > 0
                            spacing: Theme.s2
                            Button { objectName: "inkUndo"; Layout.fillWidth: true; text: qsTr("Undo the last line"); onClicked: presenter.undoInk() }
                            Button { objectName: "inkClear"; Layout.fillWidth: true; text: qsTr("Clear"); onClicked: presenter.clearInk() }
                            Button { objectName: "inkKeep"; Layout.fillWidth: true; text: qsTr("Keep on the slide"); onClicked: presenter.keepInkOnSlide() }
                        }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            visible: presenter.annotation !== 0
                            text: qsTr("Drawing stays with the show unless you keep it, and never changes the deck by itself.")
                        }
                        FieldRow {
                            label: qsTr("Showing")
                            ComboBox {
                                objectName: "customShowChoice"
                                Layout.fillWidth: true
                                enabled: !presenter.running
                                textRole: "name"
                                valueRole: "id"
                                model: [{ id: "", name: qsTr("The whole deck"), count: backend.slideCount }]
                                       .concat(backend.customShows)
                                currentIndex: Math.max(0, model.findIndex(show => show.id === backend.activeShow))
                                onActivated: backend.activeShow = currentValue
                            }
                            Button { objectName: "manageShows"; text: qsTr("Shows…"); enabled: !presenter.running
                                     onClicked: showsDialog.open() }
                        }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            visible: backend.activeShow.length > 0
                            text: qsTr("Presenting, previewing and exporting follow this show until you pick the whole deck again.")
                        }
                        FieldRow {
                            label: qsTr("Target")
                            NumField { Layout.fillWidth: true; suffix: qsTr(" min"); value: presenter.targetMinutes; onCommitted: v => presenter.targetMinutes = v }
                        }
                        // What the rehearsal measured, and what to do with it.
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: presenter.timings.length > 0
                            spacing: Theme.s1
                            SectionLabel { text: qsTr("REHEARSED") }
                            Repeater {
                                model: presenter.timings
                                Label {
                                    required property var modelData
                                    required property int index
                                    objectName: "timing" + index
                                    Layout.fillWidth: true
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fsLabel
                                    text: qsTr("Slide %1 · %2 s · now %3 s")
                                          .arg(modelData.index + 1)
                                          .arg(modelData.seconds.toFixed(1))
                                          .arg(modelData.current.toFixed(1))
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Button { objectName: "applyTimings"; Layout.fillWidth: true; text: qsTr("Use these timings")
                                         onClicked: presenter.applyTimings() }
                                Button { objectName: "discardTimings"; Layout.fillWidth: true; text: qsTr("Discard")
                                         onClicked: presenter.clearTimings() }
                            }
                            Label {
                                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                                text: qsTr("Using them makes each slide move on by itself after the time it took, in one undo step.")
                            }
                        }
                    }
                }
            }
        }

        // ── Console status ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hStatusBar
            visible: presenter.running
            color: Theme.panelBg
            Rectangle { anchors.top: parent.top; width: parent.width; height: Theme.hairline; color: Theme.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s4
                anchors.rightMargin: Theme.s4
                spacing: Theme.s3
                Label { text: qsTr("Slide %1 of %2").arg(backend.currentSlide + 1).arg(backend.slideCount); color: Theme.textSecondary; font.pixelSize: Theme.fsLabel }
                Rectangle { implicitWidth: Theme.hairline; implicitHeight: Theme.s4; color: Theme.border }
                Label { visible: presenter.buildCount > 0; text: qsTr("Build %1 of %2").arg(presenter.buildNumber).arg(presenter.buildCount); color: Theme.textSecondary; font.pixelSize: Theme.fsLabel }
                Item { Layout.fillWidth: true }
                Label { text: showGuard.state; color: showGuard.engaged ? Theme.success : Theme.textMuted; font.pixelSize: Theme.fsLabel; elide: Text.ElideRight }
                Rectangle { implicitWidth: Theme.hairline; implicitHeight: Theme.s4; color: Theme.border }
                Icon { name: "monitor"; size: Theme.szIcon - 2; color: Theme.textMuted }
                Label { text: qsTr("Audience: %1").arg(presenter.displays[presenter.audienceIndex]?.label ?? qsTr("none")); color: Theme.textSecondary; font.pixelSize: Theme.fsLabel }
            }
        }
    }

    // Named orders of the slides that already exist, never copies of them.
    Sheet {
        id: showsDialog
        objectName: "customShowsDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.s5 * 2, 720)
        height: Math.min(parent.height - Theme.s5 * 2, 620)
        modal: true
        title: qsTr("Custom shows")
        standardButtons: Dialog.Close
        property string chosen: backend.customShows.length > 0 ? backend.customShows[0].id : ""
        readonly property var show: backend.customShows.find(row => row.id === showsDialog.chosen) ?? ({})
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.s3
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    objectName: "showChooser"
                    Layout.fillWidth: true
                    textRole: "name"
                    valueRole: "id"
                    model: backend.customShows
                    currentIndex: Math.max(0, backend.customShows.findIndex(row => row.id === showsDialog.chosen))
                    onActivated: showsDialog.chosen = currentValue
                }
                TextField {
                    id: showName
                    objectName: "newShowName"
                    Layout.preferredWidth: 200
                    placeholderText: qsTr("Name a new show")
                }
                Button {
                    objectName: "addShow"
                    text: qsTr("Add")
                    enabled: showName.text.trim().length > 0
                    onClicked: {
                        const id = backend.addCustomShow(showName.text)
                        if (id.length > 0) { showsDialog.chosen = id; showName.clear() }
                    }
                }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                text: qsTr("A new show starts from the slides selected in the navigator, or the whole deck. The slides stay where they are: a show is only an order.")
            }
            RowLayout {
                Layout.fillWidth: true
                visible: !!showsDialog.show.id
                TextField {
                    objectName: "showRename"
                    Layout.fillWidth: true
                    text: showsDialog.show.name ?? ""
                    onEditingFinished: backend.renameCustomShow(showsDialog.chosen, text)
                }
                Button { objectName: "presentShow"; text: qsTr("Use this show")
                         onClicked: backend.activeShow = showsDialog.chosen }
                Button { objectName: "removeShow"; text: qsTr("Remove")
                         onClicked: { backend.removeCustomShow(showsDialog.chosen)
                                      showsDialog.chosen = backend.customShows.length > 0 ? backend.customShows[0].id : "" } }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: 2
                    Repeater {
                        model: showsDialog.show.slides ?? []
                        RowLayout {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Label {
                                objectName: "showSlide" + index
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: qsTr("%1. Slide %2 — %3").arg(index + 1).arg(modelData.index + 1)
                                      .arg(backend.navigator[modelData.index]?.title ?? "")
                            }
                            Button { flat: true; objectName: "showSlideUp" + index; icon.name: "arrow-up"
                                     display: AbstractButton.IconOnly; text: qsTr("Earlier")
                                     onClicked: backend.moveCustomShowSlide(showsDialog.chosen, index, index - 1) }
                            Button { flat: true; objectName: "showSlideDown" + index; icon.name: "arrow-down"
                                     display: AbstractButton.IconOnly; text: qsTr("Later")
                                     onClicked: backend.moveCustomShowSlide(showsDialog.chosen, index, index + 1) }
                            Button {
                                flat: true; objectName: "showSlideRemove" + index; icon.name: "x"
                                display: AbstractButton.IconOnly; text: qsTr("Take out")
                                onClicked: {
                                    const kept = (showsDialog.show.slides ?? []).map(row => row.id)
                                    kept.splice(index, 1)
                                    backend.setCustomShowSlides(showsDialog.chosen, kept)
                                }
                            }
                        }
                    }
                    Button {
                        objectName: "addSelectedToShow"
                        Layout.fillWidth: true
                        visible: !!showsDialog.show.id
                        enabled: backend.selectedSlides.length > 0
                        text: qsTr("Add the selected slides")
                        onClicked: {
                            const kept = (showsDialog.show.slides ?? []).map(row => row.id)
                            for (const id of backend.selectedSlides) if (kept.indexOf(id) < 0) kept.push(id)
                            backend.setCustomShowSlides(showsDialog.chosen, kept)
                        }
                    }
                }
            }
        }
    }
}

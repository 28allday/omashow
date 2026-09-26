import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// The guide: every part of the window, what it is for and how to get there.
// Short on purpose — a line to say what it is, one to say where it lives, and
// the handful of things people come to it for. "Take me there" asks the window
// to open the part; the window decides whether it can (see Main.visitHelp).
Sheet {
    id: root
    objectName: "helpGuide"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width - Theme.s5 * 2, 880)
    height: Math.min(parent.height - Theme.s5 * 2, 600)
    modal: true
    title: qsTr("OmaShow guide")
    standardButtons: Dialog.Close
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, .65) }

    // Whether a part can be opened right now (there is a deck, say).
    property var canVisit: function(key) { return true }
    signal visit(string key)
    property string pendingVisit: ""
    onAboutToShow: pendingVisit = ""
    onClosed: {
        const key = pendingVisit
        pendingVisit = ""
        if (key !== "" && canVisit(key)) visit(key)
    }
    signal keysRequested()

    function showTopic(key) {
        for (let i = 0; i < topics.length; ++i)
            if (topics[i].key === key) { list.currentIndex = i; break }
        open()
    }
    onOpened: list.forceActiveFocus()

    readonly property var topics: [
        { key: "start", icon: "house", name: qsTr("Start centre"),
          summary: qsTr("Where a deck begins: a new one, a template or a recent file."),
          reach: qsTr("Shown when no deck is open · File ▸ Start centre… · Ctrl+Shift+N"),
          points: [qsTr("Pick a theme, a slide size and a first slide, then Create presentation."),
                   qsTr("Templates: search, filter by shape, Install… your own, or Use this template."),
                   qsTr("Recent presentations: open, Pin, Reveal in the file manager or Remove."),
                   qsTr("Open deck… opens a .omashow, PowerPoint (.pptx) or Keynote (.key) file.")] },
        { key: "slides", icon: "list", name: qsTr("Slide list"),
          summary: qsTr("Every slide down the left, in order."),
          reach: qsTr("Left of the slide · View ▸ Show the slide list"),
          points: [qsTr("Drag to reorder; double-click or Enter to edit a slide."),
                   qsTr("Shift picks a range, Ctrl picks one more, Ctrl+A picks them all."),
                   qsTr("Right-click to duplicate, delete, skip a slide in the show or start a section."),
                   qsTr("Switch between Thumbnails, Compact and Outline at the top.")] },
        { key: "canvas", icon: "mouse-pointer-2", name: qsTr("The slide"),
          summary: qsTr("Where you build a slide, one object at a time."),
          reach: qsTr("The middle of the window"),
          points: [qsTr("Click to select, Shift+click to add, drag across empty space to pick several."),
                   qsTr("Double-click text to type on the slide; a table or chart to edit its data."),
                   qsTr("Drag to move, pull the handles to resize, the round handle rotates (Shift snaps)."),
                   qsTr("Guides appear as you move; View ▸ Snap to guides turns them off."),
                   qsTr("Ctrl+wheel zooms, the wheel or a middle-button drag pans.")] },
        { key: "toolbar", icon: "shapes", name: qsTr("Toolbar"),
          summary: qsTr("Adding things, on the left; playing and the sidebars, on the right."),
          reach: qsTr("Across the top of the window"),
          points: [qsTr("New Slide and Layout, then Text, Shape, Picture, Table, Chart, Diagram and Media."),
                   qsTr("Arrange: group, lock, hide, link, connect or combine shapes, and the slide size."),
                   qsTr("Review opens comments, findings and spelling; Play starts the show (F5)."),
                   qsTr("Format, Animate and Document at the far right choose the sidebar.")] },
        { key: "format", icon: "pencil", name: qsTr("Format"),
          summary: qsTr("How the selected object looks, or the slide when nothing is selected."),
          reach: qsTr("Format switch in the toolbar · View ▸ Format"),
          points: [qsTr("Style: fill, opacity, corners, saved styles; crop, replace or optimise a picture."),
                   qsTr("Text: font, size, alignment, lists, columns and how text fits its box."),
                   qsTr("Arrange: position, size, rotation, align, distribute and stacking order."),
                   qsTr("With nothing selected: the slide's layout, background and footer fields.")] },
        { key: "animate", icon: "sparkles", name: qsTr("Animate"),
          summary: qsTr("Builds that bring objects on and off, and the move to the next slide."),
          reach: qsTr("Animate switch in the toolbar · View ▸ Animate"),
          points: [qsTr("Choose an object, then Build In or Build Out and pick an effect."),
                   qsTr("Start each build on a click, with or after the one before, or at a time."),
                   qsTr("The timeline shows every build; Space plays, ← and → step a tenth of a second."),
                   qsTr("Slide Transition sets how this slide gives way, or Use on every slide.")] },
        { key: "document", icon: "file-text", name: qsTr("Document"),
          summary: qsTr("Settings for the whole deck."),
          reach: qsTr("Document switch in the toolbar · View ▸ Document"),
          points: [qsTr("Theme, with a way into Masters and layouts."),
                   qsTr("Slide size, language for spelling, and smart punctuation."),
                   qsTr("Shortcuts to Presenter setup and to Review.")] },
        { key: "notes", icon: "notebook-pen", name: qsTr("Presenter notes"),
          summary: qsTr("What you want to say on each slide. Only you see them in the show."),
          reach: qsTr("Under the slide · View ▸ Presenter notes"),
          points: [qsTr("Type notes for the slide you are on."),
                   qsTr("They appear on the presenter console, and in a PDF of slides with notes.")] },
        { key: "masters", icon: "layout-template", name: qsTr("Masters and layouts"),
          summary: qsTr("The design every slide is built on."),
          reach: qsTr("Format ▸ Edit masters and layouts · Document ▸ Edit masters and layouts…"),
          points: [qsTr("Masters hold the background and artwork shared by their layouts."),
                   qsTr("Layouts arrange placeholders for titles, text and pictures."),
                   qsTr("Theme colours and fonts, with a contrast check; slide numbers, date and footer."),
                   qsTr("Unused lists design nothing uses any more, to remove it. Done or Esc goes back.")] },
        { key: "lighttable", icon: "layout-grid", name: qsTr("Light table"),
          summary: qsTr("Every slide at once, laid out in a grid."),
          reach: qsTr("View ▸ Light table · the slide list's view picker"),
          points: [qsTr("Drag to reorder; double-click a slide to edit it."),
                   qsTr("The same menu and keys as the slide list; the arrows move by row."),
                   qsTr("Play starts the show; Done or Esc goes back.")] },
        { key: "review", icon: "message-square-text", name: qsTr("Review"),
          summary: qsTr("Checking the deck before anyone else sees it."),
          reach: qsTr("Review in the toolbar · View ▸ Review"),
          points: [qsTr("Findings: spelling, contrast, missing descriptions and more, each with Show me."),
                   qsTr("Find and replace across the deck, a few slides or this slide."),
                   qsTr("Comments: leave, reply to and resolve them, on a slide or an object."),
                   qsTr("Outline edits every title in one place; Statistics counts words, time and typefaces."),
                   qsTr("Reading sets the order a screen reader follows.")] },
        { key: "setup", icon: "monitor", name: qsTr("Presenter setup"),
          summary: qsTr("Getting ready to present: screens, timings and custom shows."),
          reach: qsTr("Present ▸ Presenter setup… · Document ▸ Displays, timings and shows…"),
          points: [qsTr("Choose the audience display, or Swap Displays."),
                   qsTr("Rehearse to record timings, then Use these timings."),
                   qsTr("Set a target length, and build custom shows from some of the slides."),
                   qsTr("Start Show or From This Slide when you are ready.")] },
        { key: "show", icon: "play", name: qsTr("Playing a show"),
          summary: qsTr("The audience sees the slides; you get the presenter console."),
          reach: qsTr("Play in the toolbar · F5 · Present ▸ Play from this slide"),
          points: [qsTr("Space, → or Page Down moves on; ← or Page Up goes back; Esc ends the show."),
                   qsTr("B blanks to black and F freezes the audience screen."),
                   qsTr("The console shows this slide, the next one, your notes and the clock."),
                   qsTr("W whites the screen; P, S and D are pointer, spotlight and drawing, E clears the ink.")] },
        { key: "export", icon: "share", name: qsTr("Export"),
          summary: qsTr("Turning the deck into something to send, print or play elsewhere."),
          reach: qsTr("File ▸ Export… · Ctrl+E for a PDF"),
          points: [qsTr("PDF: slides, slides with notes, an outline or handouts."),
                   qsTr("Pictures (PNG or JPEG), a film (MP4, no sound yet) or a PowerPoint deck."),
                   qsTr("Print, or Package: the deck and its linked files in one zip."),
                   qsTr("Exports queue at the bottom, where they can be cancelled or tried again.")] },
        { key: "media", icon: "clapperboard", name: qsTr("Film and sound"),
          summary: qsTr("Video and audio on a slide, embedded in the deck or linked to a file."),
          reach: qsTr("Media in the toolbar"),
          points: [qsTr("Embed small clips so the deck travels whole; link large ones."),
                   qsTr("Media preflight lists every clip: approve linked files, relink or embed them."),
                   qsTr("Optimise… makes a picture or clip smaller, keeping the original to restore.")] },
        { key: "commands", icon: "search", name: qsTr("Find a command"),
          summary: qsTr("Type what you want to do instead of hunting through menus."),
          reach: qsTr("Ctrl+K · View ▸ Find a command…"),
          points: [qsTr("Search by name or by what it is for: \"graph\" finds Chart."),
                   qsTr("↑ and ↓ choose, Enter runs it.")] },
        { key: "cli", icon: "square-terminal", name: qsTr("Command line"),
          summary: qsTr("Make, change and export decks from a terminal, without opening a window."),
          reach: qsTr("Any terminal · omashow help lists every command"),
          points: [qsTr("omashow new talk.omashow makes a deck; omashow import brings in PowerPoint or Keynote."),
                   qsTr("inspect lists every slide and the id of each object on it."),
                   qsTr("apply runs a JSON list of changes: the operations the window uses, by the same names."),
                   qsTr("export writes a PDF, pictures, a film, a package or a PowerPoint deck."),
                   qsTr("review reports what Review finds; ops lists every change apply can make."),
                   qsTr("Every command answers in JSON, and never replaces a file without --force.")] },
        { key: "status", icon: "info", name: qsTr("Status and messages"),
          summary: qsTr("What the deck is doing, along the bottom and above the slide."),
          reach: qsTr("Bottom of the window, and a bar above the slide when needed"),
          points: [qsTr("Which slide you are on, what is selected, and whether it is saved."),
                   qsTr("Zoom: out, in, 100%, fit the slide or fit the selection."),
                   qsTr("A bar appears if the file changed on disk or is open elsewhere: reload, keep yours or save as."),
                   qsTr("After opening PowerPoint or Keynote, a bar says what was left out and offers Choose typefaces….")] }
    ]
    readonly property var topic: topics[Math.max(0, list.currentIndex)]

    RowLayout {
        anchors.fill: parent
        spacing: Theme.s4

        ListView {
            id: list
            objectName: "helpTopics"
            Layout.preferredWidth: 210
            Layout.fillHeight: true
            clip: true
            model: root.topics
            currentIndex: 0
            keyNavigationWraps: true
            boundsBehavior: Flickable.StopAtBounds
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Parts of OmaShow")
            delegate: ItemDelegate {
                required property int index
                required property var modelData
                objectName: "helpTopic_" + modelData.key
                width: ListView.view.width
                highlighted: ListView.isCurrentItem
                text: modelData.name
                Accessible.name: modelData.name
                contentItem: RowLayout {
                    spacing: Theme.s2
                    Icon { name: modelData.icon; color: highlighted ? Theme.accent : Theme.textMuted }
                    Label { text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight
                            color: highlighted ? Theme.textPrimary : Theme.textSecondary }
                }
                onClicked: ListView.view.currentIndex = index
            }
        }

        Rectangle { Layout.fillHeight: true; implicitWidth: Theme.hairline; color: Theme.border }

        ScrollView {
            id: detailScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: detailScroll.availableWidth
                spacing: Theme.s2

                RowLayout {
                    spacing: Theme.s2
                    Icon { name: root.topic.icon; color: Theme.accent; size: 20 }
                    Label { objectName: "helpTopicTitle"; text: root.topic.name; color: Theme.textPrimary
                            font.pixelSize: Theme.fsStartHeading; font.weight: Theme.wHeading }
                }
                Label {
                    objectName: "helpTopicSummary"
                    Layout.fillWidth: true
                    text: root.topic.summary
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fsSection
                    wrapMode: Text.WordWrap
                }

                SectionLabel { text: qsTr("WHERE IT IS"); Layout.topMargin: Theme.s3 }
                Label { Layout.fillWidth: true; text: root.topic.reach; color: Theme.textPrimary; wrapMode: Text.WordWrap }

                SectionLabel { text: qsTr("WHAT YOU CAN DO"); Layout.topMargin: Theme.s3 }
                Repeater {
                    model: root.topic.points
                    RowLayout {
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        Label { text: "•"; color: Theme.accent; Layout.alignment: Qt.AlignTop }
                        Label { Layout.fillWidth: true; text: modelData; color: Theme.textPrimary; wrapMode: Text.WordWrap }
                    }
                }

                RowLayout {
                    Layout.topMargin: Theme.s4
                    spacing: Theme.s2
                    Button {
                        objectName: "helpTakeMeThere"
                        text: qsTr("Take me there")
                        icon.name: "arrow-right"
                        // Parts that are everywhere, or outside the window, have nowhere to go.
                        visible: ["canvas", "toolbar", "status", "show", "cli"].indexOf(root.topic.key) < 0
                        enabled: root.canVisit(root.topic.key)
                        // Go once the guide has finished closing: a popup hands the
                        // keyboard back as it closes, and would take it from the part
                        // just opened.
                        onClicked: { root.pendingVisit = root.topic.key; root.close() }
                    }
                    Button {
                        objectName: "helpAllKeys"
                        text: qsTr("All keyboard shortcuts")
                        icon.name: "keyboard"
                        flat: true
                        onClicked: { root.close(); root.keysRequested() }
                    }
                }
            }
        }
    }
}

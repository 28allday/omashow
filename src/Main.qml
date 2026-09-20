import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

ApplicationWindow {
    id: win
    width: 1440
    height: 900
    minimumWidth: 900
    minimumHeight: 560
    visible: true
    // The title carries state: a dirty marker, the deck's name, and where you
    // are in it.
    title: (backend.modified ? "* " : "") + backend.fileName + " — "
           + qsTr("slide %1 of %2").arg(backend.currentSlide + 1).arg(backend.slideCount)
           + " — " + Theme.appName

    property int workspace: 0   // 0 Edit, 1 Design, 2 Animate, 3 Review, 4 Present, 5 Export, 6 Sorter
    readonly property bool slideFocus: (slideNavigator.activeFocus || slideSorter.activeFocus) && !textEntryFocused
    readonly property bool editing: workspace === 0 && !backend.startVisible
    readonly property bool presenting: presenter.running
    // A modal dialog owns the keyboard while it is up.
    readonly property bool dialogOpen: tableEditor.visible || diagramDialog.visible
                                       || layoutApplyDialog.visible || importDesignDialog.visible
                                       || commandPalette.visible
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    color: Theme.windowBg

    // Presenter notes under the Edit canvas, as in the concept; View toggles it.
    property bool notesOpen: (backend.panelState.notesOpen ?? "true") === "true"
    // Panels: dragged wider or narrower, collapsed out of the way, and put back
    // where they were next time.
    property real navigatorWidth: backend.panelState.navigatorWidth ?? Theme.wNavigator
    property real inspectorWidth: backend.panelState.inspectorWidth ?? Theme.wInspector
    property bool navigatorCollapsed: (backend.panelState.navigatorCollapsed ?? "false") === "true"
    property bool inspectorCollapsed: (backend.panelState.inspectorCollapsed ?? "false") === "true"
    onNavigatorWidthChanged: backend.setPanelState("navigatorWidth", Math.round(navigatorWidth))
    onInspectorWidthChanged: backend.setPanelState("inspectorWidth", Math.round(inspectorWidth))
    onNavigatorCollapsedChanged: backend.setPanelState("navigatorCollapsed", navigatorCollapsed)
    onInspectorCollapsedChanged: backend.setPanelState("inspectorCollapsed", inspectorCollapsed)
    onNotesOpenChanged: backend.setPanelState("notesOpen", notesOpen)
    // A panel dragged to nothing, or a remembered width from a bigger screen,
    // comes back to something usable rather than disappearing.
    function sanePanels() {
        const room = Math.max(320, win.width - 360)
        win.navigatorWidth = Math.min(Math.max(Theme.wNavigatorMin, win.navigatorWidth), room / 2)
        win.inspectorWidth = Math.min(Math.max(Theme.wInspectorMin, win.inspectorWidth), room / 2)
    }
    onWidthChanged: win.sanePanels()

    // The grip between a panel and the canvas.
    component PanelGrip: Item {
        id: grip
        property Item panel
        property bool fromLeft: true
        property bool collapsed: false
        signal resized(real delta)
        signal toggled()
        objectName: "panelGrip"
        implicitWidth: Theme.s2
        Layout.fillHeight: true
        Rectangle { anchors.fill: parent; color: hover.hovered || drag.active ? Theme.accent : "transparent"; opacity: .35 }
        HoverHandler { id: hover; cursorShape: Qt.SplitHCursor }
        DragHandler {
            id: drag
            target: null
            yAxis.enabled: false
            onTranslationChanged: {
                if (!active) return
                grip.resized(grip.fromLeft ? translation.x : -translation.x)
            }
        }
        TapHandler { onDoubleTapped: grip.toggled() }
    }
    // Below this width the toolbar drops its labels and keeps the icons.
    readonly property bool compactToolbar: width < 1440
    readonly property string aspectName: {
        const w = backend.slideSize.width, h = backend.slideSize.height
        const ratio = h > 0 ? w / h : 0
        const named = [[16/9, qsTr("16:9 Wide")], [4/3, qsTr("4:3 Standard")], [16/10, qsTr("16:10")], [1, qsTr("1:1 Square")], [9/16, qsTr("9:16 Tall")]]
        for (const pair of named)
            if (Math.abs(ratio - pair[0]) < 0.01) return pair[1]
        return Math.round(w) + " × " + Math.round(h)
    }

    // A flat toolbar button: an icon, a label when there is room, a tooltip
    // that names the shortcut.
    component ToolAction: Button {
        property string tip: ""
        flat: true
        implicitHeight: Theme.hToolButton
        display: win.compactToolbar && icon.name !== "" ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
        ToolTip.visible: hovered && tip !== ""
        ToolTip.delay: Theme.tooltipDelay
        ToolTip.text: tip
        Accessible.name: text
    }
    component ToolbarRule: Rectangle {
        implicitWidth: Theme.hairline
        Layout.fillHeight: true
        Layout.topMargin: Theme.s3
        Layout.bottomMargin: Theme.s3
        Layout.leftMargin: Theme.s1
        Layout.rightMargin: Theme.s1
        color: Theme.border
    }
    component StatusText: Label {
        color: Theme.textSecondary
        font.pixelSize: Theme.fsLabel
    }
    component StatusRule: Rectangle {
        implicitWidth: Theme.hairline
        implicitHeight: Theme.s4
        color: Theme.border
    }
    component Hairline: Rectangle {
        color: Theme.border
        height: Theme.hairline
    }

    // ── Keys ────────────────────────────────────────────────────────────────
    Shortcut { sequences: [StandardKey.Open]; context: Qt.ApplicationShortcut; enabled: !presenter.running
               onActivated: win.confirmThenOpen() }
    Shortcut { sequences: [StandardKey.Save]; context: Qt.ApplicationShortcut; enabled: !presenter.running && !backend.startVisible
               onActivated: { win.commitEditors(); backend.save() } }
    Shortcut { sequences: [StandardKey.SaveAs]; context: Qt.ApplicationShortcut; enabled: !presenter.running && !backend.startVisible
               onActivated: { win.commitEditors(); backend.saveAsDialog() } }
    Shortcut { sequence: "Ctrl+Shift+N"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running
               onActivated: { win.commitEditors(); backend.showStart() } }
    Shortcut { sequence: "Ctrl+E"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible
               onActivated: { win.commitEditors(); backend.exportDialog({ kind: 0 }) } }
    Shortcut { sequence: "Ctrl+Z"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused; onActivated: backend.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused; onActivated: backend.redo() }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible; onActivated: backend.addSlide() }
    Shortcut { sequence: "Ctrl+Shift+Up"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.slideFocus && !win.textEntryFocused;
               onActivated: backend.nudgeSelectedSlides(-1) }
    Shortcut { sequence: "Ctrl+Shift+Down"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.slideFocus && !win.textEntryFocused;
               onActivated: backend.nudgeSelectedSlides(1) }
    Shortcut { sequence: "Ctrl+A"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing); onActivated: backend.selectAll() }
    Shortcut { sequence: "Ctrl+G"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing); onActivated: backend.groupSelected() }
    Shortcut { sequence: "Ctrl+Shift+G"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing); onActivated: backend.ungroupSelected() }
    Shortcut { sequence: "Return"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing && !!backend.selection.groupId); onActivated: backend.enterGroup() }
    Shortcut { sequence: "Ctrl+D"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus; onActivated: win.editing && backend.hasSelection ? backend.duplicateSelected() : backend.duplicateSlide() }
    Shortcut { sequences: [StandardKey.Copy]; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused && !win.slideFocus; onActivated: backend.copyAsync() }
    Shortcut { sequences: [StandardKey.Cut]; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused && !win.slideFocus; onActivated: backend.copyAsync(true) }
    Shortcut { sequences: [StandardKey.Paste]; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused && !win.slideFocus; onActivated: backend.pasteAsync() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing)
               onActivated: backend.addText() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing)
               onActivated: backend.addRect() }
    Shortcut { sequences: [StandardKey.Delete, StandardKey.Backspace]
               context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing)
               onActivated: editCanvas.pathMode===3 ? editCanvas.deletePathNode() : backend.deleteSelected() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible
               onActivated: {
                   if (win.presenting) win.endShow()
                   else if(editCanvas.cropMode) editCanvas.cancelCrop()
                   else if(editCanvas.pathMode!==0) editCanvas.cancelPathTool()
                   else { backend.cancelEdit(); backend.leaveGroup() }
               } }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && (win.workspace === 2 || win.workspace === 4)
               onActivated: backend.togglePlay() }
    Shortcut { sequence: "F5"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible; onActivated: win.startShow() }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused; onActivated: helpSheet.open() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; enabled: !presenter.running; onActivated: win.close() }

    Shortcut { sequences: ["Ctrl++", "Ctrl+="]; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused; onActivated: editCanvas.zoomBy(1.25) }
    Shortcut { sequence: "Ctrl+-"; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused; onActivated: editCanvas.zoomBy(1/1.25) }
    Shortcut { sequence: "Ctrl+0"; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused; onActivated: editCanvas.fit() }
    Shortcut { sequence: "Ctrl+Shift+0"; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused; onActivated: editCanvas.fitSelection() }
    Shortcut { sequence: "Ctrl+1"; enabled: !win.dialogOpen && win.editing && !presenter.running && !win.textEntryFocused; onActivated: editCanvas.actualSize() }

    // Arrows mean different things per workspace, so they are bound per
    // workspace rather than doing something surprising in one of them.
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.workspace !== 1)
               onActivated: win.editing ? backend.nudgeSelected(-Theme.nudge, 0) : backend.step(-0.1) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.workspace !== 1)
               onActivated: win.editing ? backend.nudgeSelected(Theme.nudge, 0) : backend.step(0.1) }
    Shortcut { sequence: "Up"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing)
               onActivated: backend.nudgeSelected(0, -Theme.nudge) }
    Shortcut { sequence: "Down"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible && !win.textEntryFocused && !win.slideFocus && (win.editing)
               onActivated: backend.nudgeSelected(0, Theme.nudge) }
    Shortcut { sequence: "PgDown"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible
               onActivated: backend.currentSlide = backend.currentSlide + 1 }
    Shortcut { sequence: "PgUp"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running && !backend.startVisible
               onActivated: backend.currentSlide = backend.currentSlide - 1 }


    // Never lose work without asking — and never ask when nothing would be lost.
    property bool allowClose: false
    readonly property bool textEntryFocused: activeFocusItem && typeof activeFocusItem.cursorPosition === "number"
    function commitEditors() {
        tableEditor.commitCell()
        editCanvas.commitTextEdit()
        editCanvas.finishCrop()
        contentItem.forceActiveFocus()
    }
    property var pendingAction: null
    function confirmThenNew() { win.guard(() => { if (backend.createDeck(startCentre.themeIndex, startCentre.deckWidth, startCentre.deckHeight, startCentre.layoutIndex)) win.workspace = 0 }) }
    function confirmThenOpen() { win.guard(() => backend.openDialog()) }
    function guard(action) {
        if (backend.operation.length > 0) return
        win.commitEditors()
        if (!backend.modified) {
            action()
            return
        }
        win.pendingAction = action
        discardSheet.open()
    }

    function startShow() { win.commitEditors(); win.workspace = 4; presenter.start(false,false) }
    function endShow() { presenter.stop() }
    SlideSizeDialog { id: slideSizeDialog }
    LayoutApplyDialog { id: layoutApplyDialog }
    // Every command the menus offer, by name and by the words people use for it.
    CommandPalette {
        id: commandPalette
        commands: [
            { group: qsTr("File"), name: qsTr("New deck"), also: "create start blank", run: () => win.confirmThenNew() },
            { group: qsTr("File"), name: qsTr("Open a deck"), also: "load file", shortcut: "Ctrl+O", run: () => win.confirmThenOpen() },
            { group: qsTr("File"), name: qsTr("Open another window"), also: "second copy", run: () => backend.openInNewWindow() },
            { group: qsTr("File"), name: qsTr("Import slides from another deck"), also: "borrow steal reuse", enabled: !backend.startVisible, run: () => backend.importDeckDialog() },
            { group: qsTr("File"), name: qsTr("Save"), also: "write keep", shortcut: "Ctrl+S", run: () => { win.commitEditors(); backend.save() } },
            { group: qsTr("File"), name: qsTr("Save as"), also: "copy elsewhere", shortcut: "Ctrl+Shift+S", run: () => { win.commitEditors(); backend.saveAsDialog() } },
            { group: qsTr("File"), name: qsTr("Start centre"), also: "home recent templates", run: () => { win.commitEditors(); backend.showStart() } },
            { group: qsTr("File"), name: qsTr("Keep this deck as a template"), also: "template reuse starting point", enabled: !backend.startVisible, run: () => { win.commitEditors(); templateName.open() } },
            { group: qsTr("File"), name: qsTr("Install a template"), also: "template add pack", run: () => backend.installTemplateDialog() },
            { group: qsTr("Export"), name: qsTr("Export a PDF"), also: "document handout print pages", shortcut: "Ctrl+E", run: () => { win.commitEditors(); backend.exportDialog({ kind: 0 }) } },
            { group: qsTr("Export"), name: qsTr("Export pictures"), also: "png jpeg images slides", run: () => { win.workspace = 5 } },
            { group: qsTr("Export"), name: qsTr("Export film"), also: "video movie mp4 record", run: () => { win.workspace = 5 } },
            { group: qsTr("Export"), name: qsTr("Print"), also: "paper printer handout", run: () => { win.workspace = 5 } },
            { group: qsTr("Export"), name: qsTr("Package the deck"), also: "zip send share assets", run: () => { win.workspace = 5 } },
            { group: qsTr("Present"), name: qsTr("Present from the start"), also: "show play full screen", shortcut: "F5", run: () => win.startShow() },
            { group: qsTr("Present"), name: qsTr("Present from this slide"), also: "show play current", run: () => { win.commitEditors(); win.workspace = 4; presenter.start(true,false) } },
            { group: qsTr("Present"), name: qsTr("Rehearse in a window"), also: "practise timing", run: () => { win.commitEditors(); win.workspace = 4; presenter.start(false,true) } },
            { group: qsTr("Workspace"), name: qsTr("Edit"), also: "canvas slide", run: () => win.workspace = 0 },
            { group: qsTr("Workspace"), name: qsTr("Design"), also: "theme master layout template", run: () => win.workspace = 1 },
            { group: qsTr("Workspace"), name: qsTr("Animate"), also: "build transition timeline motion", run: () => win.workspace = 2 },
            { group: qsTr("Workspace"), name: qsTr("Review"), also: "comments findings outline statistics accessibility", run: () => win.workspace = 3 },
            { group: qsTr("Workspace"), name: qsTr("Sorter"), also: "light table order overview", run: () => win.workspace = 6 },
            { group: qsTr("Slides"), name: qsTr("Add a slide"), also: "new page", shortcut: "Ctrl+N", enabled: !backend.startVisible, run: () => backend.addSlide() },
            { group: qsTr("Slides"), name: qsTr("Duplicate this slide"), also: "copy page", shortcut: "Ctrl+D", enabled: !backend.startVisible, run: () => backend.duplicateSlide() },
            { group: qsTr("Slides"), name: qsTr("Delete this slide"), also: "remove page", enabled: backend.slideCount > 1, run: () => backend.deleteSlide() },
            { group: qsTr("Slides"), name: qsTr("Skip this slide in the show"), also: "hide omit", enabled: !backend.startVisible, run: () => backend.setSlidesSkipped(true) },
            { group: qsTr("Slides"), name: qsTr("Change the slide size"), also: "aspect ratio widescreen portrait", enabled: !backend.startVisible, run: () => slideSizeDialog.open() },
            { group: qsTr("Insert"), name: qsTr("Text"), also: "words box type", enabled: !backend.startVisible, run: () => { win.workspace = 0; backend.addText() } },
            { group: qsTr("Insert"), name: qsTr("A shape"), also: "rectangle circle arrow", enabled: !backend.startVisible, run: () => { win.workspace = 0; shapeGallery.open() } },
            { group: qsTr("Insert"), name: qsTr("A picture"), also: "image photo png", enabled: !backend.startVisible, run: () => { win.workspace = 0; backend.insertImageDialog() } },
            { group: qsTr("Insert"), name: qsTr("Film or sound"), also: "video audio movie clip", enabled: !backend.startVisible, run: () => { win.workspace = 0; backend.insertMediaDialog() } },
            { group: qsTr("Insert"), name: qsTr("A table"), also: "grid rows columns", enabled: !backend.startVisible, run: () => { win.workspace = 0; backend.addTable() } },
            { group: qsTr("Insert"), name: qsTr("A chart"), also: "graph bar line pie data", enabled: !backend.startVisible, run: () => { win.workspace = 0; backend.addChart() } },
            { group: qsTr("Insert"), name: qsTr("A diagram"), also: "process hierarchy flow", enabled: !backend.startVisible, run: () => { win.workspace = 0; diagramDialog.show() } },
            { group: qsTr("Edit"), name: qsTr("Undo"), also: "back mistake", shortcut: "Ctrl+Z", enabled: backend.canUndo, run: () => backend.undo() },
            { group: qsTr("Edit"), name: qsTr("Redo"), also: "forward again", shortcut: "Ctrl+Shift+Z", enabled: backend.canRedo, run: () => backend.redo() },
            { group: qsTr("Edit"), name: qsTr("Paste the words only"), also: "plain text unformatted", enabled: backend.canPaste, run: () => backend.pasteSpecial(2) },
            { group: qsTr("Edit"), name: qsTr("Paste as a picture"), also: "flatten image", enabled: backend.canPaste, run: () => backend.pasteSpecial(3) },
            { group: qsTr("Edit"), name: qsTr("Find and replace"), also: "search words swap", enabled: !backend.startVisible, run: () => win.workspace = 3 },
            { group: qsTr("Review"), name: qsTr("Comment on this slide"), also: "note remark feedback", enabled: !backend.startVisible, run: () => win.workspace = 3 },
            { group: qsTr("Review"), name: qsTr("Check what would stop someone reading this"), also: "accessibility contrast alt text findings", enabled: !backend.startVisible, run: () => win.workspace = 3 },
            { group: qsTr("Design"), name: qsTr("Apply a layout"), also: "master placeholder arrange", enabled: !backend.startVisible, run: () => { win.workspace = 1; layoutApplyDialog.show(backend.slideDesign.layoutId ?? "") } },
            { group: qsTr("Design"), name: qsTr("Remove what nothing uses"), also: "unused tidy hygiene clean", enabled: !backend.startVisible, run: () => win.workspace = 1 },
            { group: qsTr("View"), name: qsTr("Presenter notes"), also: "speaker script", run: () => win.notesOpen = !win.notesOpen },
            { group: qsTr("View"), name: win.navigatorCollapsed ? qsTr("Show the slide list") : qsTr("Hide the slide list"), also: "navigator panel thumbnails sidebar", run: () => win.navigatorCollapsed = !win.navigatorCollapsed },
            { group: qsTr("View"), name: win.inspectorCollapsed ? qsTr("Show the inspector") : qsTr("Hide the inspector"), also: "panel properties sidebar", run: () => win.inspectorCollapsed = !win.inspectorCollapsed },
            { group: qsTr("View"), name: qsTr("Put the panels back"), also: "reset widths restore layout", run: () => { win.navigatorCollapsed = false; win.inspectorCollapsed = false; win.navigatorWidth = Theme.wNavigator; win.inspectorWidth = Theme.wInspector } },
            { group: qsTr("View"), name: qsTr("Snap to guides"), also: "align magnet", run: () => backend.snapEnabled = !backend.snapEnabled },
            { group: qsTr("View"), name: backend.reducedMotion ? qsTr("Allow movement again") : qsTr("Less movement"), also: "reduced motion animation accessibility vestibular", run: () => backend.reducedMotion = !backend.reducedMotion },
            { group: qsTr("View"), name: backend.highContrast ? qsTr("Ordinary contrast") : qsTr("Stronger contrast"), also: "high contrast accessibility legible", run: () => backend.highContrast = !backend.highContrast },
            { group: qsTr("Help"), name: qsTr("Keyboard shortcuts"), also: "keys help", shortcut: "?", run: () => helpSheet.open() }
        ]
    }
    Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen && !presenter.running
               onActivated: { win.commitEditors(); commandPalette.show() } }
    ImportDesignDialog { id: importDesignDialog }
    Connections { target: backend; function onImportReady() { win.commitEditors(); importDesignDialog.show() } }
    ShapeGallery { id: shapeGallery }
    TableEditor { id: tableEditor }
    DiagramDialog { id: diagramDialog }
    Connections { target: backend; function onTableEditorRequested() { win.commitEditors(); tableEditor.show() } }
    LinkDialog { id: linkDialog }
    MediaOptimisation { id: mediaOptimisation }
    Connections { target: backend; function onMediaOptimisationRequested() { win.commitEditors(); mediaOptimisation.show() } }
    MediaPreflight { id: mediaPreflight; onEditRequested: win.workspace=0 }
    Connections { target: backend; function onMediaPreviewRequested() { win.commitEditors(); win.workspace=2 } }
    Timer { id: mediaProgressDelay; interval: 150; onTriggered: if(backend.busy) mediaProgressDialog.open() }
    Connections {
        target: backend
        function onBusyChanged() {
            if(backend.busy) mediaProgressDelay.start()
            else { mediaProgressDelay.stop(); mediaProgressDialog.close() }
        }
    }
    Dialog {
        id: mediaProgressDialog; objectName: "mediaProgressDialog"; parent: Overlay.overlay; anchors.centerIn: parent
        width: 380; modal: true; closePolicy: Popup.NoAutoClose
        title: backend.mediaJobLabel
        background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rMenu }
        Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, 0.65) }
        contentItem: ColumnLayout {
            Label { text: qsTr("%1% complete").arg(backend.mediaProgress); Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted }
            ProgressBar { Layout.fillWidth: true; from: 0; to: 100; value: backend.mediaProgress }
            Button { objectName: "cancelMediaImport"; text: qsTr("Cancel"); onClicked: backend.cancelMediaJob() }
        }
    }
    Timer { id: operationDelay; interval: 150; onTriggered: if (backend.operation.length > 0) operationDialog.open() }
    Connections {
        target: backend
        function onOperationChanged() {
            if (backend.operation.length > 0) operationDelay.start()
            else { operationDelay.stop(); operationDialog.close() }
        }
    }
    Dialog {
        id: operationDialog; objectName: "documentOperationDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: 380; modal: true; closePolicy: Popup.NoAutoClose
        title: backend.operation
        contentItem: ColumnLayout {
            ProgressBar { Layout.fillWidth: true; indeterminate: true }
            Button { text: qsTr("Cancel"); onClicked: backend.cancelOperation() }
        }
    }
    CombineShapesDialog { id: combineShapesDialog }
    Dialog {
        id: templateName
        objectName: "keepTemplateDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(parent.width - Theme.s5 * 2, 520)
        modal: true; title: qsTr("Keep this deck as a template")
        standardButtons: Dialog.Save | Dialog.Cancel
        onOpened: { field.text = backend.fileName.replace(/\.omashow$/, ""); field.forceActiveFocus() }
        onAccepted: backend.saveAsTemplate(field.text)
        ColumnLayout {
            width: parent.width
            TextField { id: field; objectName: "templateNameField"; Layout.fillWidth: true
                        placeholderText: qsTr("Name for the template") }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                text: qsTr("The whole deck is kept, so whatever is on the slides becomes the starting point. Linked pictures and film stay linked and are not carried.")
            }
        }
    }
    AudienceWindow { id: audienceWindow }
    PresenterConsole { id: consoleWindow }

    StartCentre {
        id: startCentre
        anchors.fill: parent
        visible: backend.startVisible
        onCreateRequested: win.confirmThenNew()
        onOpenRequested: win.confirmThenOpen()
        onRecentRequested: path => win.guard(() => backend.openRecent(path))
        onTemplateRequested: id => win.guard(() => { if (backend.createFromTemplate(id)) win.workspace = 0 })
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: !win.presenting && !backend.startVisible

        // ── Title and menus ─────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hTitleBar
            color: Theme.windowBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s4
                anchors.rightMargin: Theme.s3
                spacing: Theme.s3

                Label {
                    text: Theme.appName
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fsTitle
                    font.weight: Font.Bold
                }

                MenuBar {
                    id: appMenus
                    Layout.fillHeight: true
                    Menu {
                        title: qsTr("File")
                        MenuItem { text: qsTr("Start centre…"); icon.name: "house"; onTriggered: { win.commitEditors(); backend.showStart() } }
                        MenuItem { text: qsTr("Open…"); icon.name: "folder-open"; onTriggered: win.confirmThenOpen() }
                        MenuItem { objectName: "newWindowMenuItem"; text: qsTr("New window"); icon.name: "square-plus"
                                   onTriggered: backend.openInNewWindow() }
                        MenuItem { objectName: "importDeckMenuItem"; text: qsTr("Import from deck…"); icon.name: "folder-open"
                                   enabled: !backend.startVisible; onTriggered: { win.commitEditors(); backend.importDeckDialog() } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Save"); icon.name: "save"; onTriggered: { win.commitEditors(); backend.save() } }
                        MenuItem { text: qsTr("Save as…"); onTriggered: { win.commitEditors(); backend.saveAsDialog() } }
                        MenuItem { objectName: "keepAsTemplate"; text: qsTr("Keep as template…")
                                   enabled: !backend.startVisible
                                   onTriggered: { win.commitEditors(); templateName.open() } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Export PDF…"); icon.name: "file-output"; onTriggered: { win.commitEditors(); backend.exportDialog({ kind: 0 }) } }
                        MenuItem { text: qsTr("Export…"); icon.name: "share"; onTriggered: { win.commitEditors(); win.workspace = 5 } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Quit"); onTriggered: win.close() }
                    }
                    Menu {
                        title: qsTr("Edit")
                        MenuItem { text: backend.canUndo ? qsTr("Undo %1").arg(backend.undoLabel) : qsTr("Undo"); icon.name: "undo-2"; enabled: backend.canUndo; onTriggered: backend.undo() }
                        MenuItem { text: qsTr("Redo"); icon.name: "redo-2"; enabled: backend.canRedo; onTriggered: backend.redo() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Cut"); icon.name: "scissors"; enabled: backend.hasSelection; onTriggered: backend.copyAsync(true) }
                        MenuItem { text: qsTr("Copy"); icon.name: "copy"; enabled: backend.hasSelection; onTriggered: backend.copyAsync() }
                        MenuItem { text: qsTr("Paste"); icon.name: "clipboard-paste"; enabled: backend.canPaste; onTriggered: backend.pasteAsync() }
                        Menu {
                            title: qsTr("Paste special")
                            enabled: backend.canPaste
                            MenuItem { objectName: "pasteKeeping"; text: qsTr("Keep its formatting")
                                       enabled: backend.clipboardKinds.objects ?? false
                                       onTriggered: backend.pasteSpecial(0) }
                            MenuItem { objectName: "pasteMatching"; text: qsTr("Match this deck's text style")
                                       enabled: backend.clipboardKinds.objects ?? false
                                       onTriggered: backend.pasteSpecial(1) }
                            MenuItem { objectName: "pasteWords"; text: qsTr("The words only")
                                       enabled: backend.clipboardKinds.text ?? false
                                       onTriggered: backend.pasteSpecial(2) }
                            MenuItem { objectName: "pastePicture"; text: qsTr("As a picture")
                                       enabled: backend.clipboardKinds.objects ?? false
                                       onTriggered: backend.pasteSpecial(3) }
                        }
                        MenuItem { text: qsTr("Duplicate"); enabled: backend.hasSelection; onTriggered: backend.duplicateSelected() }
                        MenuItem { text: qsTr("Delete"); icon.name: "trash-2"; enabled: backend.hasSelection; onTriggered: backend.deleteSelected() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Select all"); onTriggered: backend.selectAll() }
                    }
                    Menu {
                        title: qsTr("Insert")
                        MenuItem { text: qsTr("Text"); icon.name: "type"; onTriggered: { win.workspace = 0; backend.addText() } }
                        MenuItem { text: qsTr("Shape…"); icon.name: "shapes"; onTriggered: { win.workspace = 0; win.commitEditors(); shapeGallery.open() } }
                        MenuItem { text: qsTr("Picture…"); icon.name: "image"; onTriggered: { win.workspace = 0; win.commitEditors(); backend.insertImageDialog() } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Table…"); icon.name: "table"; onTriggered: { win.workspace = 0; win.commitEditors(); if (backend.addTable()) tableEditor.show() } }
                        MenuItem { text: qsTr("Chart"); icon.name: "chart-column"; onTriggered: { win.workspace = 0; win.commitEditors(); backend.addChart() } }
                        MenuItem { text: qsTr("Diagram…"); icon.name: "workflow"; onTriggered: { win.workspace = 0; win.commitEditors(); diagramDialog.show() } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Audio or video · embed…"); icon.name: "clapperboard"; onTriggered: { win.workspace = 0; win.commitEditors(); backend.insertMediaDialog(true) } }
                        MenuItem { text: qsTr("Audio or video · link…"); onTriggered: { win.workspace = 0; win.commitEditors(); backend.insertMediaDialog(false) } }
                    }
                    Menu {
                        title: qsTr("Slide")
                        MenuItem { text: qsTr("New slide"); icon.name: "square-plus"; onTriggered: backend.addSlide() }
                        MenuItem { text: qsTr("Duplicate slides"); icon.name: "copy"; onTriggered: backend.duplicateSelectedSlides() }
                        MenuItem { text: qsTr("Delete slides"); icon.name: "trash-2"; enabled: backend.slideSelectionState.canDelete; onTriggered: backend.deleteSelectedSlides() }
                        MenuItem { text: backend.slideSelectionState.allSkipped ? qsTr("Include in show") : qsTr("Skip in show"); icon.name: "eye-off"; onTriggered: backend.setSlidesSkipped(!backend.slideSelectionState.allSkipped) }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Slide size…"); icon.name: "scan"; onTriggered: { win.commitEditors(); slideSizeDialog.show() } }
                    }
                    Menu {
                        title: qsTr("Format")
                        MenuItem { text: qsTr("Link or action…"); icon.name: "link"; enabled: backend.hasSelection; onTriggered: linkDialog.show() }
                        MenuItem { text: qsTr("Combine shapes…"); enabled: backend.selectionCount > 1; onTriggered: { win.commitEditors(); combineShapesDialog.open() } }
                        MenuItem { text: qsTr("Themes and layouts"); icon.name: "palette"; onTriggered: { win.commitEditors(); win.workspace = 1 } }
                    }
                    Menu {
                        title: qsTr("Arrange")
                        MenuItem { text: qsTr("Group"); icon.name: "group"; enabled: backend.selectionCount > 1; onTriggered: backend.groupSelected() }
                        MenuItem { text: qsTr("Ungroup"); icon.name: "ungroup"; enabled: !!backend.selection.groupId; onTriggered: backend.ungroupSelected() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Bring forward"); icon.name: "bring-to-front"; enabled: backend.hasSelection; onTriggered: backend.raiseSelected(1) }
                        MenuItem { text: qsTr("Send backward"); icon.name: "send-to-back"; enabled: backend.hasSelection; onTriggered: backend.raiseSelected(-1) }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Lock selected"); icon.name: "lock"; enabled: backend.hasSelection; onTriggered: backend.setSelectedProperty("locked", true) }
                        MenuItem { text: qsTr("Unlock all"); icon.name: "lock-open"; onTriggered: backend.unlockAllObjects() }
                        MenuItem { text: qsTr("Hide selected"); icon.name: "eye-off"; enabled: backend.hasSelection; onTriggered: backend.setSelectedProperty("hidden", true) }
                        MenuItem { text: qsTr("Show all"); icon.name: "eye"; onTriggered: backend.showAllObjects() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Connect objects"); enabled: backend.selectionCount === 2; onTriggered: backend.connectSelected() }
                    }
                    Menu {
                        title: qsTr("View")
                        MenuItem { text: qsTr("Edit"); icon.name: "pencil"; onTriggered: { win.commitEditors(); win.workspace = 0 } }
                        MenuItem { text: qsTr("Design"); icon.name: "palette"; onTriggered: { win.commitEditors(); win.workspace = 1 } }
                        MenuItem { text: qsTr("Animate"); icon.name: "sparkles"; onTriggered: { win.commitEditors(); win.workspace = 2 } }
                        MenuItem { text: qsTr("Slide sorter"); icon.name: "layout-grid"; onTriggered: { win.commitEditors(); win.workspace = 6; slideSorter.focusBrowser() } }
                        MenuItem { objectName: "toggleNavigator"; text: win.navigatorCollapsed ? qsTr("Show the slide list") : qsTr("Hide the slide list")
                                   onTriggered: win.navigatorCollapsed = !win.navigatorCollapsed }
                        MenuItem { objectName: "toggleInspector"; text: win.inspectorCollapsed ? qsTr("Show the inspector") : qsTr("Hide the inspector")
                                   onTriggered: win.inspectorCollapsed = !win.inspectorCollapsed }
                        MenuItem { objectName: "resetPanels"; text: qsTr("Put the panels back")
                                   onTriggered: { win.navigatorCollapsed = false; win.inspectorCollapsed = false
                                                  win.navigatorWidth = Theme.wNavigator; win.inspectorWidth = Theme.wInspector } }
                        MenuItem { objectName: "commandSearchMenuItem"; text: qsTr("Find a command…"); onTriggered: commandPalette.show() }
                        MenuSeparator {}
                        MenuItem { objectName: "reducedMotionItem"; checkable: true; checked: backend.reducedMotion
                                   text: qsTr("Less movement"); onTriggered: backend.reducedMotion = checked }
                        MenuItem { objectName: "highContrastItem"; checkable: true; checked: backend.highContrast
                                   text: qsTr("Stronger contrast"); onTriggered: backend.highContrast = checked }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Zoom in"); icon.name: "zoom-in"; enabled: win.editing; onTriggered: editCanvas.zoomBy(1.25) }
                        MenuItem { text: qsTr("Zoom out"); icon.name: "zoom-out"; enabled: win.editing; onTriggered: editCanvas.zoomBy(1 / 1.25) }
                        MenuItem { text: qsTr("Fit slide"); icon.name: "maximize"; enabled: win.editing; onTriggered: editCanvas.fit() }
                        MenuItem { text: qsTr("Actual size"); enabled: win.editing; onTriggered: editCanvas.actualSize() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Snap to guides"); checkable: true; checked: backend.snapEnabled; onTriggered: backend.snapEnabled = !backend.snapEnabled }
                        MenuItem { text: qsTr("Presenter notes"); checkable: true; checked: win.notesOpen; onTriggered: win.notesOpen = !win.notesOpen }
                    }
                    Menu {
                        title: qsTr("Present")
                        MenuItem { text: qsTr("Play from start"); icon.name: "play"; onTriggered: win.startShow() }
                        MenuItem { text: qsTr("Play from this slide"); onTriggered: { win.commitEditors(); win.workspace = 4; presenter.start(true, false) } }
                        MenuItem { text: qsTr("Rehearse"); icon.name: "timer"; onTriggered: { win.commitEditors(); win.workspace = 4; presenter.start(false, true) } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Presenter setup"); icon.name: "monitor"; onTriggered: { win.commitEditors(); win.workspace = 4 } }
                    }
                    Menu {
                        title: qsTr("Help")
                        MenuItem { text: qsTr("Keyboard shortcuts"); icon.name: "keyboard"; onTriggered: helpSheet.open() }
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    visible: backend.canUndo && win.width > 1100
                    text: qsTr("Ctrl+Z  %1").arg(backend.undoLabel)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fsLabel
                    elide: Text.ElideRight
                    Layout.maximumWidth: Theme.wInspector
                }
                ToolButton {
                    icon.name: "keyboard"
                    onClicked: helpSheet.open()
                    ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay
                    ToolTip.text: qsTr("Keyboard shortcuts  ?")
                    Accessible.name: qsTr("Keyboard shortcuts")
                }
            }
        }

        // ── Workspaces ──────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hWorkspaceBar
            color: Theme.windowBg
            Hairline { anchors.top: parent.top; width: parent.width }
            Hairline { anchors.bottom: parent.bottom; width: parent.width }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.s3
                height: parent.height
                spacing: Theme.s1

                Repeater {
                    model: [
                        { name: qsTr("EDIT"), icon: "pencil", on: true },
                        { name: qsTr("DESIGN"), icon: "palette", on: true },
                        { name: qsTr("ANIMATE"), icon: "sparkles", on: true },
                        { name: qsTr("REVIEW"), icon: "message-square-text", on: true },
                        { name: qsTr("PRESENT"), icon: "presentation", on: true },
                        { name: qsTr("EXPORT"), icon: "share", on: true },
                        { name: qsTr("SORTER"), icon: "layout-grid", on: true }
                    ]

                    // A tab not backed by anything is visibly disabled and says
                    // why, rather than opening an empty room.
                    Item {
                        required property var modelData
                        required property int index
                        readonly property bool current: win.workspace === index
                        Accessible.role: Accessible.PageTab
                        Accessible.name: modelData.name
                        Accessible.description: modelData.on ? qsTr("Workspace") : qsTr("Not built yet")
                        Accessible.checkable: true
                        Accessible.checked: current
                        Accessible.onPressAction: if (modelData.on) win.workspace = index
                        readonly property color ink: !modelData.on ? Theme.borderStrong
                                                     : current ? Theme.accent
                                                     : hover.hovered ? Theme.textPrimary : Theme.textSecondary
                        width: tabRow.implicitWidth + Theme.s4 * 2
                        height: parent.height

                        Rectangle {
                            anchors.fill: parent
                            anchors.bottomMargin: Theme.hairline
                            color: hover.hovered && modelData.on && !parent.current ? Theme.withAlpha(Theme.controlHover, 0.5) : "transparent"
                        }
                        Row {
                            id: tabRow
                            anchors.centerIn: parent
                            spacing: Theme.s2
                            Icon { name: modelData.icon; color: parent.parent.ink; anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: modelData.name
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fsControl
                                font.weight: parent.parent.current ? Theme.wHeading : Theme.wNormal
                                font.letterSpacing: Theme.capsTracking
                                color: parent.parent.ink
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        Rectangle {
                            visible: parent.current
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: Theme.activeUnderline
                            color: Theme.accent
                        }
                        HoverHandler {
                            id: hover
                            cursorShape: modelData.on ? Qt.PointingHandCursor : Qt.ArrowCursor
                        }
                        TapHandler { onTapped: if (modelData.on) { win.commitEditors(); win.workspace = index; if(index === 6) slideSorter.focusBrowser() } }
                        ToolTip.visible: hover.hovered && !modelData.on
                        ToolTip.delay: Theme.tooltipDelay
                        ToolTip.text: qsTr("%1 is not built yet").arg(modelData.name.toLowerCase())
                    }
                }
            }
        }

        // ── Context toolbar ─────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hToolbar
            color: Theme.panelBg
            visible: win.editing

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s3
                anchors.rightMargin: Theme.s3
                spacing: Theme.s1

                ToolAction { icon.name: "house"; text: qsTr("Start centre"); display: AbstractButton.IconOnly; tip: qsTr("Start centre  Ctrl+Shift+N"); onClicked: { win.commitEditors(); backend.showStart() } }
                ToolAction { icon.name: "folder-open"; text: qsTr("Open"); display: AbstractButton.IconOnly; tip: qsTr("Open  Ctrl+O"); onClicked: win.confirmThenOpen() }
                ToolAction {
                    icon.name: "save"
                    text: qsTr("Save")
                    display: AbstractButton.IconOnly
                    tip: backend.modified ? qsTr("Save — unsaved changes  Ctrl+S") : qsTr("Save  Ctrl+S")
                    onClicked: { win.commitEditors(); backend.save() }
                    Rectangle {
                        visible: backend.modified
                        width: Theme.szStatusDot - 2; height: width; radius: width / 2
                        color: Theme.warning
                        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.s1
                    }
                }
                ToolbarRule {}
                ToolAction { icon.name: "square-plus"; text: qsTr("New Slide"); tip: qsTr("New slide  Ctrl+N"); onClicked: backend.addSlide() }
                ToolAction {
                    id: layoutButton
                    icon.name: "layout-template"
                    text: qsTr("Layout")
                    property bool opensMenu: true
                    tip: qsTr("Apply a layout to this slide")
                    onClicked: backend.design.layouts.length ? layoutMenu.popup(layoutButton, 0, layoutButton.height) : backend.setupDesign()
                }
                Menu {
                    id: layoutMenu
                    Instantiator {
                        model: backend.design.layouts
                        delegate: MenuItem {
                            required property var modelData
                            text: modelData.name
                            checkable: true
                            checked: modelData.id === backend.slideDesign.layoutId
                            onTriggered: { win.commitEditors(); layoutApplyDialog.show(modelData.id) }
                        }
                        onObjectAdded: (index, object) => layoutMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => layoutMenu.removeItem(object)
                    }
                }
                ToolbarRule {}
                ToolAction { icon.name: "type"; text: qsTr("Text"); tip: qsTr("Text box  T"); onClicked: backend.addText() }
                ToolAction { objectName: "insertShape"; icon.name: "shapes"; text: qsTr("Shape"); property bool opensMenu: true; tip: qsTr("Shape gallery  S adds a rectangle"); onClicked: { win.commitEditors(); shapeGallery.open() } }
                ToolAction { objectName: "insertPicture"; icon.name: "image"; text: qsTr("Picture"); tip: qsTr("Insert a picture"); onClicked: { win.commitEditors(); backend.insertImageDialog() } }
                ToolAction { objectName: "insertTable"; icon.name: "table"; text: qsTr("Table"); tip: qsTr("Insert a table"); onClicked: { win.commitEditors(); if(backend.addTable()) tableEditor.show() } }
                ToolAction { objectName: "insertChart"; icon.name: "chart-column"; text: qsTr("Chart"); tip: qsTr("Insert a chart"); onClicked: { win.commitEditors(); backend.addChart() } }
                ToolAction { objectName: "insertDiagram"; icon.name: "workflow"; text: qsTr("Diagram"); tip: qsTr("Insert a process or hierarchy diagram"); onClicked: { win.commitEditors(); diagramDialog.show() } }
                ToolAction { id: mediaButton; objectName: "mediaMenuButton"; icon.name: "clapperboard"; text: qsTr("Media"); property bool opensMenu: true; tip: qsTr("Audio and video"); onClicked: mediaMenu.popup(mediaButton,0,mediaButton.height) }
                Menu {
                    id: mediaMenu
                    MenuItem { objectName: "insertEmbeddedMedia"; text: qsTr("Insert audio/video · embed…"); onTriggered: { win.commitEditors(); backend.insertMediaDialog(true) } }
                    MenuItem { objectName: "insertLinkedMedia"; text: qsTr("Insert audio/video · link…"); onTriggered: { win.commitEditors(); backend.insertMediaDialog(false) } }
                    MenuSeparator {}
                    MenuItem { objectName: "openMediaPreflight"; text: qsTr("Media preflight…"); onTriggered: mediaPreflight.open() }
                }
                ToolbarRule {}
                ToolAction { id: arrangeButton; objectName: "arrangeMenuButton"; icon.name: "layers"; text: qsTr("Arrange"); property bool opensMenu: true; tip: qsTr("Arrange, group, lock and connect"); onClicked: arrangeMenu.popup(arrangeButton,0,arrangeButton.height) }
                Menu {
                    id: arrangeMenu
                    MenuItem { text: qsTr("Copy"); icon.name: "copy"; enabled: backend.hasSelection; onTriggered: backend.copyAsync() }
                    MenuItem { text: qsTr("Cut"); icon.name: "scissors"; enabled: backend.hasSelection; onTriggered: backend.copyAsync(true) }
                    MenuItem { text: qsTr("Paste"); icon.name: "clipboard-paste"; enabled: backend.canPaste; onTriggered: backend.pasteAsync() }
                    MenuItem { text: qsTr("Duplicate objects"); enabled: backend.hasSelection; onTriggered: backend.duplicateSelected() }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Select all"); onTriggered: backend.selectAll() }
                    MenuItem { text: qsTr("Group"); icon.name: "group"; enabled: backend.selectionCount > 1; onTriggered: backend.groupSelected() }
                    MenuItem { text: qsTr("Ungroup"); icon.name: "ungroup"; enabled: !!backend.selection.groupId; onTriggered: backend.ungroupSelected() }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Lock selected"); icon.name: "lock"; enabled: backend.hasSelection; onTriggered: backend.setSelectedProperty("locked",true) }
                    MenuItem { text: qsTr("Hide selected"); icon.name: "eye-off"; enabled: backend.hasSelection; onTriggered: backend.setSelectedProperty("hidden",true) }
                    MenuItem { text: qsTr("Unlock all"); icon.name: "lock-open"; onTriggered: backend.unlockAllObjects() }
                    MenuItem { text: qsTr("Show all"); icon.name: "eye"; onTriggered: backend.showAllObjects() }
                    MenuSeparator {}
                    MenuItem { objectName: "objectLinkAction"; text: qsTr("Link or action…"); icon.name: "link"; enabled: backend.hasSelection; onTriggered: linkDialog.show() }
                    MenuItem { objectName: "connectObjectsAction"; text: qsTr("Connect objects"); icon.name: "spline"; enabled: backend.selectionCount===2; onTriggered: backend.connectSelected() }
                    MenuItem { objectName: "combineShapesAction"; text: qsTr("Combine shapes…"); enabled: backend.selectionCount>1; onTriggered: { win.commitEditors(); combineShapesDialog.open() } }
                    MenuSeparator {}
                    MenuItem { objectName: "slideSizeAction"; text: qsTr("Slide size…"); icon.name: "scan"; onTriggered: { win.commitEditors(); slideSizeDialog.show() } }
                }
                Item { Layout.fillWidth: true }
                ToolAction { icon.name: "undo-2"; text: qsTr("Undo"); display: AbstractButton.IconOnly; enabled: backend.canUndo; tip: qsTr("Undo %1  Ctrl+Z").arg(backend.undoLabel); onClicked: backend.undo() }
                ToolAction { icon.name: "redo-2"; text: qsTr("Redo"); display: AbstractButton.IconOnly; enabled: backend.canRedo; tip: qsTr("Redo  Ctrl+Shift+Z"); onClicked: backend.redo() }
                ToolbarRule {}
                ToolAction { objectName: "toolbarPlay"; icon.name: "play"; text: qsTr("Play"); highlighted: true; flat: false; tip: qsTr("Play from the start  F5"); onClicked: win.startShow() }
            }
            Hairline { anchors.bottom: parent.bottom; width: parent.width }
        }
        // ── Body ────────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            SlideNavigator {
                id: slideNavigator
                objectName: "slideNavigatorPanel"
                onEditRequested: win.workspace = 0
                Layout.preferredWidth: win.navigatorCollapsed ? 0 : win.navigatorWidth
                Layout.minimumWidth: win.navigatorCollapsed ? 0 : Theme.wNavigatorMin
                Layout.maximumWidth: win.navigatorCollapsed ? 0 : Number.POSITIVE_INFINITY
                Layout.fillHeight: true
                clip: true
                visible: (win.workspace === 0 || win.workspace === 2) && !win.navigatorCollapsed
            }
            PanelGrip {
                objectName: "navigatorGrip"
                visible: win.workspace === 0 || win.workspace === 2
                collapsed: win.navigatorCollapsed
                onResized: delta => { win.navigatorCollapsed = false
                                      win.navigatorWidth += delta; win.sanePanels() }
                onToggled: win.navigatorCollapsed = !win.navigatorCollapsed
            }

            SlideNavigator { id: slideSorter; sorter: true; Layout.fillWidth: true; Layout.fillHeight: true; visible: win.workspace === 6; onEditRequested: { win.workspace=0; editCanvas.forceActiveFocus() } }

            // Edit: the canvas, and the presenter notes under it.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                visible: win.editing

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.pasteboard
                    EditCanvas { id: editCanvas; anchors.fill: parent }

                    Rectangle {
                        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: Theme.s3
                        z: 20
                        visible: !editCanvas.cropMode
                        color: Theme.panelBg; border.color: Theme.border; radius: Theme.rCard
                        width: snapToggle.implicitWidth + Theme.s1 * 2; height: snapToggle.implicitHeight + Theme.s1 * 2
                        Button {
                            id: snapToggle
                            objectName: "snapToggle"
                            anchors.centerIn: parent
                            flat: true
                            checkable: true
                            checked: backend.snapEnabled
                            icon.name: "magnet"
                            text: qsTr("Snap to guides")
                            onToggled: backend.snapEnabled = checked
                            ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay
                            ToolTip.text: backend.snapEnabled ? qsTr("Snapping to edges, centres and guides") : qsTr("Snapping is off")
                        }
                    }
                }

                // Presenter notes, as in the concept: a header strip and a well.
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: win.notesOpen ? Theme.hCardHeader + Theme.hNotes : Theme.hCardHeader
                    color: Theme.panelBg
                    Hairline { anchors.top: parent.top; width: parent.width }
                    RowLayout {
                        id: notesHeader
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s3
                        height: Theme.hCardHeader
                        spacing: Theme.s2
                        Label { text: qsTr("Presenter Notes"); font.weight: Theme.wHeading; color: Theme.textPrimary }
                        Icon { name: "notebook-pen"; color: Theme.textMuted; visible: backend.slideNotes !== "" }
                        Item { Layout.fillWidth: true }
                        ToolButton {
                            icon.name: win.notesOpen ? "chevron-down" : "chevron-up"
                            onClicked: win.notesOpen = !win.notesOpen
                            ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay
                            ToolTip.text: win.notesOpen ? qsTr("Hide notes") : qsTr("Show notes")
                            Accessible.name: ToolTip.text
                        }
                    }
                    ScrollView {
                        visible: win.notesOpen
                        anchors.top: notesHeader.bottom; anchors.bottom: parent.bottom
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.leftMargin: Theme.s4; anchors.rightMargin: Theme.s4; anchors.bottomMargin: Theme.s3
                        TextArea {
                            objectName: "editNotes"
                            text: backend.slideNotes
                            placeholderText: qsTr("Notes for this slide — only you see them while presenting.")
                            wrapMode: TextArea.Wrap
                            textFormat: TextEdit.PlainText
                            onEditingFinished: if (text !== backend.slideNotes) backend.setSlideNotes(text)
                        }
                    }
                }
            }

            DesignWorkspace {
                onApplyLayoutRequested: layoutId => { win.commitEditors(); layoutApplyDialog.show(layoutId) }
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.workspace === 1
            }

            AnimateWorkspace {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.workspace === 2
            }

            ReviewWorkspace {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.workspace === 3
            }

            PresenterPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.workspace === 4
                onStarting: win.commitEditors()
            }

            ExportWorkspace {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.workspace === 5
            }

            PanelGrip {
                objectName: "inspectorGrip"
                fromLeft: false
                visible: win.editing
                collapsed: win.inspectorCollapsed
                onResized: delta => { win.inspectorCollapsed = false
                                      win.inspectorWidth += delta; win.sanePanels() }
                onToggled: win.inspectorCollapsed = !win.inspectorCollapsed
            }
            Inspector {
                objectName: "inspectorPanel"
                onApplyLayoutRequested: layoutId => { win.commitEditors(); layoutApplyDialog.show(layoutId) }
                Layout.preferredWidth: win.inspectorCollapsed ? 0 : win.inspectorWidth
                Layout.minimumWidth: win.inspectorCollapsed ? 0 : Theme.wInspectorMin
                Layout.maximumWidth: win.inspectorCollapsed ? 0 : Number.POSITIVE_INFINITY
                Layout.fillHeight: true
                clip: true
                visible: win.editing && !win.inspectorCollapsed
            }
        }

        // ── What happened to the file while you were working ────────────────
        Rectangle {
            objectName: "fileStateBar"
            Layout.fillWidth: true
            visible: (backend.fileState.changedOnDisk ?? false) || (backend.fileState.readOnly ?? false)
                     || (backend.fileState.missing ?? false) || (backend.fileState.openedElsewhere ?? false)
            implicitHeight: Theme.hRow + Theme.s2
            color: Theme.withAlpha(Theme.accent, 0.12)
            Hairline { anchors.bottom: parent.bottom; width: parent.width }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s4
                anchors.rightMargin: Theme.s4
                spacing: Theme.s3
                Icon { name: "triangle-alert"; color: Theme.accent }
                Label {
                    objectName: "fileStateMessage"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: backend.fileState.openedElsewhere
                          ? qsTr("%1 is open in another copy of OmaShow. The last one to save wins; the version it replaces is kept beside it as .bak.").arg(backend.fileName)
                          : backend.fileState.missing
                          ? qsTr("%1 is no longer on disk. Saving will write it again.").arg(backend.fileName)
                          : backend.fileState.changedOnDisk
                          ? qsTr("%1 has changed on disk since you opened it.").arg(backend.fileName)
                          : qsTr("%1 is read-only. Saving will ask where to put it instead.").arg(backend.fileName)
                }
                Button {
                    objectName: "reloadFromDisk"
                    visible: backend.fileState.changedOnDisk ?? false
                    text: backend.modified ? qsTr("Discard mine and reload") : qsTr("Reload")
                    onClicked: win.guard(() => backend.reloadFromDisk())
                }
                Button {
                    objectName: "keepMyVersion"
                    visible: backend.fileState.changedOnDisk ?? false
                    text: qsTr("Keep mine")
                    onClicked: backend.keepMyVersion()
                }
                Button {
                    objectName: "saveElsewhere"
                    visible: (backend.fileState.readOnly ?? false) && !(backend.fileState.changedOnDisk ?? false)
                    text: qsTr("Save as…")
                    onClicked: { win.commitEditors(); backend.saveAsDialog() }
                }
            }
        }

        // ── Footer ──────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.hStatusBar
            color: Theme.windowBg
            Hairline { anchors.top: parent.top; width: parent.width }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s4
                anchors.rightMargin: Theme.s3
                spacing: Theme.s3

                StatusText { text: qsTr("Slide %1 of %2").arg(backend.currentSlide + 1).arg(backend.slideCount) }
                StatusRule {}
                StatusText { text: backend.fileName; elide: Text.ElideMiddle; Layout.maximumWidth: Theme.wInspector }
                StatusRule { visible: backend.hasSelection }
                StatusText { visible: backend.hasSelection; text: qsTr("%1 selected").arg(backend.selection.type) }
                Item { Layout.fillWidth: true }
                StatusText {
                    objectName: "statusMessage"
                    text: backend.status; visible: text !== "" && text !== backend.fileName
                    // What just happened, for anything reading the window aloud.
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
                StatusText { visible: showGuard.engaged; text: showGuard.state; color: Theme.success }

                // Zoom lives here, as in the concept, so the canvas stays clear.
                RowLayout {
                    visible: win.editing
                    spacing: 0
                    StatusRule {}
                    ToolButton { objectName: "zoomOut"; icon.name: "zoom-out"; onClicked: editCanvas.zoomBy(1/1.25)
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Zoom out  Ctrl+−") }
                    ToolButton { objectName: "zoomActual"; text: Math.round(editCanvas.zoom*100) + "%"; font.family: Theme.monoFamily; onClicked: editCanvas.actualSize()
                                 implicitWidth: Theme.s5 * 2 + Theme.s3
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Actual size · 100%  Ctrl+1") }
                    ToolButton { objectName: "zoomIn"; icon.name: "zoom-in"; onClicked: editCanvas.zoomBy(1.25)
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Zoom in  Ctrl++") }
                    ToolButton { objectName: "zoomFit"; icon.name: "maximize"; onClicked: editCanvas.fit()
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Fit slide  Ctrl+0") }
                    ToolButton { objectName: "zoomSelection"; icon.name: "scan-search"; enabled: backend.hasSelection; onClicked: editCanvas.fitSelection()
                                 ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Fit selection  Ctrl+Shift+0") }
                }
                StatusRule {}
                ToolButton {
                    text: win.aspectName
                    enabled: !win.presenting && !backend.startVisible
                    onClicked: { win.commitEditors(); slideSizeDialog.show() }
                    ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay
                    ToolTip.text: qsTr("Slide size %1 × %2").arg(Math.round(backend.slideSize.width)).arg(Math.round(backend.slideSize.height))
                }
                StatusRule {}
                Rectangle {
                    width: Theme.szStatusDot; height: width; radius: width / 2
                    color: backend.modified ? Theme.warning : Theme.success
                }
                StatusText {
                    text: backend.modified ? qsTr("Unsaved changes") : qsTr("Saved")
                    color: backend.modified ? Theme.warning : Theme.textSecondary
                }
            }
        }
    }

    // Offered only when a previous session actually left work behind — a clean
    // quit leaves no journal, so an ordinary launch shows nothing.
    Dialog {
        id: recoverySheet
        title: qsTr("Unsaved work was found")
        modal: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: parent
        property var candidates: []

        ColumnLayout {
            spacing: Theme.s3

            Label {
                text: qsTr("OmaShow closed unexpectedly. This work was not saved.")
                color: Theme.textPrimary
                font.pixelSize: Theme.fsControl
            }

            Repeater {
                model: recoverySheet.candidates
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: Theme.s3

                    ColumnLayout {
                        spacing: 0
                        Label {
                            text: modelData.name
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fsControl
                            font.weight: Theme.wHeading
                        }
                        Label {
                            text: modelData.savedAt
                            color: Theme.textMuted
                            font.pixelSize: Theme.fsLabel
                        }
                    }
                    Item { Layout.fillWidth: true }
                    ToolAction {
                        text: qsTr("Discard")
                        onClicked: {
                            backend.discardRecovery(modelData.journalPath)
                            recoverySheet.candidates = backend.recoveryCandidates()
                            if (recoverySheet.candidates.length === 0)
                                recoverySheet.close()
                        }
                    }
                    ToolAction {
                        text: qsTr("Recover")
                        onClicked: {
                            backend.openAsync("file://" + modelData.journalPath, true)
                            recoverySheet.candidates = backend.recoveryCandidates()
                            if (recoverySheet.candidates.length === 0)
                                recoverySheet.close()
                        }
                    }
                }
            }

            Label {
                text: qsTr("Recovering opens a copy — nothing already saved is written over.")
                color: Theme.textMuted
                font.pixelSize: Theme.fsLabel
            }
        }
    }

    onClosing: event => {
        if (backend.operation.length > 0) { event.accepted = false; return }
        if (presenter.running) { event.accepted = false; presenter.stop(); return }
        win.commitEditors()
        if (backend.modified && !win.allowClose) {
            event.accepted = false
            win.guard(() => { win.allowClose = true; win.close() })
        }
    }

    Component.onCompleted: {
        win.sanePanels()
        presenter.attach(audienceWindow,consoleWindow,win)
        const found = backend.recoveryCandidates()
        if (found.length > 0) {
            recoverySheet.candidates = found
            recoverySheet.open()
        }
    }

    Dialog {
        id: discardSheet
        width: Theme.wInspector + Theme.s5 * 2
        objectName: "discardDialog"
        title: qsTr("Discard unsaved changes?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Discard | Dialog.Save | Dialog.Cancel

        Label {
            width: discardSheet.availableWidth
            wrapMode: Text.Wrap
            text: qsTr("%1 has changes that are not saved.").arg(backend.fileName)
            color: Theme.textPrimary
            font.pixelSize: Theme.fsControl
        }
        onDiscarded: { const go = win.pendingAction; win.pendingAction = null; discardSheet.close(); if (go) go() }
        onAccepted: backend.save()
        onRejected: win.pendingAction = null
    }

    Dialog {
        id: errorSheet
        width: Theme.wInspector + Theme.s5 * 2
        title: qsTr("That did not work")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        property string message
        Label {
            text: errorSheet.message
            color: Theme.textPrimary
            font.pixelSize: Theme.fsControl
            wrapMode: Text.Wrap
            width: Theme.wInspector
        }
    }

    Connections {
        target: backend
        function onImageCropEditorRequested() { win.workspace=0; editCanvas.startCrop() }
        function onLinkEditorRequested() { linkDialog.show() }
        function onSaved() {
            const go = win.pendingAction
            win.pendingAction = null
            if (go) go()
        }
        function onSaveCanceled() { win.pendingAction = null }
        function onFailed(message) {
            win.pendingAction = null
            errorSheet.message = message
            errorSheet.open()
        }
    }

    Dialog {
        id: helpSheet
        title: qsTr("Keys")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close

        ColumnLayout {
            spacing: Theme.s1
            Repeater {
                model: [
                    qsTr("Ctrl+O / Ctrl+S — open / save"),
                    qsTr("Ctrl+Shift+S — save as"),
                    qsTr("Ctrl+Shift+N — Start centre"),
                    qsTr("Ctrl+E — export PDF"),
                    qsTr("Double-click text — edit it on the slide"),
                    qsTr("T / S — add text / shape"),
                    qsTr("Delete — remove the selected object"),
                    qsTr("Arrows — nudge (step time in Animate)"),
                    qsTr("Ctrl+N — new slide"),
                    qsTr("Ctrl+D — duplicate selected objects or slide"),
                    qsTr("Ctrl+C / Ctrl+X / Ctrl+V — copy / cut / paste"),
                    qsTr("Ctrl++ / Ctrl+- — zoom in / out"),
                    qsTr("Ctrl+0 / Ctrl+1 — fit slide / 100%"),
                    qsTr("Ctrl+Shift+0 — fit selection"),
                    qsTr("Rotation handle · Shift snaps to 15°"),
                    qsTr("Slides: Shift range, Ctrl toggle, Ctrl+A all"),
                    qsTr("Slides: Ctrl+D duplicate, Delete remove"),
                    qsTr("Sections: Ctrl+← / Ctrl+→ collapse / expand"),
                    qsTr("Sections: Ctrl+Alt+↑ / ↓ move section"),
                    qsTr("Ctrl+wheel — zoom; middle drag or wheel — pan"),
                    qsTr("PgUp / PgDn — previous / next slide"),
                    qsTr("Ctrl+Z / Ctrl+Shift+Z — undo / redo"),
                    qsTr("F5 — start the show, Escape leaves"),
                    qsTr("Space — play / pause outside Edit"),
                    qsTr("Ctrl+Q — quit")
                ]
                Label {
                    required property string modelData
                    text: modelData
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fsControl
                }
            }
        }
    }
}

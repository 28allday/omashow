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
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fsControl
    color: Theme.windowBg

    // Presenter notes under the Edit canvas, as in the concept; View toggles it.
    property bool notesOpen: true
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
               onActivated: { win.commitEditors(); backend.exportPdfDialog(false) } }
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
    AudienceWindow { id: audienceWindow }
    PresenterConsole { id: consoleWindow }

    StartCentre {
        id: startCentre
        anchors.fill: parent
        visible: backend.startVisible
        onCreateRequested: win.confirmThenNew()
        onOpenRequested: win.confirmThenOpen()
        onRecentRequested: path => win.guard(() => backend.openRecent(path))
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
                        MenuItem { objectName: "importDeckMenuItem"; text: qsTr("Import from deck…"); icon.name: "folder-open"
                                   enabled: !backend.startVisible; onTriggered: { win.commitEditors(); backend.importDeckDialog() } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Save"); icon.name: "save"; onTriggered: { win.commitEditors(); backend.save() } }
                        MenuItem { text: qsTr("Save as…"); onTriggered: { win.commitEditors(); backend.saveAsDialog() } }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Export PDF…"); icon.name: "file-output"; onTriggered: { win.commitEditors(); backend.exportPdfDialog(false) } }
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
                onEditRequested: win.workspace = 0
                Layout.preferredWidth: Theme.wNavigator
                Layout.minimumWidth: Theme.wNavigatorMin
                Layout.fillHeight: true
                visible: win.workspace === 0 || win.workspace === 2
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

            // Export — PDF is real; the other targets are listed as not built
            // rather than offered and then failing.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.workspace === 5

                ColumnLayout {
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: Theme.s5 * 2
                    width: Math.min(parent.width - Theme.s5 * 2, Theme.wInspector * 2.4)
                    spacing: Theme.s3

                    Label { text: qsTr("Export"); font.pixelSize: Theme.fsStartHeading; font.weight: Theme.wHeading }
                    Label { text: qsTr("%1 · %2 slides").arg(backend.fileName).arg(backend.slideCount); color: Theme.textSecondary; Layout.bottomMargin: Theme.s3 }

                    Card {
                        Layout.fillWidth: true
                        implicitHeight: pdfCard.implicitHeight + Theme.s4 * 2
                        ColumnLayout {
                            id: pdfCard
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Theme.s2
                            spacing: Theme.s3
                            RowLayout {
                                spacing: Theme.s3
                                Rectangle {
                                    implicitWidth: Theme.hToolTile - Theme.s3; implicitHeight: implicitWidth; radius: Theme.rCard
                                    color: Theme.withAlpha(Theme.accent, 0.12); border.color: Theme.withAlpha(Theme.accent, 0.4)
                                    Icon { anchors.centerIn: parent; name: "file-text"; size: Theme.szIconLarge + 4; color: Theme.accent }
                                }
                                ColumnLayout {
                                    spacing: 2
                                    Label { text: qsTr("PDF document"); font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading }
                                    Label { text: qsTr("Real text, not outlines — searchable and quotable."); color: Theme.textSecondary }
                                }
                            }
                            Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }
                            CheckBox { id: stagePages; text: qsTr("A page per build stage (handout)") }
                            CheckBox { id: includeSkipped; text: qsTr("Include skipped slides") }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                Button {
                                    icon.name: "file-output"
                                    text: qsTr("Export PDF…")
                                    highlighted: true
                                    onClicked: backend.exportPdfDialog(stagePages.checked,includeSkipped.checked)
                                    ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Export PDF  Ctrl+E")
                                }
                            }
                        }
                    }

                    SectionLabel { text: qsTr("NOT BUILT YET"); Layout.topMargin: Theme.s3 }
                    Repeater {
                        model: [
                            { name: qsTr("Images"), icon: "file-image", why: qsTr("bin/shot renders frames; a slide-range export is not built") },
                            { name: qsTr("Video"), icon: "file-play", why: qsTr("the evaluator makes it frame-exact; no encoder is wired in yet") },
                            { name: qsTr("Office presentation (.pptx)"), icon: "presentation", why: qsTr("Gate 3") },
                            { name: qsTr("OpenDocument (.odp)"), icon: "presentation", why: qsTr("Gate 3") }
                        ]
                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: Theme.hRow + Theme.s3
                            color: "transparent"
                            border.color: Theme.border
                            border.width: Theme.hairline
                            radius: Theme.rCard
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.s4
                                anchors.rightMargin: Theme.s4
                                spacing: Theme.s3
                                Icon { name: modelData.icon; color: Theme.textMuted }
                                Label { text: modelData.name; color: Theme.textSecondary }
                                Item { Layout.fillWidth: true }
                                Label { text: modelData.why; color: Theme.textMuted; font.pixelSize: Theme.fsLabel; elide: Text.ElideRight; Layout.maximumWidth: parent.width * 0.6 }
                            }
                        }
                    }
                }
            }

            Inspector {
                onApplyLayoutRequested: layoutId => { win.commitEditors(); layoutApplyDialog.show(layoutId) }
                Layout.preferredWidth: Theme.wInspector
                Layout.minimumWidth: Theme.wInspectorMin
                Layout.fillHeight: true
                visible: win.editing
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
                StatusText { text: backend.status; visible: text !== "" && text !== backend.fileName }
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

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

FocusScope {
    id: root
    property bool sorter: false
    property int mode: 0 // thumbnails, compact, outline
    property int dragFrom: -1 // authored slide indices, never filtered row numbers
    property int dropIndex: -1
    property int dropBoundarySlide: -1
    property string sectionTarget: ""
    readonly property var current: backend.navigator[backend.currentSlide] ?? ({})
    readonly property var rows: visible ? backend.browserSlides : []
    readonly property var view: sorter ? grid : list
    readonly property real cardWidth: sorter ? grid.cellWidth : list.width
    // List rows: a number column, then the thumbnail. Sorter cells: the
    // thumbnail with its number and title under it.
    readonly property real numberWidth: sorter ? 0 : Theme.s5
    readonly property real thumbWidth: sorter ? cardWidth - Theme.s3 * 2 : cardWidth - numberWidth - Theme.s3 - Theme.s2
    readonly property real thumbHeight: thumbWidth * backend.slideSize.height / Math.max(1, backend.slideSize.width)
    readonly property real cardHeight: Theme.hRow + thumbHeight + Theme.hRow + Theme.s3
    function bodyHeight(row) {
        if (row.collapsed) return Theme.hRow
        if (mode === 1) return Theme.hRow
        if (mode === 2) return Theme.hRow + Theme.hControl * 3
        return thumbHeight
    }
    // What each badge on a thumbnail means.
    function badgeTip(name) {
        return name === "sparkles" ? qsTr("Has builds")
             : name === "notebook-pen" ? qsTr("Has presenter notes")
             : name === "clapperboard" ? qsTr("Has film or sound")
             : name === "message-square-text" ? qsTr("Has open comments")
             : name === "arrow-left-right" ? qsTr("Has its own transition")
             : name === "triangle-alert" ? qsTr("A picture here has no description")
             : ""
    }
    signal editRequested()
    signal lightTableRequested()
    Item { id: keyboardFocus; focus: true }
    function focusBrowser() { keyboardFocus.forceActiveFocus() }

    function visibleIndex(slide) {
        const section = backend.navigator[slide]?.sectionId ?? ""
        for(let i=0;i<rows.length;++i)
            if(rows[i].index === slide || (rows[i].collapsed && rows[i].sectionId === section)) return i
        return -1
    }
    function selectedInSection(id) {
        const selected = backend.selectedSlides
        return backend.navigator.filter(row => row.sectionId === id && selected.indexOf(row.id) >= 0).length
    }
    function choose(row,toggle,range) {
        if(row.collapsed) backend.selectSection(row.sectionId,toggle,range)
        else backend.selectSlide(row.index,toggle,range)
    }
    function revealCurrent() {
        const at = visibleIndex(backend.currentSlide)
        if(at >= 0) view.positionViewAtIndex(at,ListView.Contain)
    }
    function toggleSection(id) {
        root.focusBrowser()
        backend.setSectionCollapsed(id,backend.collapsedSections.indexOf(id) < 0)
    }
    function openSection(id,button) {
        sectionTarget = id
        contextSectionMenu.popup(button,0,button.height)
    }
    Connections {
        target: backend
        function onCurrentSlideChanged() { Qt.callLater(root.revealCurrent) }
        function onBrowserChanged() { Qt.callLater(root.revealCurrent) }
    }
    Rectangle {
        anchors.fill: parent; color: Theme.panelBg
        Rectangle { anchors.right: parent.right; width: Theme.hairline; height: parent.height; color: Theme.border; visible: !root.sorter }
    }
    Keys.onPressed: event => {
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0
        const alt = (event.modifiers & Qt.AltModifier) !== 0
        const step = root.sorter ? Math.max(1,Math.floor(grid.width/grid.cellWidth)) : 1
        if(ctrl && alt && (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) backend.moveSection(root.current.sectionId,event.key === Qt.Key_Up ? -1 : 1)
        else if(ctrl && !shift && (event.key === Qt.Key_Left || event.key === Qt.Key_Right)) backend.setSectionCollapsed(root.current.sectionId,event.key === Qt.Key_Left)
        else if(ctrl && event.key === Qt.Key_A) backend.selectAllSlides()
        else if(ctrl && event.key === Qt.Key_D) backend.duplicateSelectedSlides()
        else if(ctrl && shift && (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) backend.nudgeSelectedSlides(event.key === Qt.Key_Up ? -1 : 1)
        else if(event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) backend.deleteSelectedSlides()
        else if(event.key === Qt.Key_Return) { if(root.current.sectionId) backend.setSectionCollapsed(root.current.sectionId,false); root.editRequested() }
        else if([Qt.Key_Up,Qt.Key_Down,Qt.Key_Left,Qt.Key_Right,Qt.Key_Home,Qt.Key_End].includes(event.key)) {
            const delta = event.key === Qt.Key_Up ? -step : event.key === Qt.Key_Down ? step : event.key === Qt.Key_Left ? -1 : 1
            const at = event.key === Qt.Key_Home ? 0 : event.key === Qt.Key_End ? rows.length-1 : Math.max(0,Math.min(rows.length-1,visibleIndex(backend.currentSlide)+delta))
            if(rows[at]) root.choose(rows[at],ctrl,shift)
            root.revealCurrent()
        } else { event.accepted = false; return }
        event.accepted = true
    }
    ColumnLayout {
        id: header
        width: parent.width
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.hCardHeader + Theme.s1
            Layout.leftMargin: Theme.s3
            Layout.rightMargin: Theme.s1
            spacing: 0
            Label { text: root.sorter ? qsTr("Slide sorter") : qsTr("Slides"); font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading; color: Theme.textPrimary }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: modeBox
                objectName: "navigatorMode"; visible: !root.sorter; flat: true
                // As wide as the longest view it can show, so "Thumbnails" is never
                // clipped, whatever the language or the desktop's text size.
                implicitWidth: Math.ceil(model.reduce((widest, label) => Math.max(widest, modeFont.advanceWidth(label)), 0))
                               + leftPadding + rightPadding
                               + (contentItem ? (contentItem.leftPadding ?? 0) + (contentItem.rightPadding ?? 0) : 0)
                implicitHeight: Theme.hControl
                FontMetrics { id: modeFont; font: modeBox.font }
                // The fourth choice is the light table: every slide at once,
                // across the whole window, as Keynote's View menu offers it.
                model: [qsTr("Thumbnails"),qsTr("Compact"),qsTr("Outline"),qsTr("Light table")]; currentIndex: root.mode
                onActivated: index => { if (index === 3) { currentIndex = root.mode; root.lightTableRequested() } else root.mode = index }
                ToolTip.visible: hovered && !popup.visible; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Show slides as thumbnails, titles, an outline or the light table")
            }
            ToolButton {
                icon.name: "plus"; onClicked: backend.addSlide()
                ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("New slide  Ctrl+N"); Accessible.name: qsTr("New slide")
            }
            ToolButton {
                id: actionsButton; objectName: root.sorter ? "sorterActions" : "slideActions"
                icon.name: "menu"; onClicked: actionsMenu.popup(actionsButton,0,actionsButton.height)
                ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Slide actions · Shift selects a range, Ctrl toggles"); Accessible.name: qsTr("Slide actions")
            }
        }
        Label {
            visible: backend.selectedSlides.length > 1
            Layout.leftMargin: Theme.s3; Layout.bottomMargin: Theme.s1
            text: qsTr("%1 slides selected").arg(backend.selectedSlides.length)
            color: Theme.accent; font.pixelSize: Theme.fsLabel
        }
    }
    component SectionCommands: Menu {
        property string sectionId: ""
        readonly property var info: { backend.revision; backend.collapsedSections; return backend.sectionInfo(sectionId) }
        background: Rectangle { implicitWidth: Theme.wInspector; color: Theme.panelRaised; border.color: Theme.border; radius: Theme.rMenu }
        MenuItem { objectName: "toggleSectionAction"; text: info.collapsed ? qsTr("Expand section") : qsTr("Collapse section"); onTriggered: root.toggleSection(sectionId) }
        MenuItem { objectName: "selectSectionAction"; text: qsTr("Select section slides"); onTriggered: backend.selectSection(sectionId) }
        MenuSeparator {}
        MenuItem { objectName: "moveSectionUpAction"; text: qsTr("Move section up"); enabled: info.canMoveUp ?? false; onTriggered: backend.moveSection(sectionId,-1) }
        MenuItem { objectName: "moveSectionDownAction"; text: qsTr("Move section down"); enabled: info.canMoveDown ?? false; onTriggered: backend.moveSection(sectionId,1) }
        MenuSeparator {}
        MenuItem { objectName: "renameSectionAction"; text: qsTr("Rename section…"); onTriggered: sectionName.ask(sectionId) }
        MenuItem { objectName: "removeSectionAction"; text: qsTr("Remove section, keep slides"); onTriggered: backend.removeSection(sectionId) }
    }
    SectionCommands { id: contextSectionMenu; objectName: root.sorter ? "sorterSectionMenu" : "navigatorSectionMenu"; sectionId: root.sectionTarget }
    Menu {
        id: actionsMenu
        background: Rectangle { implicitWidth: Theme.wInspector; color: Theme.panelRaised; border.color: Theme.border; radius: Theme.rMenu }
        objectName: root.sorter ? "sorterMenu" : "navigatorMenu"
        MenuItem { text: qsTr("Select all slides"); onTriggered: backend.selectAllSlides() }
        MenuItem { objectName: "duplicateSlidesAction"; text: qsTr("Duplicate selected slides"); onTriggered: backend.duplicateSelectedSlides() }
        MenuItem { text: qsTr("Delete selected slides"); enabled: backend.slideSelectionState.canDelete; onTriggered: backend.deleteSelectedSlides() }
        MenuItem { objectName: "skipSlidesAction"; text: backend.slideSelectionState.allSkipped ? qsTr("Include in show") : qsTr("Skip in show"); onTriggered: backend.setSlidesSkipped(!backend.slideSelectionState.allSkipped) }
        MenuSeparator {}
        MenuItem { text: qsTr("Move up · Ctrl+Shift+↑"); onTriggered: backend.nudgeSelectedSlides(-1) }
        MenuItem { text: qsTr("Move down · Ctrl+Shift+↓"); onTriggered: backend.nudgeSelectedSlides(1) }
        MenuSeparator {}
        MenuItem { objectName: "startSectionAction"; text: qsTr("Start section here…"); onTriggered: sectionName.ask("") }
        SectionCommands { title: qsTr("Current section"); sectionId: root.current.sectionId ?? ""; enabled: !!sectionId }
        MenuSeparator {}
        MenuItem { objectName: "collapseAllSectionsAction"; text: qsTr("Collapse all sections"); onTriggered: backend.collapseAllSections(true) }
        MenuItem { objectName: "expandAllSectionsAction"; text: qsTr("Expand all sections"); onTriggered: backend.collapseAllSections(false) }
    }
    ListView {
        id: list
        objectName: "navigatorList"
        visible: !root.sorter
        anchors.top: header.bottom; anchors.bottom: parent.bottom; width: parent.width
        clip: true; model: visible ? backend.browserModel : null; currentIndex: root.visibleIndex(backend.currentSlide)
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: slideCard
    }
    GridView {
        id: grid
        objectName: "sorterGrid"
        visible: root.sorter
        anchors.top: header.bottom; anchors.bottom: parent.bottom; width: parent.width
        clip: true
        cellWidth: width / Math.max(1,Math.floor(width/Theme.wInspector))
        cellHeight: root.cardHeight
        model: visible ? backend.browserModel : null; currentIndex: root.visibleIndex(backend.currentSlide)
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: slideCard
    }
    Component {
        id: slideCard
        Item {
            id: row
            required property int index // visible row
            required property var modelData
            readonly property int slide: modelData.index
            readonly property int selectedCount: modelData.collapsed ? root.selectedInSection(modelData.sectionId) : (backend.selectedSlides.indexOf(modelData.id) >= 0 ? 1 : 0)
            readonly property bool selected: selectedCount > 0
            readonly property bool current: !modelData.collapsed && slide === backend.currentSlide
            readonly property real headerHeight: modelData.sectionStart || root.sorter ? Theme.hRow : 0
            objectName: (root.sorter ? "sorterRow" : "slideRow") + slide
            // A slide in a list, said the way the list says it.
            Accessible.role: Accessible.ListItem
            Accessible.name: qsTr("Slide %1 — %2").arg(slide + 1).arg(modelData.title ?? "")
            Accessible.description: [modelData.skipped ? qsTr("skipped") : "",
                                     modelData.notes ? qsTr("has notes") : "",
                                     modelData.commentCount ? qsTr("has comments") : "",
                                     modelData.buildCount ? qsTr("has builds") : "",
                                     modelData.transitionOwn ? qsTr("arrives its own way") : "",
                                     modelData.undescribed ? qsTr("something here needs a description") : ""]
                                    .filter(part => part.length > 0).join(", ")
            Accessible.selectable: true
            Accessible.selected: selected
            Accessible.onPressAction: backend.currentSlide = slide
            width: root.cardWidth
            height: root.sorter ? root.cardHeight : headerHeight + root.bodyHeight(modelData) + Theme.s2

            // Section header: chevron, NAME (first–last), and its commands.
            RowLayout {
                id: sectionHeader
                visible: row.modelData.sectionStart
                x: Theme.s1; width: row.width - Theme.s2; height: row.headerHeight
                spacing: 0
                ToolButton {
                    objectName: (root.sorter ? "sorterSectionToggle" : "sectionToggle") + row.slide
                    icon.name: row.modelData.collapsed ? "chevron-right" : "chevron-down"
                    implicitWidth: Theme.szIconHit - Theme.s1; implicitHeight: Theme.szIconHit - Theme.s1
                    onClicked: root.toggleSection(row.modelData.sectionId)
                    ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay
                    ToolTip.text: row.modelData.collapsed ? qsTr("Expand section · Ctrl+→") : qsTr("Collapse section · Ctrl+←")
                    Accessible.name: ToolTip.text + ": " + row.modelData.sectionName
                }
                Label {
                    Layout.fillWidth: true
                    text: row.modelData.sectionName.toUpperCase() + (row.modelData.sectionCount > 0 ? "  (" + (row.modelData.sectionFirst + 1) + "–" + (row.modelData.sectionLast + 1) + ")" : "")
                    elide: Text.ElideRight
                    color: row.selectedCount > 0 && row.modelData.collapsed ? Theme.accent : Theme.textSecondary
                    font.pixelSize: Theme.fsLabel; font.weight: Theme.wHeading; font.letterSpacing: Theme.capsTracking
                }
                ToolButton {
                    id: sectionButton
                    objectName: (root.sorter ? "sorterSectionActions" : "sectionActions") + row.slide
                    icon.name: "ellipsis"
                    implicitWidth: Theme.szIconHit - Theme.s1; implicitHeight: Theme.szIconHit - Theme.s1
                    onClicked: root.openSection(row.modelData.sectionId,sectionButton)
                    ToolTip.visible: hovered; ToolTip.delay: Theme.tooltipDelay; ToolTip.text: qsTr("Section controls")
                    Accessible.name: qsTr("Section controls: %1").arg(row.modelData.sectionName)
                }
            }

            // Slide number, beside the card in the list and under it in the sorter.
            Label {
                visible: !root.sorter && !row.modelData.collapsed
                x: 0; y: card.y + Theme.s1
                width: root.numberWidth; horizontalAlignment: Text.AlignRight
                text: row.slide + 1
                color: row.selected ? Theme.accent : Theme.textMuted
                font.pixelSize: Theme.fsControl; font.weight: row.selected ? Theme.wHeading : Theme.wNormal
            }

            Rectangle {
                id: card
                x: root.sorter ? Theme.s3 : root.numberWidth + Theme.s2
                y: row.headerHeight
                width: root.thumbWidth
                height: row.modelData.collapsed ? Theme.hRow
                      : root.sorter || root.mode === 0 ? root.thumbHeight : root.bodyHeight(row.modelData)
                radius: Theme.rControl
                color: root.mode === 0 || root.sorter ? Theme.windowBg
                     : row.selected ? Theme.withAlpha(Theme.accent, 0.1) : "transparent"
                border.width: row.selected ? Theme.selectionRing : Theme.hairline
                border.color: row.selected ? Theme.accent : (root.mode === 0 || root.sorter ? Theme.border : "transparent")
                opacity: root.dragFrom >= 0 && row.selected ? .5 : 1
                clip: true

                Image {
                    id: thumb
                    visible: !row.modelData.collapsed && (root.sorter || root.mode === 0)
                    anchors.fill: parent; anchors.margins: row.selected ? Theme.selectionRing : Theme.hairline
                    fillMode: Image.PreserveAspectFit
                    // Keyed on the slide's own stamp, not the deck revision, so an
                    // edit elsewhere leaves this picture as it is.
                    source: "image://slides/"+row.slide+"/"+(row.modelData.stamp ?? ""); sourceSize.width: root.sorter ? 640 : 320
                    opacity: row.modelData.skipped ? .4 : 1
                    // The old picture stays up while the new one is drawn (Qt 6.8+).
                    Component.onCompleted: if ("retainWhileLoading" in thumb) thumb.retainWhileLoading = true
                }
                // What the thumbnail cannot show: skipped, builds, notes, media,
                // comments, how the show arrives, and anything undescribed.
                Row {
                    visible: !row.modelData.collapsed && (root.sorter || root.mode === 0)
                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: Theme.s1 + 2
                    spacing: Theme.s1
                    Repeater {
                        model: [row.modelData.buildCount ? "sparkles" : "", row.modelData.notes ? "notebook-pen" : "", row.modelData.mediaCount ? "clapperboard" : "", row.modelData.commentCount ? "message-square-text" : "", row.modelData.transitionOwn ? "arrow-left-right" : "", row.modelData.undescribed ? "triangle-alert" : ""].filter(x => x.length)
                        Rectangle {
                            required property string modelData
                            objectName: "slideBadge_" + modelData
                            width: Theme.szIcon + Theme.s1; height: width; radius: Theme.rHandle
                            color: Theme.withAlpha(Theme.showBg, 0.65)
                            HoverHandler { id: badgeHover }
                            ToolTip.visible: badgeHover.hovered; ToolTip.delay: Theme.tooltipDelay
                            ToolTip.text: root.badgeTip(modelData)
                            Icon { anchors.centerIn: parent; name: parent.modelData; size: Theme.szIcon - 4
                                   color: parent.modelData === "triangle-alert" ? Theme.warning : Theme.textPrimary }
                        }
                    }
                }
                Rectangle {
                    visible: row.modelData.skipped && !row.modelData.collapsed && (root.sorter || root.mode === 0)
                    anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.s1 + 2
                    width: Theme.szIcon + Theme.s1; height: width; radius: Theme.rHandle
                    color: Theme.withAlpha(Theme.showBg, 0.65)
                    Icon { anchors.centerIn: parent; name: "eye-off"; size: Theme.szIcon - 4; color: Theme.danger }
                }
                // Compact and outline rows: the title, then the text.
                ColumnLayout {
                    visible: !row.modelData.collapsed && !root.sorter && root.mode !== 0
                    anchors.fill: parent; anchors.leftMargin: Theme.s2; anchors.rightMargin: Theme.s2
                    spacing: 0
                    Label { Layout.fillWidth: true; Layout.preferredHeight: Theme.hRow; verticalAlignment: Text.AlignVCenter
                            text: row.modelData.title || qsTr("Untitled"); elide: Text.ElideRight
                            color: row.selected ? Theme.accent : row.modelData.skipped ? Theme.textMuted : Theme.textPrimary }
                    Label { visible: root.mode === 2; Layout.fillWidth: true; Layout.fillHeight: true; text: row.modelData.outline || qsTr("No text"); wrapMode: Text.Wrap; maximumLineCount: 4; elide: Text.ElideRight; color: Theme.textSecondary; font.pixelSize: Theme.fsLabel }
                }
                Label {
                    visible: row.modelData.collapsed
                    anchors.fill: parent; anchors.leftMargin: Theme.s2
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("%1 slides · %2 selected").arg(row.modelData.sectionCount).arg(row.selectedCount)
                    color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                }

                MouseArea {
                    id: dragArea
                    objectName: (root.sorter ? "sorterDrag" : "slideDrag") + row.slide
                    anchors.fill: parent; preventStealing: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    property point grab
                    property bool collapseOnRelease: false
                    onPressed: mouse => {
                        root.focusBrowser()
                        grab = Qt.point(mouse.x,mouse.y)
                        const ctrl = (mouse.modifiers & Qt.ControlModifier) !== 0
                        const shift = (mouse.modifiers & Qt.ShiftModifier) !== 0
                        collapseOnRelease = row.selected && !ctrl && !shift && mouse.button === Qt.LeftButton
                        if(!row.selected || ctrl || shift) root.choose(row.modelData,ctrl,shift)
                        if(mouse.button === Qt.RightButton) actionsMenu.popup(dragArea,mouse.x,mouse.y)
                    }
                    onPositionChanged: mouse => {
                        if(!pressed || !(pressedButtons & Qt.LeftButton)) return
                        if(root.dragFrom < 0 && Math.hypot(mouse.x-grab.x,mouse.y-grab.y) > Theme.s2) root.dragFrom = row.slide
                        if(root.dragFrom < 0) return
                        const point = mapToItem(root.view,mouse.x,mouse.y)
                        const at = root.view.indexAt(point.x+root.view.contentX,point.y+root.view.contentY)
                        const target = root.rows[at]
                        root.dropIndex = target ? target.index : -1
                        root.dropBoundarySlide = target ? (target.collapsed ? (root.dragFrom < target.index ? target.sectionLast : target.sectionFirst) : target.index) : -1
                        if(point.y < Theme.s5) root.view.contentY = Math.max(0,root.view.contentY-Theme.s2)
                        if(point.y > root.view.height-Theme.s5) root.view.contentY = Math.min(Math.max(0,root.view.contentHeight-root.view.height),root.view.contentY+Theme.s2)
                    }
                    onReleased: {
                        const from=root.dragFrom, to=root.dropIndex, boundary=root.dropBoundarySlide
                        root.dragFrom=-1; root.dropIndex=-1; root.dropBoundarySlide=-1
                        if(from >= 0 && to >= 0 && from !== to) backend.moveSelectedSlides(boundary,from<to)
                        else if(collapseOnRelease) root.choose(row.modelData,false,false)
                    }
                    onDoubleClicked: { if(row.modelData.collapsed) root.toggleSection(row.modelData.sectionId); else root.editRequested() }
                    onCanceled: { root.dragFrom=-1; root.dropIndex=-1; root.dropBoundarySlide=-1 }
                }
            }
            // The sorter shows each slide's number and title under it.
            Label {
                visible: root.sorter && !row.modelData.collapsed
                x: card.x; y: card.y + card.height; width: card.width; height: Theme.hRow
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                text: (row.slide + 1) + "   " + String(row.modelData.title || qsTr("Untitled")).replace(/\s+/g, " ")
                color: row.selected ? Theme.accent : Theme.textSecondary; font.pixelSize: Theme.fsLabel
            }
            Rectangle { x: card.x; y: root.dragFrom < row.slide ? card.y+card.height+Theme.s1 : card.y-Theme.s1-Theme.selectionRing; width: card.width; height: Theme.selectionRing; color: Theme.accent; visible: root.dragFrom >= 0 && root.dropIndex === row.slide }
        }
    }
    Sheet {
        id: sectionName
        objectName: root.sorter ? "sorterSectionNameDialog" : "sectionNameDialog"
        parent: Overlay.overlay; anchors.centerIn: parent; modal: true
        background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
        Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
        title: editingId ? qsTr("Rename section") : qsTr("Start section")
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string editingId: ""
        function ask(id) { editingId=id; input.text=id ? backend.sectionInfo(id).name : ""; open(); input.forceActiveFocus(); input.selectAll() }
        onOpened: standardButton(Dialog.Ok).enabled = Qt.binding(() => input.text.trim().length > 0)
        TextField { id: input; objectName: "sectionNameInput"; width: Theme.wInspector; placeholderText: qsTr("Section name"); onAccepted: if(text.trim().length) sectionName.accept() }
        onAccepted: { if(editingId) backend.renameSection(editingId,input.text); else backend.startSection(input.text) }
    }
}

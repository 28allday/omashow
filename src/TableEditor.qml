import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0
Sheet {
    id: root
    objectName: "tableEditor"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width-Theme.s5*2,1120)
    height: Math.min(parent.height-Theme.s4*2,800)
    modal: true; title: root.info.chart ? qsTr("Edit chart data") : qsTr("Edit table")
    standardButtons: Dialog.Close
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
    readonly property var tableModel: backend.tableModel
    readonly property var cell: tableModel.selection
    readonly property var info: tableModel.summary
    property string editedId: ""
    property string baseText: ""
    property string tableId: ""
    property bool committing: false
    property string message: ""
    function show() { tableId=backend.selectedId; message=""; loadCell(); open(); grid.forceActiveFocus() }
    function loadCell() { editedId=cell.id ?? ""; baseText=cell.text ?? ""; editor.text=baseText }
    function commitCell() {
        if (!visible || committing || editedId === "" || editor.text === baseText) return true
        committing=true
        const result=tableModel.commitText(editedId,editor.text,baseText)
        committing=false
        if (!result) { message=qsTr("The cell changed elsewhere or the text is too large. Copy your edit before selecting another cell."); return false }
        baseText=editor.text
        return true
    }
    function selectCell(row,column,extend) { if (!commitCell()) return; tableModel.selectCell(row,column,extend); loadCell(); grid.forceActiveFocus() }
    function move(horizontal,vertical,extend) {
        if (!commitCell()) return
        tableModel.moveCell(horizontal,vertical,extend); loadCell()
        grid.positionViewAtCell(Qt.point(cell.column ?? 0,cell.row ?? 0),TableView.Contain)
    }
    function act(operation) { if(commitCell()) { message=operation() ? "" : qsTr("That change cannot be applied to this range."); loadCell() } }
    function format(key,value) { act(() => tableModel.formatCells(key,value)) }
    onAboutToHide: commitCell()
    onClosed: { pasteDialog.close(); sortDialog.close(); root.tableModel.cancelDataFile(); editedId="" }
    Connections {
        target: backend
        function onSelectionChanged() {
            if (root.visible && (backend.selectedId !== root.tableId || (backend.selection.type !== "table" && backend.selection.type !== "chart"))) root.close()
        }
    }
    Connections {
        target: root.tableModel
        function onDataFileReady(text,name) { if(root.visible) pasteDialog.showText(text,name) }
        function onDataFileFailed(message) { root.message=message }
        function onSelectionChanged() {
            if(pasteDialog.visible) pasteDialog.update()
            if(sortDialog.visible) sortDialog.update()
            if(root.visible && !root.committing && editor.text === root.baseText) root.loadCell()
        }
    }
    contentItem: RowLayout {
        spacing: Theme.s4
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: Theme.s2
            RowLayout {
                Label { textFormat: Text.PlainText; text: qsTr("%1 rows × %2 columns").arg(root.info.rows ?? 0).arg(root.info.columns ?? 0); color: Theme.textMuted }
                Item { Layout.fillWidth: true }
                Button { objectName: "tableCopyRange"; text: qsTr("Copy"); Accessible.name: qsTr("Copy table range"); onClicked: { if(root.commitCell()) root.tableModel.copyRange() } }
                Button { objectName: "tableImportData"; enabled: !root.info.loadingData; text: qsTr("Import…"); Accessible.name: qsTr("Import CSV or TSV data"); onClicked: if(root.commitCell()) root.tableModel.importDataDialog() }
                Button { objectName: "tablePasteRange"; text: qsTr("Paste…"); Accessible.name: qsTr("Paste table data"); onClicked: if(root.commitCell()) pasteDialog.show() }
            }
            ColumnLayout {
                Layout.fillWidth: true; visible: (root.info.sourcePath ?? "").length>0 || root.info.loadingData
                spacing: 0
                Label { objectName: "tableSourcePath"; textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; text: root.info.sourcePath ?? ""; color: Theme.textMuted; font.pixelSize: Theme.fsCaption }
                RowLayout {
                    Layout.fillWidth: true
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.info.loadingData ? qsTr("Reading file…") : root.info.sourceEdited ? qsTr("Local data edits. Refresh will replace them after preview.") : qsTr("Saved data is available offline. Refresh reads this file only when requested."); color: root.info.sourceEdited ? Theme.accent : Theme.textMuted; font.pixelSize: Theme.fsCaption }
                    Button { objectName: "tableRefreshData"; text: qsTr("Refresh…"); visible: !root.info.loadingData; onClicked: if(root.commitCell()) root.tableModel.refreshDataFile() }
                    Button { objectName: "tableDisconnectData"; text: qsTr("Disconnect"); visible: !root.info.loadingData; onClicked: root.act(() => root.tableModel.disconnectDataFile()) }
                    Button { objectName: "tableCancelData"; text: qsTr("Cancel read"); visible: root.info.loadingData; onClicked: root.tableModel.cancelDataFile() }
                }
            }
            TableAccessibility {
                objectName: "tableAccessible"; model: root.tableModel; view: grid
                Accessible.role: Accessible.Table
                onSelectRequested: (row,column,extend) => { root.selectCell(row,column,extend); grid.positionViewAtCell(Qt.point(column,row),TableView.Contain) }
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 130
                Rectangle { anchors.fill: parent; color: Theme.controlBg; border.color: Theme.border }
                HorizontalHeaderView {
                    id: columnHeader; anchors.top: parent.top; anchors.left: rowHeader.right; anchors.right: parent.right
                    syncView: grid; clip: true
                    delegate: Rectangle {
                        required property var display
                        implicitWidth: 130; implicitHeight: Theme.hControl
                        color: Theme.panelRaised; border.color: Theme.border
                        Label { textFormat: Text.PlainText; anchors.centerIn: parent; text: display; color: Theme.textMuted; font.family: Theme.monoFamily }
                    }
                }
                VerticalHeaderView {
                    id: rowHeader; anchors.top: columnHeader.bottom; anchors.left: parent.left; anchors.bottom: parent.bottom
                    syncView: grid; clip: true
                    delegate: Rectangle {
                        required property var display
                        implicitWidth: 38; implicitHeight: 58
                        color: Theme.panelRaised; border.color: Theme.border
                        Label { textFormat: Text.PlainText; anchors.centerIn: parent; text: display; color: Theme.textMuted; font.family: Theme.monoFamily }
                    }
                }
                TableView {
                    id: grid; objectName: "tableGrid"
                    anchors.top: columnHeader.bottom; anchors.left: rowHeader.right; anchors.right: parent.right; anchors.bottom: parent.bottom
                    clip: true; model: root.tableModel; keyNavigationEnabled: false; pointerNavigationEnabled: false
                    columnSpacing: 1; rowSpacing: 1; reuseItems: true
                    columnWidthProvider: () => Math.max(130,(width-2)/Math.min(5,Math.max(1,columns)))
                    rowHeightProvider: () => 58
                    Accessible.ignored: true
                    Accessible.role: Accessible.Table; Accessible.name: qsTr("Table cells, %1 rows and %2 columns").arg(rows).arg(columns)
                    Accessible.description: qsTr("Arrows move between cells. Shift selects a range. Enter edits. Tab advances. Control V previews a pasted range.")
                    ScrollBar.vertical: ScrollBar {}
                    ScrollBar.horizontal: ScrollBar {}
                    Keys.onPressed: event => {
                        const shift=(event.modifiers & Qt.ShiftModifier)!==0, control=(event.modifiers & Qt.ControlModifier)!==0
                        if(control && event.key===Qt.Key_Z) {if(shift) backend.redo();else backend.undo();root.loadCell();event.accepted=true}
                        else if(control && event.key===Qt.Key_A) {root.tableModel.selectCell(0,0);root.tableModel.selectCell(rows-1,columns-1,true);root.loadCell();event.accepted=true}
                        else if(control && event.key===Qt.Key_X) {root.tableModel.copyRange();root.act(() => root.tableModel.clearRange());event.accepted=true}
                        else if(event.key===Qt.Key_Delete || event.key===Qt.Key_Backspace) {root.act(() => root.tableModel.clearRange());event.accepted=true}
                        else if(control && event.key===Qt.Key_C) {root.tableModel.copyRange();event.accepted=true}
                        else if(control && event.key===Qt.Key_V) {pasteDialog.show();event.accepted=true}
                        else if(event.key===Qt.Key_Left) {root.move(-1,0,shift);event.accepted=true}
                        else if(event.key===Qt.Key_Right) {root.move(1,0,shift);event.accepted=true}
                        else if(event.key===Qt.Key_Up) {root.move(0,-1,shift);event.accepted=true}
                        else if(event.key===Qt.Key_Down) {root.move(0,1,shift);event.accepted=true}
                        else if(event.key===Qt.Key_Tab || event.key===Qt.Key_Backtab) {root.move(shift?-1:1,0,false);event.accepted=true}
                        else if(event.key===Qt.Key_Return || event.key===Qt.Key_Enter || event.key===Qt.Key_F2) {editor.forceActiveFocus();editor.selectAll();event.accepted=true}
                        else if(!control && event.text.length>0) {editor.forceActiveFocus();editor.text=event.text;editor.cursorPosition=editor.length;event.accepted=true}
                    }
                    delegate: Rectangle {
                        required property int row
                        required property int column
                        required property string cellText
                        required property string cellAddress
                        required property string coveredBy
                        required property int spanRows
                        required property int spanColumns
                        required property bool isHeader
                        required property bool inSelection
                        objectName: "tableCell"+row+"_"+column
                        implicitWidth: 130; implicitHeight: 58
                        color: inSelection ? Theme.withAlpha(Theme.accent,.18) : isHeader ? Theme.panelRaised : Theme.controlBg
                        border.width: inSelection ? Theme.focusRing : 0; border.color: Theme.accent
                        Accessible.ignored: true
                        Accessible.role: isHeader ? Accessible.ColumnHeader : Accessible.Cell
                        Accessible.name: cellAddress+", "+(coveredBy.length ? qsTr("merged with %1").arg(coveredBy) : cellText)
                        Accessible.description: spanRows>1 || spanColumns>1 ? qsTr("Spans %1 rows and %2 columns").arg(spanRows).arg(spanColumns) : ""
                        Accessible.focusable: true; Accessible.focused: grid.activeFocus && row===(root.cell.row ?? 0) && column===(root.cell.column ?? 0)
                        Accessible.selected: inSelection
                        Accessible.onPressAction: root.selectCell(row,column,false)
                        Label { textFormat: Text.PlainText;
                            anchors.fill: parent; anchors.margins: Theme.s2; verticalAlignment: Text.AlignVCenter
                            text: coveredBy.length ? "↖ "+coveredBy : cellText.length ? cellText : cellAddress
                            color: coveredBy.length || !cellText.length ? Theme.textMuted : Theme.textPrimary
                            wrapMode: Text.Wrap; elide: Text.ElideRight; maximumLineCount: 2
                            font.pixelSize: Theme.fsControl
                        }
                        TapHandler { id: cellTap; onTapped: root.selectCell(row,column,(cellTap.point.modifiers & Qt.ShiftModifier)!==0); onDoubleTapped: {root.selectCell(row,column,false);editor.forceActiveFocus();editor.selectAll()} }
                    }
                }
            }
            RowLayout {
                Label { textFormat: Text.PlainText; text: (root.cell.address ?? "")+" · "+qsTr("Cell text"); color: Theme.textPrimary }
                Item { Layout.fillWidth: true }
                Label { textFormat: Text.PlainText; text: qsTr("Tab: next · Enter: below · Ctrl+Enter: new line"); color: Theme.textMuted; font.pixelSize: Theme.fsCaption }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.preferredHeight: 88
                TextArea {
                    id: editor; objectName: "tableCellEditor"; textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap; selectByMouse: true
                    Accessible.name: qsTr("Cell %1 text").arg(root.cell.address ?? "")
                    onActiveFocusChanged: if(!activeFocus) root.commitCell()
                    Keys.onPressed: event => {
                        if(event.key===Qt.Key_Tab || event.key===Qt.Key_Backtab) {root.move(event.modifiers & Qt.ShiftModifier?-1:1,0,false);event.accepted=true}
                        else if((event.key===Qt.Key_Return || event.key===Qt.Key_Enter) && !(event.modifiers & Qt.ControlModifier)) {root.move(0,event.modifiers & Qt.ShiftModifier?-1:1,false);event.accepted=true}
                        else if((event.key===Qt.Key_Return || event.key===Qt.Key_Enter) && event.modifiers & Qt.ControlModifier) {insert(cursorPosition,"\n");event.accepted=true}
                        else if(event.key===Qt.Key_Escape) {root.loadCell();grid.forceActiveFocus();event.accepted=true}
                    }
                }
            }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; visible: root.message.length>0; text: root.message; color: Theme.accent }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.info.chart ? qsTr("Values use the chart's number locale. Empty cells make gaps; formulas never run.") : root.height>=620 ? qsTr("Shift-click selects a range. Merged text stays in its first cell. The preview shows the slide appearance.") : qsTr("Shift-click selects a range. Tables stay on one slide; overflowing cells show a warning."); color: Theme.textMuted; font.pixelSize: Theme.fsCaption }
            Image {
                visible: root.height>=620
                Layout.fillWidth: true; Layout.preferredHeight: Math.min(140,root.height*.19)
                source: root.visible ? "image://slides/table/"+backend.currentSlide+"/"+root.tableId+"/"+backend.revision : ""
                sourceSize.width: 800; fillMode: Image.PreserveAspectFit; cache: false
                Accessible.name: qsTr("Table appearance preview")
            }
        }
        ScrollView {
            id: formatScroll; objectName: "tableFormatScroll"
            Layout.preferredWidth: 268; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: formatScroll.availableWidth; spacing: Theme.s2
                SectionLabel { text: qsTr("RANGE %1").arg(root.cell.range ?? "") }
                RowLayout {
                    visible: !root.info.chart
                    Button { objectName: "tableMerge"; text: qsTr("Merge"); onClicked: root.act(() => root.tableModel.mergeCells()) }
                    Button { objectName: "tableSplit"; text: qsTr("Split"); enabled: root.cell.merged ?? false; onClicked: root.act(() => root.tableModel.splitCell()) }
                }
                SectionLabel { text: qsTr("ROWS AND COLUMNS") }
                RowLayout {
                    Button { objectName: "tableAddRow"; text: qsTr("+ Row"); onClicked: root.act(() => root.tableModel.changeAxis(true,(root.cell.row ?? 0)+1,false)) }
                    Button { objectName: "tableDeleteRow"; text: qsTr("− Row"); enabled: (root.info.rows ?? 0)>(root.info.chart?2:1) && (!root.info.chart || (root.cell.row ?? 0)>0); onClicked: root.act(() => root.tableModel.changeAxis(true,root.cell.row ?? 0,true)) }
                }
                RowLayout {
                    Button { objectName: "tableAddColumn"; text: qsTr("+ Column"); onClicked: root.act(() => root.tableModel.changeAxis(false,(root.cell.column ?? 0)+1,false)) }
                    Button { objectName: "tableDeleteColumn"; text: qsTr("− Column"); enabled: (root.info.columns ?? 0)>(root.info.chart?2:1) && (!root.info.chart || (root.cell.column ?? 0)>0); onClicked: root.act(() => root.tableModel.changeAxis(false,root.cell.column ?? 0,true)) }
                }
                RowLayout {
                    Button { text: qsTr("Row ↑"); enabled: (root.cell.row ?? 0)>(root.info.chart?1:0); onClicked: root.act(() => root.tableModel.moveAxis(true,root.cell.row,root.cell.row-1)) }
                    Button { text: qsTr("Row ↓"); enabled: (root.cell.row ?? 0)>=(root.info.chart?1:0) && root.cell.row<root.info.rows-1; onClicked: root.act(() => root.tableModel.moveAxis(true,root.cell.row,root.cell.row+1)) }
                }
                RowLayout {
                    Button { text: qsTr("Column ←"); enabled: (root.cell.column ?? 0)>(root.info.chart?1:0); onClicked: root.act(() => root.tableModel.moveAxis(false,root.cell.column,root.cell.column-1)) }
                    Button { text: qsTr("Column →"); enabled: (root.cell.column ?? 0)>=(root.info.chart?1:0) && root.cell.column<root.info.columns-1; onClicked: root.act(() => root.tableModel.moveAxis(false,root.cell.column,root.cell.column+1)) }
                }
                NumField { visible: !root.info.chart; Layout.fillWidth: true; label: qsTr("Row weight"); value: root.cell.rowSize ?? 1; step: .1; onCommitted: value => root.act(() => root.tableModel.setAxisSize(true,root.cell.row ?? 0,value)) }
                NumField { visible: !root.info.chart; Layout.fillWidth: true; label: qsTr("Column weight"); value: root.cell.columnSize ?? 1; step: .1; onCommitted: value => root.act(() => root.tableModel.setAxisSize(false,root.cell.column ?? 0,value)) }
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.info.chart ? qsTr("First row: series names. First column: categories, or X values for scatter charts. Empty values make gaps; invalid numbers are reported below.") : qsTr("Weights share the table's total width and height. New rows and columns fit inside its frame."); color: Theme.textMuted; font.pixelSize: Theme.fsCaption }
                RowLayout {
                    visible: !root.info.chart
                    Button { text: qsTr("Equal rows"); onClicked: root.act(() => root.tableModel.distribute(true)) }
                    Button { text: qsTr("Equal columns"); onClicked: root.act(() => root.tableModel.distribute(false)) }
                }
                CheckBox { visible: !root.info.chart; objectName: "tableHeaderRow"; text: qsTr("Header row"); checked: (root.info.headerRows ?? 0)>0; onClicked: root.act(() => root.tableModel.setTableOption("headerRows",checked)) }
                CheckBox { visible: !root.info.chart; text: qsTr("Header column"); checked: (root.info.headerColumns ?? 0)>0; onClicked: root.act(() => root.tableModel.setTableOption("headerColumns",checked)) }
                CheckBox { visible: !root.info.chart; objectName: "tableBanding"; text: qsTr("Alternating rows"); checked: root.info.banded ?? false; onClicked: root.act(() => root.tableModel.setTableOption("banded",checked)) }
                Button { objectName: "tableSortRows"; text: qsTr("Sort rows…"); onClicked: if(root.commitCell()) sortDialog.show() }
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: root.info.chart ?? false; wrapMode: Text.Wrap; text: (root.info.chartIssues ?? []).concat(root.info.chartWarnings ?? []).join("\n"); color: Theme.accent }
                ColumnLayout { visible: !root.info.chart; Layout.fillWidth: true
                SectionLabel { text: qsTr("CELL FORMAT") }
                TextField { Layout.fillWidth: true; text: root.cell.fontFamily ?? ""; Accessible.name: qsTr("Cell font family"); onEditingFinished: root.format("fontFamily",text) }
                NumField { objectName: "tableFontSize"; Layout.fillWidth: true; label: qsTr("Size"); value: root.cell.fontSize ?? 20; onCommitted: value => root.format("fontSize",value) }
                RowLayout {
                    Button { objectName: "tableBold"; text: qsTr("Bold"); checkable: true; checked: (root.cell.fontWeight ?? 400)>=600; onClicked: root.format("fontWeight",checked?700:400) }
                    Button { text: qsTr("Italic"); checkable: true; checked: root.cell.italic ?? false; onClicked: root.format("italic",checked) }
                }
                ComboBox { Layout.fillWidth: true; model: [qsTr("Left"),qsTr("Centre"),qsTr("Right"),qsTr("Justify")]; currentIndex: root.cell.textAlign ?? 0; Accessible.name: qsTr("Cell horizontal alignment"); onActivated: root.format("textAlign",currentIndex) }
                ComboBox { Layout.fillWidth: true; model: [qsTr("Top"),qsTr("Middle"),qsTr("Bottom")]; currentIndex: root.cell.verticalAlign ?? 1; Accessible.name: qsTr("Cell vertical alignment"); onActivated: root.format("verticalAlign",currentIndex) }
                ComboBox { Layout.fillWidth: true; model: [qsTr("Clip with overflow warning"),qsTr("Shrink text to fit")]; currentIndex: root.cell.textFit ?? 0; Accessible.name: qsTr("Cell text fit"); onActivated: root.format("textFit",currentIndex) }
                Label { textFormat: Text.PlainText; text: qsTr("Text colour"); color: Theme.textMuted }
                TextField { Layout.fillWidth: true; text: root.cell.textColor ?? ""; Accessible.name: qsTr("Cell text colour"); onEditingFinished: root.format("textColor",text) }
                Label { textFormat: Text.PlainText; text: qsTr("Fill colour"); color: Theme.textMuted }
                TextField { Layout.fillWidth: true; text: root.cell.fill ?? ""; Accessible.name: qsTr("Cell fill colour"); onEditingFinished: root.format("fill",text) }
                Button { text: qsTr("Use table fill"); onClicked: root.format("fill",null) }
                NumField { Layout.fillWidth: true; label: qsTr("Padding"); value: root.cell.padding ?? 12; onCommitted: value => root.format("padding",value) }
                NumField { Layout.fillWidth: true; label: qsTr("Border"); value: root.cell.borderWidth ?? 1; step: .5; onCommitted: value => root.format("borderWidth",value) }
                TextField { Layout.fillWidth: true; text: root.cell.borderColor ?? ""; Accessible.name: qsTr("Cell border colour"); onEditingFinished: root.format("borderColor",text) }
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; visible: (root.info.overflow ?? 0)>0; text: qsTr("%1 cells overflow. Increase their space, reduce padding/font size, or use Shrink text to fit.").arg(root.info.overflow ?? 0); color: Theme.accent }
                }
            }
        }
    }
    Sheet {
        id: pasteDialog; objectName: "tablePasteDialog"
        parent: Overlay.overlay; anchors.centerIn: parent; width: Math.min(parent.width-Theme.s5*2,780); height: Math.min(parent.height-Theme.s5*2,610)
        Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
        modal: true; title: qsTr("Paste table data")
        property var preview: ({})
        property bool filePreview: false
        property string originalText: ""
        function update() { preview=filePreview ? root.tableModel.previewDataFile(pasteSource.text,[0,9,44,59][delimiter.currentIndex],linkFile.checked) : root.tableModel.previewPaste(pasteSource.text,[0,9,44,59][delimiter.currentIndex]) }
        function showText(text,name) {
            filePreview=name.length>0; originalText=text
            linkFile.checked=filePreview && root.info.candidateRefresh
            title=linkFile.checked ? qsTr("Refresh %1").arg(name) : name.length ? qsTr("Import %1").arg(name) : root.info.chart ? qsTr("Paste chart data") : qsTr("Paste table data")
            delimiter.currentIndex=linkFile.checked ? Math.max(0,[0,9,44,59].indexOf(root.info.sourceDelimiter)) : 0
            pasteSource.text=text; update(); open()
        }
        function show() { showText(root.tableModel.clipboardText(),"") }
        standardButtons: Dialog.Apply | Dialog.Cancel
        onOpened: standardButton(Dialog.Apply).enabled=preview.ok ?? false
        onPreviewChanged: if(visible) standardButton(Dialog.Apply).enabled=preview.ok ?? false
        onApplied: {root.act(() => filePreview ? root.tableModel.applyDataFile(pasteSource.text,[0,9,44,59][delimiter.currentIndex],linkFile.checked) : root.tableModel.pasteText(pasteSource.text,[0,9,44,59][delimiter.currentIndex])); close()}
        onClosed: {root.tableModel.cancelDataFile();linkFile.checked=false}
        background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
        contentItem: ColumnLayout {
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: pasteDialog.filePreview; text: root.info.candidatePath ?? ""; wrapMode: Text.WrapAnywhere; color: Theme.textMuted; font.pixelSize: Theme.fsCaption }
            CheckBox { id: linkFile; objectName: "tableLinkFile"; visible: pasteDialog.filePreview; text: qsTr("Link to this file for manual refresh"); enabled: !root.info.candidateRefresh && pasteSource.text===pasteDialog.originalText; onToggled: pasteDialog.update() }
            ComboBox { id: delimiter; Layout.fillWidth: true; model: [qsTr("Detect delimiter"),qsTr("Tab (TSV)"),qsTr("Comma (CSV)"),qsTr("Semicolon")]; onActivated: pasteDialog.update() }
            ScrollView { Layout.fillWidth: true; Layout.fillHeight: true; TextArea { id: pasteSource; objectName: "tablePasteSource"; readOnly: linkFile.checked; textFormat: TextEdit.PlainText; selectByMouse: true; wrapMode: TextEdit.NoWrap; Accessible.name: qsTr("Delimited table data"); onTextChanged: pasteDialog.update() } }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: pasteDialog.preview.error || (linkFile.checked ? qsTr("Replace the entire grid: %1 × %2 → %3 × %4. %5 new or changed cells. Removed rows and columns are discarded. One undo restores them.").arg(pasteDialog.preview.oldRows ?? 0).arg(pasteDialog.preview.oldColumns ?? 0).arg(pasteDialog.preview.rows ?? 0).arg(pasteDialog.preview.columns ?? 0).arg(pasteDialog.preview.changed ?? 0) : qsTr("%1 rows × %2 columns starting at %3. The table expands to fit.").arg(pasteDialog.preview.rows ?? 0).arg(pasteDialog.preview.columns ?? 0).arg(root.cell.address ?? "")); color: pasteDialog.preview.ok ? Theme.textPrimary : Theme.accent }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: linkFile.checked; wrapMode: Text.Wrap; text: (root.info.sourceEdited ? qsTr("Local data edits will be replaced. ") : "") + (root.info.chart ? qsTr("Unique series and category names retain their colours when reordered.") : qsTr("Existing cell formatting is retained by position.")) + " " + qsTr("Opening the deck uses saved data; it never refreshes automatically."); color: Theme.accent; font.pixelSize: Theme.fsCaption }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: (pasteDialog.preview.preview ?? []).join("\n"); color: Theme.textMuted; font.family: Theme.monoFamily; elide: Text.ElideRight; maximumLineCount: 6 }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("Values stay as written, including decimal separators and leading zeros. Formulas are stored as literal text and never run. Pasting replaces destination text in one undo step."); color: Theme.textMuted; font.pixelSize: Theme.fsCaption }
        }
    }
    Sheet {
        id: sortDialog; objectName: "tableSortDialog"
        parent: Overlay.overlay; anchors.centerIn: parent
        width: Math.min(parent.width-Theme.s5*2,640); height: Math.min(parent.height-Theme.s5*2,580)
        Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg,.65) }
        modal: true; title: qsTr("Sort table rows"); standardButtons: Dialog.Apply | Dialog.Cancel
        property var preview: ({})
        function update() { preview=root.tableModel.previewSort(sortColumn.currentIndex,sortOrder.currentIndex===0,sortType.currentIndex===1,["en_GB","de_DE","fr_FR"][sortLocale.currentIndex]) }
        function show() { sortColumn.currentIndex=root.cell.column ?? 0; update(); open() }
        onOpened: standardButton(Dialog.Apply).enabled=preview.ok ?? false
        onPreviewChanged: if(visible) standardButton(Dialog.Apply).enabled=preview.ok ?? false
        onApplied: {root.act(() => root.tableModel.sortRows(sortColumn.currentIndex,sortOrder.currentIndex===0,sortType.currentIndex===1,["en_GB","de_DE","fr_FR"][sortLocale.currentIndex]));close()}
        background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
        contentItem: ColumnLayout {
            ComboBox { id: sortColumn; objectName: "tableSortColumn"; Layout.fillWidth: true; model: Array.from({length: root.info.columns ?? 0},(_,i) => qsTr("Column %1").arg(i+1)); Accessible.name: qsTr("Sort column"); onActivated: sortDialog.update() }
            RowLayout {
                ComboBox { id: sortOrder; objectName: "tableSortOrder"; Layout.fillWidth: true; model: [qsTr("Ascending"),qsTr("Descending")]; Accessible.name: qsTr("Sort order"); onActivated: sortDialog.update() }
                ComboBox { id: sortType; objectName: "tableSortType"; Layout.fillWidth: true; model: [qsTr("Text"),qsTr("Numbers")]; Accessible.name: qsTr("Sort value type"); onActivated: sortDialog.update() }
            }
            ComboBox { id: sortLocale; Layout.fillWidth: true; model: [qsTr("English (UK) · 1,234.56"),qsTr("German · 1.234,56"),qsTr("French · 1 234,56")]; Accessible.name: qsTr("Sort locale"); onActivated: sortDialog.update() }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: sortDialog.preview.error || qsTr("Preview · %1 rows").arg(sortDialog.preview.rows ?? 0); color: sortDialog.preview.ok ? Theme.textPrimary : Theme.accent }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; Layout.fillHeight: true; wrapMode: Text.Wrap; text: (sortDialog.preview.preview ?? []).join("\n"); color: Theme.textMuted; font.family: Theme.monoFamily }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("The header row stays first. Entire rows move together, including their formatting and height. Empty values stay last; equal values keep their order. Nothing changes until Apply."); color: Theme.textMuted }
        }
    }

}

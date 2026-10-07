import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Style / Text / Arrange for the selected object. Every control here is backed
// by a real property on a real object: with nothing selected the panel shows
// the slide's own settings rather than dead fields.
Rectangle {
    id: root
    objectName: "inspector"
    color: Theme.panelBg

    property bool stylesExpanded: false
    signal applyLayoutRequested(string layoutId)
    readonly property var sel: backend.selection
    // While text is selected on the slide, formatting applies to that stretch.
    readonly property var range: backend.textSelection
    // Type sizes and other text measures are shown and typed in points, the
    // way PowerPoint shows them, and stored in slide units as before.
    readonly property real ptPerUnit: backend.ptPerUnit
    function style(key, on) {
        if (range.active) backend.formatSelection(key, on ? 1 : 2)
        else backend.setSelectedProperty(key, on)
    }
    readonly property int currentWeight: range.active ? (range.weight ?? 400) : (sel.fontWeight ?? 400)
    // Asked once per family change; the backend caches each family's faces.
    readonly property string weightFamily: (range.active ? range.fontFamily : sel.fontFamily) ?? Theme.fontFamily
    readonly property var familyWeights: isText ? backend.fontWeights(weightFamily) : []
    readonly property var weightChoices: {
        const list = familyWeights.slice()
        if (list.indexOf(currentWeight) < 0) list.push(currentWeight)
        return list.sort((a, b) => a - b)
    }
    function weightName(w) {
        const names = { 100: qsTr("Thin"), 200: qsTr("Extra light"), 300: qsTr("Light"), 400: qsTr("Regular"),
                        500: qsTr("Medium"), 600: qsTr("Semibold"), 700: qsTr("Bold"), 800: qsTr("Extra bold"),
                        900: qsTr("Black") }
        return names[w] ?? String(w)
    }
    readonly property bool isMedia: backend.hasSelection && sel.type === "media"
    readonly property bool isImage: backend.hasSelection && sel.type === "image"
    readonly property bool isText: backend.hasSelection && sel.type === "text"
    readonly property bool isConnector: backend.selectionCount === 1 && (sel.connector ?? false)
    // With nothing selected the map is empty, and the panel's bindings still
    // evaluate even though it is hidden — so the colour reads go through here
    // rather than handing QColor an undefined.
    readonly property color swatch: {
        const value = root.isText ? root.sel.textColor : root.sel.fill
        return value !== undefined ? value : Theme.controlBg
    }
    readonly property string swatchText: {
        const value = root.isText ? root.sel.textColor : root.sel.fill
        return value !== undefined ? value : ""
    }

    Connections {
        target: backend
        function onSelectionChanged() { if(tabs.currentIndex === 1 && backend.selection.type !== "text") tabs.currentIndex = 0 }
    }
    component Divider: Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: Theme.s1
        Layout.bottomMargin: Theme.s1
        implicitHeight: Theme.hairline
        color: Theme.border
    }
    // A square icon button for the align / distribute / order rows.
    component IconAction: Button {
        property string tip: ""
        display: AbstractButton.IconOnly
        implicitWidth: Theme.hControl + Theme.s1
        implicitHeight: Theme.hControl + Theme.s1
        ToolTip.visible: hovered && tip !== ""
        ToolTip.delay: Theme.tooltipDelay
        ToolTip.text: tip
        Accessible.name: tip
    }
    // B / I / U. The letter is the icon; the accessible name stays the word.
    component StyleMark: Button {
        property string tip: ""
        property bool mark: true
        checkable: true
        ToolTip.visible: hovered && tip !== ""
        ToolTip.delay: Theme.tooltipDelay
        ToolTip.text: tip
        Accessible.name: tip
    }
    // Bold is the same weight the Size row's weight menu writes: 700 on, 400
    // off. A selected stretch uses "weight"; the whole box uses "fontWeight".
    function bold(on) {
        if (range.active) backend.formatSelection("weight", on ? 700 : 400)
        else backend.setSelectedProperty("fontWeight", on ? 700 : 400)
    }

    Rectangle {
        anchors.left: parent.left
        width: Theme.hairline
        height: parent.height
        color: Theme.border
    }

    TabBar {
        id: tabs
        objectName: "inspectorTabs"
        onCurrentIndexChanged: if (inspectorScroll.contentItem) inspectorScroll.contentItem.contentY = 0
        anchors.left: parent.left
        anchors.leftMargin: Theme.hairline
        anchors.right: parent.right
        height: Theme.hTab
        visible: backend.hasSelection
        TabButton { text: qsTr("Style") }
        TabButton { text: qsTr("Text"); enabled: root.isText }
        TabButton { text: qsTr("Arrange") }
    }

    // ── Nothing selected: the slide itself ──────────────────────────────────
    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.s4
        spacing: Theme.s3
        visible: !backend.hasSelection
        Label { text: qsTr("Slide"); font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading; Layout.bottomMargin: Theme.s1 }
        FieldRow {
            label: qsTr("Layout")
            ComboBox {
                Layout.fillWidth: true
                visible: backend.design.layouts.length > 0
                model: backend.design.layouts
                textRole: "name"; valueRole: "id"
                currentIndex: model.findIndex(l => l.id === backend.slideDesign.layoutId)
                displayText: currentIndex < 0 ? qsTr("Freeform") : currentText
                onActivated: root.applyLayoutRequested(currentValue)
            }
            Button { visible: backend.design.layouts.length === 0; Layout.fillWidth: true; text: qsTr("Add layouts"); icon.name: "layout-template"; onClicked: backend.setupDesign() }
        }
        FieldRow {
            label: qsTr("Background")
            Rectangle {
                implicitWidth: Theme.hControl; implicitHeight: Theme.hControl; radius: Theme.rControl
                color: backend.slideDesign.background || Theme.controlBg
                border.color: Theme.borderStrong; border.width: Theme.hairline
            }
            TextField { Layout.fillWidth: true; text: backend.slideDesign.background ?? ""; font.family: Theme.monoFamily
                        placeholderText: qsTr("Master"); onEditingFinished: backend.setSlideBackground(text) }
        }
        FieldRow {
            label: ""
            Button { Layout.fillWidth: true; text: qsTr("Use master background"); icon.name: "rotate-ccw"
                     enabled: backend.slideDesign.backgroundOverride ?? false
                     onClicked: backend.setSlideBackground("", true) }
        }
        CheckBox {
            objectName: "showMasterArtwork"
            Layout.fillWidth: true
            text: qsTr("Show master artwork")
            enabled: backend.slideDesign.hasMaster ?? false
            checked: backend.slideDesign.showMasterObjects ?? true
            onToggled: backend.setMasterArtworkVisible(checked)
        }
        CheckBox {
            objectName: "showMasterFields"
            Layout.fillWidth: true; text: qsTr("Show numbers, date and footer")
            enabled: backend.slideDesign.hasMaster ?? false
            checked: backend.slideDesign.showMasterFields ?? true
            onToggled: backend.setMasterFieldsVisible(checked)
        }
        Divider {}
        RowLayout {
            spacing: Theme.s2
            Icon { name: "info"; color: Theme.textMuted }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    text: qsTr("Select an object to style it. The theme and slide size are under Document.") }
        }
    }

    ScrollView {
        id: inspectorScroll
        objectName: "inspectorScroll"
        anchors.top: tabs.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.s4
        anchors.rightMargin: Theme.s4
        anchors.topMargin: Theme.s3
        contentWidth: availableWidth
        clip: true
        visible: backend.hasSelection
        StackLayout {
            width: inspectorScroll.availableWidth
            currentIndex: tabs.currentIndex

        // ── Style ───────────────────────────────────────────────────────────
        ColumnLayout {
            spacing: Theme.s3

            RowLayout {
                visible: (root.sel.placeholderId ?? "") !== ""
                Layout.fillWidth: true
                spacing: Theme.s2
                Icon { name: "layout-template"; color: Theme.accent }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.accent
                    font.pixelSize: Theme.fsLabel
                    text: (root.sel.overrides ?? []).length > 0 ? qsTr("Layout placeholder · local edits") : qsTr("Inherited from layout")
                }
            }
            RowLayout {
                visible: (root.sel.placeholderId ?? "") !== ""
                Layout.fillWidth: true
                Button { Layout.fillWidth: true; text: qsTr("Reset style"); onClicked: backend.resetPlaceholder(false) }
                Button { Layout.fillWidth: true; text: qsTr("Reset position"); onClicked: backend.resetPlaceholder(true) }
            }
            ChartTools { Layout.fillWidth: true; visible: root.sel.type === "chart"; sel: root.sel }
            MediaTools { Layout.fillWidth: true; visible: root.isMedia; sel: root.sel }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; visible: root.sel.type === "table" && (root.sel.tableOverflow ?? 0)>0; text: qsTr("%1 cells overflow. Edit the table to adjust text fit, padding or row sizes.").arg(root.sel.tableOverflow ?? 0); color: Theme.warning }
            Button { objectName: "editTable"; Layout.fillWidth: true; visible: root.sel.type === "table"; icon.name: "table"; text: qsTr("Edit table cells…"); onClicked: backend.editSelectedTable() }

            Button {
                objectName: "toggleObjectStyles"
                Layout.fillWidth: true
                flat: true
                icon.name: root.stylesExpanded ? "chevron-down" : "chevron-right"
                text: qsTr("Saved styles") + " · " + backend.objectStyles.length
                onClicked: root.stylesExpanded = !root.stylesExpanded
            }
            ColumnLayout {
                visible: root.stylesExpanded; Layout.fillWidth: true
                spacing: Theme.s2
                ComboBox { id: savedStyle; objectName: "savedObjectStyles"; Layout.fillWidth: true; model: backend.objectStyles; textRole: "name"; valueRole: "id"; displayText: count?currentText:qsTr("No saved styles") }
                RowLayout {
                    Layout.fillWidth: true
                    Button { objectName: "applyObjectStyle"; Layout.fillWidth: true; text: qsTr("Apply"); enabled: savedStyle.count>0; onClicked: backend.applyObjectStyle(savedStyle.currentValue) }
                    Button { Layout.fillWidth: true; text: qsTr("Remove"); enabled: savedStyle.count>0; onClicked: backend.removeObjectStyle(savedStyle.currentValue) }
                }
                TextField { id: styleName; objectName: "objectStyleName"; Layout.fillWidth: true; placeholderText: qsTr("Name this appearance"); Accessible.name: qsTr("Object style name") }
                RowLayout {
                    Layout.fillWidth: true
                    Button { objectName: "saveObjectStyle"; Layout.fillWidth: true; text: qsTr("Save new style"); enabled: backend.selectionCount===1 && styleName.text.trim().length>0; onClicked: { backend.saveObjectStyle(styleName.text); styleName.clear() } }
                    Button { Layout.fillWidth: true; text: qsTr("Update"); enabled: backend.selectionCount===1 && savedStyle.count>0; onClicked: backend.saveObjectStyle(styleName.text.trim() || savedStyle.currentText,savedStyle.currentValue) }
                }
            }
            Divider {}
            FieldRow {
                label: root.isText ? qsTr("Colour") : qsTr("Fill")
                visible: !root.isImage && !root.isMedia
                Rectangle {
                    implicitWidth: Theme.hControl + Theme.s3
                    implicitHeight: Theme.hControl
                    radius: Theme.rControl
                    color: root.swatch
                    border.color: Theme.borderStrong
                    border.width: Theme.hairline
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.swatchText
                    font.family: Theme.monoFamily
                    Accessible.name: root.isText ? qsTr("Text colour") : qsTr("Fill colour")
                    onEditingFinished:
                        backend.setSelectedProperty(root.isText ? "textColor" : "fill", text)
                }
            }

            FieldRow {
                label: qsTr("Opacity")
                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 1
                    value: root.sel.opacity ?? 1
                    onPressedChanged: if (!pressed) backend.setSelectedProperty("opacity", value)
                    Accessible.name: qsTr("Opacity")
                }
                Rectangle {
                    implicitWidth: Theme.s5 * 2 + Theme.s2
                    implicitHeight: Theme.hControl
                    radius: Theme.rControl
                    color: Theme.controlBg
                    border.color: Theme.border; border.width: Theme.hairline
                    Label {
                        anchors.centerIn: parent
                        text: Math.round((root.sel.opacity ?? 1) * 100) + "%"
                        font.family: Theme.monoFamily
                    }
                }
            }

            FieldRow {
                label: qsTr("Corner radius")
                visible: !root.isText && !root.isImage && !root.isMedia && !root.isConnector
                NumField {
                    Layout.fillWidth: true
                    suffix: " px"
                    value: root.sel.cornerRadius ?? 0
                    onCommitted: (v) => backend.setSelectedProperty("cornerRadius", v)
                }
            }
            Divider { visible: shapeTools.visible || connectorTools.visible }
            ShapeTools { id: shapeTools; Layout.fillWidth: true; visible: backend.hasSelection && root.sel.type === "rect" && !root.sel.connector }
            ConnectorTools { id: connectorTools; Layout.fillWidth: true; visible: root.isConnector }
            ColumnLayout {
                visible: root.isImage
                Layout.fillWidth: true
                spacing: Theme.s3
                SectionLabel { text: root.sel.imageFormat === "svg" ? qsTr("SVG VECTOR") : qsTr("PICTURE") }
                FieldRow {
                    label: qsTr("Fit")
                    ComboBox {
                        objectName: "imageFitMode"
                        Layout.fillWidth: true
                        model: [qsTr("Fit inside box"),qsTr("Fill box"),qsTr("Stretch to box")]
                        currentIndex: root.sel.imageMode ?? 0
                        onActivated: backend.setSelectedProperty("imageMode",currentIndex)
                    }
                }
                Button { objectName: "cropOnSlide"; Layout.fillWidth: true; icon.name: "crop"; text: qsTr("Crop on slide"); onClicked: backend.editSelectedImageCrop() }
                SectionLabel { text: qsTr("SOURCE CROP (%)") }
                GridLayout {
                    columns: 2
                    columnSpacing: Theme.s2
                    Layout.fillWidth: true
                    NumField { objectName: "cropX"; Layout.fillWidth: true; label: qsTr("X"); value: (root.sel.cropX ?? 0)*100; onCommitted: v => backend.setSelectedProperty("cropX",v/100) }
                    NumField { objectName: "cropY"; Layout.fillWidth: true; label: qsTr("Y"); value: (root.sel.cropY ?? 0)*100; onCommitted: v => backend.setSelectedProperty("cropY",v/100) }
                    NumField { objectName: "cropW"; Layout.fillWidth: true; label: qsTr("W"); value: (root.sel.cropW ?? 1)*100; onCommitted: v => backend.setSelectedProperty("cropW",v/100) }
                    NumField { objectName: "cropH"; Layout.fillWidth: true; label: qsTr("H"); value: (root.sel.cropH ?? 1)*100; onCommitted: v => backend.setSelectedProperty("cropH",v/100) }
                }
                PictureAdjustments { Layout.fillWidth: true }
                RowLayout {
                    Layout.fillWidth: true
                    Button { Layout.fillWidth: true; text: qsTr("Reset crop"); onClicked: backend.resetImageCrop() }
                    Button { Layout.fillWidth: true; text: qsTr("Replace…"); onClicked: backend.replaceImageDialog() }
                }
            }
            ColumnLayout {
                visible: backend.hasSelection && (root.isImage || root.sel.fillStyle===4) && root.sel.imageFormat!=="svg"
                Layout.fillWidth: true
                Button { objectName: "optimisePicture"; Layout.fillWidth: true; text: qsTr("Optimise picture…"); onClicked: backend.optimiseSelectedMedia() }
                Button { objectName: "restoreOriginalPicture"; Layout.fillWidth: true; visible: root.sel.imageHasOriginal??false; text: qsTr("Restore original picture"); onClicked: backend.restoreOriginalMedia() }
                Button { objectName: "discardOriginalPicture"; Layout.fillWidth: true; visible: root.sel.imageHasOriginal??false; text: qsTr("Discard original to reduce deck size"); onClicked: backend.discardOriginalMedia() }
            }

            Divider {}
            Button { objectName: "objectLinkButton"; Layout.fillWidth: true; icon.name: "link"; text: root.sel.linkKind?qsTr("Edit link or action…"):qsTr("Add link or action…"); onClicked: backend.editSelectedLink() }
            Label { Layout.fillWidth: true; visible: !!root.sel.linkIssue; text: root.sel.linkIssue??""; color: Theme.warning; wrapMode: Text.Wrap }

            Item { Layout.fillHeight: true }
        }

        // ── Text ────────────────────────────────────────────────────────────
        ColumnLayout {
            spacing: Theme.s3

            TextArea {
                objectName: "textContent"
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.s5 * 4
                text: root.sel.text ?? ""
                wrapMode: TextArea.Wrap
                // Forced plain: AutoText would parse a stray angle bracket or a
                // pasted filename as rich text.
                textFormat: TextEdit.PlainText
                Accessible.name: qsTr("Text content")
                onEditingFinished: backend.setSelectedProperty("text", text)
            }

            FieldRow {
                label: qsTr("Content")
                ComboBox {
                    objectName: "textKind"
                    Layout.fillWidth: true
                    Layout.minimumWidth: Theme.s5 * 5
                    // Along a path is not something to choose here: it needs a
                    // shape to follow, so it arrives through Arrange.
                    model: [qsTr("Words"), qsTr("An equation"), qsTr("Along a path")]
                    currentIndex: root.sel.textKind ?? 0
                    delegate: ItemDelegate {
                        required property int index
                        required property string modelData
                        width: parent.width
                        text: modelData
                        enabled: index < 2 || (root.sel.textKind ?? 0) === 2
                        highlighted: ListView.isCurrentItem
                    }
                    onActivated: backend.setSelectedProperty("textKind", currentIndex)
                }
            }
            Button {
                objectName: "takeTextOffPath"
                Layout.fillWidth: true
                visible: (root.sel.textKind ?? 0) === 2
                text: qsTr("Take the words off the path")
                onClicked: backend.takeTextOffPath()
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                font.pixelSize: Theme.fsLabel
                visible: (root.sel.textKind ?? 0) === 2
                text: qsTr("Align decides where the words start along the line, and the up-and-down alignment which side of it they sit on. Edit the line itself with the node tools.")
            }
            Button {
                objectName: "equationHelp"
                Layout.fillWidth: true
                visible: (root.sel.textKind ?? 0) === 1
                text: qsTr("What can I type?")
                onClicked: equationHelp.open()
            }
            Label {
                objectName: "equationProblem"
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.warning
                visible: (root.sel.equationError ?? "").length > 0
                text: root.sel.equationError ?? ""
            }
            Sheet {
                id: equationHelp
                objectName: "equationHelpDialog"
                parent: Overlay.overlay; anchors.centerIn: parent
                width: Math.min(parent.width - Theme.s5 * 2, 520)
                modal: true; title: qsTr("Writing an equation")
                standardButtons: Dialog.Close
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s2
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                        text: qsTr("Type it the way it is written by hand. Everything else is ordinary text, drawn in the box's own typeface and colour.")
                    }
                    Repeater {
                        model: [
                            { what: qsTr("Above and below"), how: "x^2   a_n   x_i^2" },
                            { what: qsTr("Fractions"), how: "\\frac{a}{b}" },
                            { what: qsTr("Roots"), how: "\\sqrt{x}   \\sqrt[3]{x}" },
                            { what: qsTr("Sums and integrals"), how: "\\sum_{i=1}^{n}   \\int_0^1   \\lim_{x \\to 0}" },
                            { what: qsTr("Greek"), how: "\\alpha \\beta \\pi \\Omega" },
                            { what: qsTr("Signs"), how: "\\times \\div \\pm \\leq \\geq \\neq \\approx \\infty" },
                            { what: qsTr("Brackets that grow"), how: "\\left( \\frac{a}{b} \\right)" },
                            { what: qsTr("Marks and words"), how: "\\bar{x}   \\hat{y}   \\vec{v}   \\text{where}" },
                            { what: qsTr("Space and new lines"), how: "\\, \\quad   \\\\" }
                        ]
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Theme.s3
                            Label { Layout.preferredWidth: Theme.wFieldLabel * 1.4; text: modelData.what; color: Theme.textMuted }
                            Label { Layout.fillWidth: true; text: modelData.how; font.family: Theme.monoFamily; wrapMode: Text.Wrap }
                        }
                    }
                }
            }

            FieldRow {
                label: qsTr("Font")
                ComboBox {
                    Layout.fillWidth: true
                    editable: true
                    model: Qt.fontFamilies()
                    currentIndex: model.indexOf(root.sel.fontFamily ?? "")
                    displayText: root.sel.fontFamily ?? Theme.fontFamily
                    onActivated: backend.setSelectedProperty("fontFamily",currentText)
                    onAccepted: root.range.active ? backend.formatSelection("fontFamily",editText)
                                                  : backend.setSelectedProperty("fontFamily",editText)
                }
            }
            FieldRow {
                label: qsTr("Size")
                // Half points; grow / shrink beside it. One undo step per change.
                FontSizeField {
                    value: (root.range.active ? (root.range.fontSize ?? Theme.fsBase) : (root.sel.fontSize ?? Theme.fsBase)) * root.ptPerUnit
                    onCommitted: (v) => root.range.active ? backend.formatSelection("fontSize", v / root.ptPerUnit)
                                                          : backend.setSelectedProperty("fontSize", v / root.ptPerUnit)
                }
                ComboBox {
                    objectName: "textWeight"
                    Layout.preferredWidth: Theme.s5 * 5
                    // Only weights the typeface really has, so each entry looks
                    // different; the current value stays listed even if odd.
                    // A face with a single weight has nothing to choose.
                    model: root.weightChoices.map(w => ({ value: w, text: root.weightName(w) }))
                    textRole: "text"
                    valueRole: "value"
                    visible: root.weightChoices.length > 1
                    currentIndex: Math.max(0, root.weightChoices.indexOf(root.currentWeight))
                    onActivated: (index) => root.range.active ? backend.formatSelection("weight", root.weightChoices[index])
                                                              : backend.setSelectedProperty("fontWeight", root.weightChoices[index])
                }
            }
            FieldRow {
                label: qsTr("Style")
                StyleMark { objectName: "textBold"; text: "B"; font.bold: true; tip: qsTr("Bold")
                            checked: (root.range.active ? (root.range.weight ?? 400) : (root.sel.fontWeight ?? 400)) >= 700
                            onToggled: root.bold(checked) }
                StyleMark { objectName: "textItalic"; text: "I"; font.italic: true; tip: qsTr("Italic")
                            checked: root.range.active ? (root.range.italic ?? false) : (root.sel.italic ?? false)
                            onToggled: root.style("italic", checked) }
                StyleMark { objectName: "textUnderline"; text: "U"; font.underline: true; tip: qsTr("Underline")
                            checked: root.range.active ? (root.range.underline ?? false) : (root.sel.underline ?? false)
                            onToggled: root.style("underline", checked) }
                Button { objectName: "textStrike"; checkable: true; text: qsTr("Strike")
                         visible: root.range.active
                         checked: root.range.strike ?? false
                         onToggled: backend.formatSelection("strike", checked ? 1 : 2) }
                Item { Layout.fillWidth: true }
            }
            FieldRow {
                label: qsTr("Baseline")
                visible: root.range.active
                ComboBox {
                    objectName: "textBaseline"
                    Layout.fillWidth: true
                    model: [qsTr("Normal"), qsTr("Raised"), qsTr("Lowered")]
                    currentIndex: root.range.baseline ?? 0
                    onActivated: backend.formatSelection("baseline", currentIndex)
                }
            }
            Label {
                objectName: "textRangeNote"
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                visible: root.range.active
                text: (root.range.mixed ?? []).length > 0
                      ? qsTr("The selected text is not all the same. Changing something here settles it.")
                      : qsTr("These apply to the selected text, not the whole box.")
            }
            FieldRow {
                label: qsTr("Language")
                ComboBox {
                    objectName: "textLanguage"
                    Layout.fillWidth: true
                    editable: true
                    model: [qsTr("Follow the deck")].concat(backend.spellingLanguages())
                    // A stretch of text can be in another language; with nothing
                    // selected it is the whole box.
                    displayText: {
                        const chosen = root.range.active ? (root.range.language ?? "")
                                                         : (root.sel.language ?? "")
                        return chosen.length > 0 ? chosen : qsTr("Follow the deck")
                    }
                    onActivated: {
                        const chosen = currentIndex === 0 ? "" : currentText
                        root.range.active ? backend.formatSelection("language", chosen)
                                          : backend.setSelectedProperty("language", chosen)
                    }
                    onAccepted: root.range.active ? backend.formatSelection("language", editText)
                                                  : backend.setSelectedProperty("language", editText)
                }
            }
            Label {
                objectName: "spellingNote"
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                font.pixelSize: Theme.fsLabel
                visible: !backend.spellingAvailable
                text: qsTr("No dictionary for %1 is installed, so spelling is not checked. Review says the same.").arg(backend.deckLanguage)
            }
            FieldRow {
                label: qsTr("Tab stop")
                NumField { objectName: "textTabStop"; Layout.fillWidth: true; suffix: " pt"; decimals: 1
                           value: (root.sel.tabStop ?? 0) * root.ptPerUnit
                           onCommitted: v => backend.setSelectedProperty("tabStop", v / root.ptPerUnit) }
            }
            FieldRow {
                label: qsTr("Columns")
                SpinBox { objectName: "textColumns"; Layout.fillWidth: true; from: 1; to: 6
                          value: root.sel.columns ?? 1
                          onValueModified: backend.setSelectedProperty("columns", value) }
                NumField { objectName: "textColumnGap"; Layout.fillWidth: true; suffix: " pt"; decimals: 1
                           visible: (root.sel.columns ?? 1) > 1
                           value: (root.sel.columnGap ?? 0) * root.ptPerUnit
                           onCommitted: v => backend.setSelectedProperty("columnGap", v / root.ptPerUnit) }
            }
            FieldRow {
                label: qsTr("Direction")
                ComboBox {
                    objectName: "textDirection"
                    Layout.fillWidth: true
                    model: [qsTr("Follow the words"), qsTr("Left to right"), qsTr("Right to left")]
                    currentIndex: root.sel.direction ?? 0
                    onActivated: backend.setSelectedProperty("direction", currentIndex)
                }
            }
            Divider {}
            FieldRow {
                label: qsTr("Style")
                ComboBox {
                    objectName: "textStyleChoice"
                    Layout.fillWidth: true
                    textRole: "name"
                    valueRole: "id"
                    model: [{ id: "", name: qsTr("No style"), uses: 0 }].concat(backend.textStyles)
                    currentIndex: Math.max(0, model.findIndex(style => style.id === (root.sel.textStyleId ?? "")))
                    onActivated: backend.applyTextStyle(currentValue)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Button { objectName: "keepTextStyle"; Layout.fillWidth: true; text: qsTr("Keep as style…")
                         onClicked: textStyleName.open() }
                Button { objectName: "updateTextStyle"; Layout.fillWidth: true; text: qsTr("Update style")
                         enabled: (root.sel.textStyleId ?? "").length > 0
                         onClicked: backend.updateTextStyleFromSelection() }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                visible: (root.sel.textStyleId ?? "").length > 0
                text: qsTr("This box follows a style. Changing something here is its own, and stays when the style changes.")
            }
            Sheet {
                id: textStyleName
                objectName: "textStyleDialog"
                parent: Overlay.overlay; anchors.centerIn: parent
                width: Math.min(parent.width - Theme.s5 * 2, 460)
                modal: true; title: qsTr("Keep this look as a style")
                standardButtons: Dialog.Save | Dialog.Cancel
                onOpened: { textStyleField.text = ""; textStyleField.forceActiveFocus() }
                onAccepted: backend.addTextStyle(textStyleField.text)
                ColumnLayout {
                    width: parent.width
                    TextField { id: textStyleField; objectName: "textStyleNameField"; Layout.fillWidth: true
                                placeholderText: qsTr("Name for the style")
                                onAccepted: if (text.trim().length) textStyleName.accept() }
                    Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                            font.pixelSize: Theme.fsLabel
                            text: qsTr("Typeface, size, weight, colour, alignment, spacing, lists, tabs, columns and direction. Not the words.") }
                }
            }
            Divider {}
            FieldRow {
                label: qsTr("Align")
                ComboBox {
                    objectName: "textAlignment"
                    Layout.fillWidth: true
                    model: [qsTr("Left"),qsTr("Centre"),qsTr("Right"),qsTr("Justify")]
                    currentIndex: root.sel.textAlign ?? 0
                    onActivated: backend.setSelectedProperty("textAlign",currentIndex)
                }
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Top"),qsTr("Middle"),qsTr("Bottom")]
                    currentIndex: root.sel.verticalAlign ?? 1
                    onActivated: backend.setSelectedProperty("verticalAlign",currentIndex)
                }
            }
            FieldRow {
                label: qsTr("Line height")
                NumField { objectName: "textLineHeight"; Layout.fillWidth: true; suffix: "%"; value: root.sel.lineHeight ?? 100; onCommitted: v => backend.setSelectedProperty("lineHeight",v) }
            }
            FieldRow {
                label: qsTr("After ¶")
                NumField { objectName: "textParagraphSpacing"; Layout.fillWidth: true; value: root.sel.paragraphSpacing ?? 0; onCommitted: v => backend.setSelectedProperty("paragraphSpacing",v) }
            }
            FieldRow {
                label: qsTr("Indent")
                NumField { Layout.fillWidth: true; value: root.sel.textIndent ?? 0; onCommitted: v => backend.setSelectedProperty("textIndent",v) }
            }
            FieldRow {
                label: qsTr("List")
                ComboBox {
                    objectName: "textListStyle"
                    Layout.fillWidth: true
                    model: [qsTr("None"), qsTr("Bullets"), qsTr("Numbers"), qsTr("Circles"),
                            qsTr("Squares"), qsTr("a. b. c."), qsTr("A. B. C."),
                            qsTr("i. ii. iii."), qsTr("I. II. III.")]
                    currentIndex: root.sel.listStyle ?? 0
                    onActivated: backend.setSelectedProperty("listStyle",currentIndex)
                }
                NumField {
                    Layout.preferredWidth: Theme.s5 * 3
                    visible: [2,5,6,7,8].indexOf(root.sel.listStyle ?? 0) >= 0
                    label: qsTr("#"); value: root.sel.listStart ?? 1
                    onCommitted: v => backend.setSelectedProperty("listStart",v)
                }
            }
            Label { visible: (root.sel.listStyle ?? 0) > 0; text: qsTr("Leading tabs nest list items."); color: Theme.textMuted; font.pixelSize: Theme.fsLabel }
            Divider {}
            FieldRow {
                label: qsTr("Text fit")
                ComboBox {
                    objectName: "textFitMode"
                    Layout.fillWidth: true
                    model: [qsTr("Clip and warn"),qsTr("Shrink to fit")]
                    currentIndex: root.sel.textFit ?? 0
                    onActivated: backend.setSelectedProperty("textFit",currentIndex)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                visible: (root.sel.textOverflow ?? false) || (root.sel.textFit === 1 && root.sel.effectiveFontSize < root.sel.fontSize)
                Icon { name: (root.sel.textOverflow ?? false) ? "triangle-alert" : "info"; color: (root.sel.textOverflow ?? false) ? Theme.warning : Theme.textMuted }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    color: (root.sel.textOverflow ?? false) ? Theme.warning : Theme.textSecondary
                    text: (root.sel.textOverflow ?? false) ? qsTr("Some text is outside this box.") : qsTr("Displayed at %1 pt to fit.").arg(Math.round((root.sel.effectiveFontSize ?? 0) * root.ptPerUnit * 10) / 10)
                }
            }
            Button { objectName: "fitTextBox"; Layout.fillWidth: true; text: qsTr("Resize box to fit text"); onClicked: backend.fitSelectedTextBox() }
            Item { Layout.fillHeight: true }
        }

        // ── Arrange ─────────────────────────────────────────────────────────
        ColumnLayout {
            spacing: Theme.s3

            FieldRow {
                label: qsTr("Position")
                NumField { enabled: !root.isConnector; Layout.fillWidth: true; label: qsTr("X"); value: root.sel.x ?? 0
                           onCommitted: (v) => backend.setSelectedProperty("x", v) }
                NumField { enabled: !root.isConnector; Layout.fillWidth: true; label: qsTr("Y"); value: root.sel.y ?? 0
                           onCommitted: (v) => backend.setSelectedProperty("y", v) }
            }
            FieldRow {
                label: qsTr("Size")
                NumField { enabled: !root.isConnector; Layout.fillWidth: true; label: qsTr("W"); value: root.sel.w ?? 0
                           onCommitted: (v) => backend.setSelectedProperty("w", v) }
                NumField { enabled: !root.isConnector; Layout.fillWidth: true; label: qsTr("H"); value: root.sel.h ?? 0
                           onCommitted: (v) => backend.setSelectedProperty("h", v) }
            }
            FieldRow {
                label: qsTr("Rotation")
                visible: backend.selectionCount === 1
                Icon { name: "rotate-cw"; color: Theme.textMuted }
                NumField {
                    enabled: !root.isConnector
                    Layout.preferredWidth: Theme.s5 * 4
                    suffix: "°"
                    value: root.sel.rotation ?? 0
                    onCommitted: (v) => backend.setSelectedProperty("rotation", v)
                }
                Item { Layout.fillWidth: true }
            }
            Divider {}
            FieldRow {
                label: qsTr("Align to")
                ComboBox {
                    id: alignmentReference
                    Layout.fillWidth: true
                    model: [qsTr("Selection"), qsTr("Slide"), qsTr("First selected object")]
                }
            }
            FieldRow {
                label: qsTr("Align")
                Repeater {
                    model: [{name:qsTr("Align left"),key:"left",icon:"align-start-vertical"},{name:qsTr("Align centre"),key:"center",icon:"align-center-vertical"},{name:qsTr("Align right"),key:"right",icon:"align-end-vertical"},
                            {name:qsTr("Align top"),key:"top",icon:"align-start-horizontal"},{name:qsTr("Align middle"),key:"middle",icon:"align-center-horizontal"},{name:qsTr("Align bottom"),key:"bottom",icon:"align-end-horizontal"}]
                    IconAction { required property var modelData; icon.name: modelData.icon; text: modelData.name; tip: modelData.name
                                 onClicked: backend.alignSelected(modelData.key,alignmentReference.currentIndex) }
                }
                Item { Layout.fillWidth: true }
            }
            FieldRow {
                label: qsTr("Distribute")
                IconAction { icon.name: "align-horizontal-distribute-center"; text: qsTr("Space across"); tip: qsTr("Space evenly across — three or more objects"); enabled: backend.selectionCount >= 3
                             onClicked: backend.distributeSelected(true,alignmentReference.currentIndex) }
                IconAction { icon.name: "align-vertical-distribute-center"; text: qsTr("Space down"); tip: qsTr("Space evenly down — three or more objects"); enabled: backend.selectionCount >= 3
                             onClicked: backend.distributeSelected(false,alignmentReference.currentIndex) }
                Item { Layout.fillWidth: true }
            }
            Divider {}
            FieldRow {
                label: qsTr("Group")
                Button { objectName: "groupButton"; Layout.fillWidth: true; icon.name: "group"; text: qsTr("Group"); enabled: backend.selectionCount > 1; onClicked: backend.groupSelected() }
                Button { Layout.fillWidth: true; icon.name: "ungroup"; text: qsTr("Ungroup"); enabled: !!root.sel.groupId; onClicked: backend.ungroupSelected() }
            }
            FieldRow {
                label: qsTr("Order")
                Button { Layout.fillWidth: true; icon.name: "bring-to-front"; text: qsTr("Forward"); onClicked: backend.raiseSelected(1) }
                Button { Layout.fillWidth: true; icon.name: "send-to-back"; text: qsTr("Backward"); onClicked: backend.raiseSelected(-1) }
            }
            Item { Layout.fillHeight: true }
        }
    }
    }
}

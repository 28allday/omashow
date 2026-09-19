import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

RowLayout {
    id: root
    objectName: "designWorkspace"
    spacing: 0
    readonly property var info: backend.design
    property string masterId: ""
    property string layoutId: ""
    property string roleId: ""
    property int previewTheme: -1
    readonly property var master: info.masters.find(m => m.id === masterId) ?? info.masters[0] ?? ({})
    readonly property var layouts: info.layouts.filter(l => l.masterId === (master.id ?? ""))
    readonly property var layout: layouts.find(l => l.id === layoutId) ?? layouts[0] ?? ({})
    readonly property var placeholder: (layout.placeholders ?? []).find(p => p.id === roleId) ?? layout.placeholders?.[0] ?? ({})
    function preview(id, preset) { return "image://slides/layout/" + (id ?? "") + "/" + backend.revision + "/" + preset }
    function setPlaceholder(key, value) { backend.setPlaceholderProperty(layout.id, placeholder.id, key, value) }

    component Heading: SectionLabel {}
    component Action: Button {
        flat: true
        Layout.fillWidth: true
        ToolTip.visible: hovered && display === AbstractButton.IconOnly
        ToolTip.delay: Theme.tooltipDelay
        ToolTip.text: text
    }
    // A selectable list row: accent tint and a lit edge when current.
    component PickRow: ItemDelegate {
        property bool current: false
        Layout.fillWidth: true
        highlighted: current
        background: Rectangle {
            implicitHeight: Theme.hRow
            radius: Theme.rControl
            color: parent.current ? Theme.withAlpha(Theme.accent, 0.12) : parent.hovered ? Theme.controlHover : "transparent"
            border.width: parent.current ? Theme.hairline : 0
            border.color: Theme.accent
        }
    }
    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }

    Rectangle {
        Layout.preferredWidth: Theme.wNavigator + Theme.s5
        Layout.fillHeight: true
        color: Theme.panelBg
        Rectangle { anchors.right: parent.right; width: Theme.hairline; height: parent.height; color: Theme.border }
        ScrollView {
            anchors.fill: parent
            anchors.margins: Theme.s3
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: Theme.s2
                Heading { text: qsTr("MASTERS") }
                Button { visible: root.info.masters.length === 0; text: qsTr("Add starter layouts"); onClicked: backend.setupDesign() }
                Repeater {
                    model: root.info.masters
                    PickRow {
                        required property var modelData
                        text: modelData.name + " · " + qsTr("%1 slides").arg(modelData.slides)
                        current: modelData.id === root.master.id
                        onClicked: { root.masterId = modelData.id; root.layoutId = ""; root.roleId = "" }
                    }
                }
                RowLayout {
                    Action { icon.name: "plus"; display: AbstractButton.IconOnly; text: qsTr("New"); onClicked: root.masterId = backend.addMaster() }
                    Action { icon.name: "copy"; display: AbstractButton.IconOnly; text: qsTr("Duplicate"); enabled: !!root.master.id; onClicked: root.masterId = backend.addMaster(root.master.id) }
                    Action { icon.name: "trash-2"; display: AbstractButton.IconOnly; text: qsTr("Delete"); enabled: !!root.master.id; onClicked: removal.ask(true) }
                }
                TextField { Layout.fillWidth: true; enabled: !!root.master.id; text: root.master.name ?? "";
                            placeholderText: qsTr("Master name"); onEditingFinished: backend.setMasterProperty(root.master.id, "name", text) }
                RowLayout {
                    Action { icon.name: "arrow-up"; text: qsTr("Move up"); enabled: !!root.master.id; onClicked: backend.moveMaster(root.master.id, -1) }
                    Action { icon.name: "arrow-down"; text: qsTr("Move down"); enabled: !!root.master.id; onClicked: backend.moveMaster(root.master.id, 1) }
                }
                Heading { text: qsTr("MASTER BACKGROUND") }
                TextField { Layout.fillWidth: true; enabled: !!root.master.id; text: root.master.background ?? "";
                            onEditingFinished: backend.setMasterProperty(root.master.id, "background", text) }
                Action { text: root.master.backgroundToken ? qsTr("Linked to theme background") : qsTr("Use theme background");
                         enabled: !!root.master.id && !root.master.backgroundToken;
                         onClicked: backend.setMasterProperty(root.master.id, "backgroundToken", "background") }
                Divider {}
                Heading { text: qsTr("LAYOUTS") }
                // Layouts as thumbnails, two to a row.
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.s2
                    rowSpacing: Theme.s2
                    Repeater {
                        model: root.layouts
                        Item {
                            id: layoutCard
                            required property var modelData
                            readonly property bool current: modelData.id === root.layout.id
                            Layout.fillWidth: true
                            implicitHeight: thumbFrame.height + Theme.hRow - Theme.s1
                            Rectangle {
                                id: thumbFrame
                                width: parent.width
                                height: width * backend.slideSize.height / Math.max(1, backend.slideSize.width)
                                radius: Theme.rControl
                                color: Theme.showBg
                                border.width: layoutCard.current ? Theme.selectionRing : Theme.hairline
                                border.color: layoutCard.current ? Theme.accent : Theme.border
                                Image { anchors.fill: parent; anchors.margins: parent.border.width; fillMode: Image.PreserveAspectFit; cache: false
                                        source: root.preview(layoutCard.modelData.id, -1); sourceSize.width: 240 }
                            }
                            Label { anchors.top: thumbFrame.bottom; anchors.topMargin: 2; width: parent.width; elide: Text.ElideRight
                                    text: layoutCard.modelData.name; font.pixelSize: Theme.fsLabel
                                    color: layoutCard.current ? Theme.accent : Theme.textSecondary }
                            TapHandler { onTapped: { root.layoutId = layoutCard.modelData.id; root.roleId = "" } }
                        }
                    }
                }
                RowLayout {
                    Action { icon.name: "plus"; display: AbstractButton.IconOnly; text: qsTr("New"); enabled: !!root.master.id; onClicked: root.layoutId = backend.addLayout(root.master.id) }
                    Action { icon.name: "copy"; display: AbstractButton.IconOnly; text: qsTr("Duplicate"); enabled: !!root.layout.id; onClicked: root.layoutId = backend.addLayout(root.master.id, root.layout.id) }
                    Action { icon.name: "trash-2"; display: AbstractButton.IconOnly; text: qsTr("Delete"); enabled: !!root.layout.id; onClicked: removal.ask(false) }
                }
                TextField { Layout.fillWidth: true; text: root.layout.name ?? ""; enabled: !!root.layout.id;
                            placeholderText: qsTr("Layout name"); onEditingFinished: backend.setLayoutProperty(root.layout.id, "name", text) }
                Label { Layout.fillWidth: true; text: qsTr("Used by %1 slides").arg(root.layout.slides ?? 0); color: Theme.textMuted }
                Button { objectName: "applyLayoutButton"; Layout.fillWidth: true; text: qsTr("Apply to slide %1").arg(backend.currentSlide + 1);
                         enabled: !!root.layout.id; onClicked: backend.applyLayout(root.layout.id) }
                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel;
                        text: qsTr("Matching placeholders keep their content and local edits. Unmatched objects stay on the slide.") }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.margins: Theme.s4
        spacing: Theme.s3
        Heading { text: qsTr("THEME PREVIEW") }
        RowLayout {
            Layout.fillWidth: true
            Repeater {
                model: [qsTr("Midnight"), qsTr("Paper"), qsTr("Grove")]
                Button {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    icon.name: "palette"
                    checked: root.previewTheme === index || (root.previewTheme < 0 && root.info.name === modelData)
                    onClicked: root.previewTheme = index
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.pasteboard
            radius: Theme.rCard
            border.width: Theme.hairline
            border.color: Theme.border
            Image {
                objectName: "layoutPreview"
                anchors.fill: parent
                anchors.margins: Theme.s4
                fillMode: Image.PreserveAspectFit
                source: root.preview(root.layout.id, root.previewTheme)
                sourceSize.width: Math.max(1, width * Screen.devicePixelRatio)
                cache: false
            }
        }
        RowLayout {
            Label { Layout.fillWidth: true; text: root.previewTheme < 0 ? qsTr("Current theme: %1").arg(root.info.name) : qsTr("Theme preview · no changes applied"); color: Theme.textSecondary }
            Button { visible: root.previewTheme >= 0; text: qsTr("Cancel"); onClicked: root.previewTheme = -1 }
            Button { objectName: "applyThemeButton"; visible: root.previewTheme >= 0; text: qsTr("Apply theme");
                     onClicked: { backend.applyTheme(root.previewTheme); root.previewTheme = -1 } }
        }
        Label { Layout.fillWidth: true; color: Theme.textMuted; wrapMode: Text.Wrap;
                text: qsTr("%1 linked slides · %2 × %3 · local colours are preserved").arg(root.info.linkedSlides).arg(backend.slideSize.width).arg(backend.slideSize.height) }
        Heading { text: qsTr("PLACEHOLDERS") }
        ComboBox {
            Layout.fillWidth: true
            model: root.layout.placeholders ?? []
            textRole: "id"; valueRole: "id"
            currentIndex: model.findIndex(p => p.id === root.placeholder.id)
            onActivated: root.roleId = currentValue
        }
        RowLayout {
            Action { icon.name: "type"; text: qsTr("Add text"); enabled: !!root.layout.id; onClicked: backend.addPlaceholder(root.layout.id, false) }
            Action { icon.name: "shapes"; text: qsTr("Add shape"); enabled: !!root.layout.id; onClicked: backend.addPlaceholder(root.layout.id, true) }
            Action { icon.name: "arrow-up"; text: qsTr("Earlier"); enabled: !!root.placeholder.id; onClicked: backend.movePlaceholder(root.layout.id, root.placeholder.id, -1) }
            Action { icon.name: "arrow-down"; text: qsTr("Later"); enabled: !!root.placeholder.id; onClicked: backend.movePlaceholder(root.layout.id, root.placeholder.id, 1) }
        }
        Button { text: qsTr("Remove placeholder · keep slide content"); enabled: !!root.placeholder.id;
                 onClicked: backend.deletePlaceholder(root.layout.id, root.placeholder.id) }
    }

    Rectangle {
        Layout.preferredWidth: Theme.wInspector
        Layout.fillHeight: true
        color: Theme.panelBg
        Rectangle { anchors.left: parent.left; width: Theme.hairline; height: parent.height; color: Theme.border; z: 1 }
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            TabBar {
                id: inspectorTabs
                Layout.fillWidth: true
                TabButton { text: qsTr("Placeholder") }
                TabButton { text: qsTr("Theme") }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Theme.s4
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s3
                    ColumnLayout {
                        visible: inspectorTabs.currentIndex === 0
                        Layout.fillWidth: true
                        enabled: !!root.placeholder.id
                        Heading { text: root.placeholder.type === "rect" ? qsTr("SHAPE PLACEHOLDER") : qsTr("TEXT PLACEHOLDER") }
                        Heading { text: qsTr("POSITION AND SIZE") }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: Theme.s2
                            Repeater {
                                model: ["x", "y", "w", "h"]
                                NumField {
                                    required property string modelData
                                    Layout.fillWidth: true
                                    label: modelData.toUpperCase()
                                    value: root.placeholder[modelData] ?? 0
                                    onCommitted: v => root.setPlaceholder(modelData, v)
                                }
                            }
                        }
                        Heading { text: qsTr("DEFAULT PROMPT"); visible: root.placeholder.type === "text" }
                        TextArea {
                            Layout.fillWidth: true
                            visible: root.placeholder.type === "text"
                            text: root.placeholder.text ?? ""
                            wrapMode: TextEdit.Wrap
                            textFormat: TextEdit.PlainText
                            onEditingFinished: root.setPlaceholder("text", text)
                        }
                        Heading { text: qsTr("TYPE SIZE"); visible: root.placeholder.type === "text" }
                        NumField { Layout.fillWidth: true; visible: root.placeholder.type === "text";
                                   value: root.placeholder.fontSize ?? 48; onCommitted: v => root.setPlaceholder("fontSize", v) }
                        ComboBox {
                            Layout.fillWidth: true
                            visible: root.placeholder.type === "text"
                            model: [400, 500, 600, 700]
                            currentIndex: Math.max(0, model.indexOf(root.placeholder.fontWeight ?? 400))
                            onActivated: root.setPlaceholder("fontWeight", model[currentIndex])
                        }
                        Heading { text: qsTr("COLOUR") }
                        TextField {
                            Layout.fillWidth: true
                            text: (root.placeholder.type === "text" ? root.placeholder.textColor : root.placeholder.fill) ?? ""
                            onEditingFinished: root.setPlaceholder(root.placeholder.type === "text" ? "textColor" : "fill", text)
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Local colour"), "foreground", "muted", "accent", "background"]
                            currentIndex: Math.max(0, model.indexOf((root.placeholder.type === "text" ? root.placeholder.textColorToken : root.placeholder.fillToken) ?? ""))
                            onActivated: root.setPlaceholder(root.placeholder.type === "text" ? "textColorToken" : "fillToken", currentIndex > 0 ? model[currentIndex] : "")
                        }
                        Heading { text: qsTr("FONT LINK"); visible: root.placeholder.type === "text" }
                        ComboBox {
                            Layout.fillWidth: true
                            visible: root.placeholder.type === "text"
                            model: ["heading", "body"]
                            currentIndex: Math.max(0, model.indexOf(root.placeholder.fontToken ?? "body"))
                            onActivated: root.setPlaceholder("fontToken", currentText)
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: inspectorTabs.currentIndex === 1
                        Heading { text: qsTr("THEME COLOURS") }
                        Repeater {
                            model: ["background", "foreground", "muted", "accent"]
                            FieldRow {
                                required property string modelData
                                label: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                                Rectangle { implicitWidth: Theme.hControl; implicitHeight: Theme.hControl; radius: Theme.rControl
                                            color: root.info.colors[modelData] ?? Theme.controlBg; border.color: Theme.borderStrong; border.width: Theme.hairline }
                                TextField { Layout.fillWidth: true; text: root.info.colors[modelData]; font.family: Theme.monoFamily
                                            onEditingFinished: backend.setThemeToken(modelData, text, false) }
                            }
                        }
                        Heading { text: qsTr("THEME FONTS") }
                        Repeater {
                            model: ["heading", "body"]
                            FieldRow {
                                required property string modelData
                                label: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                                ComboBox { Layout.fillWidth: true; model: Qt.fontFamilies(); editable: true;
                                           currentIndex: model.indexOf(root.info.fonts[modelData]);
                                           onActivated: backend.setThemeToken(modelData, currentText, true);
                                           onAccepted: backend.setThemeToken(modelData, editText, true) }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: removal
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: isMaster ? qsTr("Delete master") : qsTr("Delete layout")
        standardButtons: Dialog.Cancel | Dialog.Ok
        property bool isMaster: true
        property string targetId: ""
        property int uses: 0
        property var choices: []
        function ask(master) {
            isMaster = master
            const target = master ? root.master : root.layout
            targetId = target.id
            uses = master ? target.layouts : target.slides
            choices = (master ? root.info.masters : root.info.layouts).filter(item => item.id !== targetId)
            open()
        }
        onOpened: standardButton(Dialog.Ok).enabled = uses === 0 || choices.length > 0
        ColumnLayout {
            width: Theme.wInspector
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap;
                    text: removal.isMaster ? qsTr("%1 layouts use this master. Select their replacement.").arg(removal.uses)
                                           : qsTr("%1 slides use this layout. Content is kept when replacing it.").arg(removal.uses) }
            ComboBox { id: replacement; Layout.fillWidth: true; visible: removal.uses > 0;
                       model: removal.choices; textRole: "name"; valueRole: "id" }
        }
        onAccepted: {
            if (isMaster) backend.deleteMaster(targetId, uses > 0 ? replacement.currentValue : "")
            else backend.deleteLayout(targetId, uses > 0 ? replacement.currentValue : "")
        }
    }
}

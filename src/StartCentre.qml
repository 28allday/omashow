import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

Rectangle {
    id: root
    objectName: "startCentre"
    color: Theme.windowBg
    property int themeIndex: 0
    property int layoutIndex: 0
    readonly property real deckWidth: dimensions.currentIndex === 4 ? customWidth.value : sizes[dimensions.currentIndex].w
    readonly property real deckHeight: dimensions.currentIndex === 4 ? customHeight.value : sizes[dimensions.currentIndex].h
    readonly property var sizes: [{w:1920,h:1080},{w:1440,h:1080},{w:1080,h:1920},{w:1080,h:1080},{w:1920,h:1080}]
    readonly property var themes: [qsTr("Midnight"),qsTr("Paper"),qsTr("Grove")]
    signal createRequested()
    signal openRequested()
    signal recentRequested(string path)
    signal templateRequested(string id)
    property string chosenTemplate: ""
    readonly property var chosen: backend.templates.find(row => row.id === chosenTemplate) ?? ({})
    // Search matches the name, where it came from and the typefaces it asks for.
    readonly property var shownTemplates: {
        const needle = templateSearch.text.trim().toLowerCase()
        const shape = templateAspect.currentIndex
        return backend.templates.filter(row => {
            if (shape === 1 && row.aspect !== "16:9") return false
            if (shape === 2 && row.aspect !== "4:3") return false
            if (shape === 3 && row.aspect !== qsTr("Square")) return false
            if (shape === 4 && row.aspect !== qsTr("Portrait")) return false
            if (needle.length === 0) return true
            const hay = (row.name + " " + row.source + " " + (row.fonts ?? []).join(" ")
                         + " " + (row.layouts ?? []).join(" ")).toLowerCase()
            return needle.split(/\s+/).every(word => hay.indexOf(word) >= 0)
        })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s5
        spacing: Theme.s5
        RowLayout {
            Layout.fillWidth: true
            Label { text: Theme.appName; font.pixelSize: Theme.fsStartTitle; font.weight: Theme.wHeading }
            Item { Layout.fillWidth: true }
            Button { text: qsTr("Back to deck"); visible: backend.hasDocument; onClicked: backend.resumeDeck() }
            Button { text: qsTr("Open deck…"); onClicked: root.openRequested() }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.s5
            ScrollView {
                id: templateScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: templateScroll.availableWidth
                    spacing: Theme.s4
                    Label { text: qsTr("Make room for your next idea."); font.pixelSize: Theme.fsStartHeading; font.weight: Theme.wHeading }
                    Label { text: qsTr("Choose a theme. Your colours, fonts and layouts stay editable."); color: Theme.textSecondary; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.s3
                        Repeater {
                            model: root.themes
                            Button {
                                id: card
                                required property int index
                                required property string modelData
                                objectName: "startTheme" + index
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                implicitHeight: Theme.hTemplateCard
                                checkable: true
                                checked: root.themeIndex === index
                                onClicked: root.themeIndex = index
                                background: Rectangle {
                                    color: card.hovered ? Theme.panelRaised : Theme.panelBg
                                    border.color: card.checked ? Theme.accent : Theme.border
                                    border.width: card.checked ? Theme.selectionRing : Theme.hairline
                                    radius: Theme.rControl
                                }
                                contentItem: ColumnLayout {
                                    spacing: Theme.s2
                                    Image {
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        source: "image://slides/start/" + card.index + "/" + root.deckWidth + "/" + root.deckHeight + "/" + root.layoutIndex
                                        sourceSize.width: Theme.wTemplatePreview
                                        fillMode: Image.PreserveAspectFit
                                    }
                                    Label { text: card.modelData; color: card.checked ? Theme.accent : Theme.textPrimary; font.weight: Theme.wHeading }
                                }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }
                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        columnSpacing: Theme.s4
                        Label { text: qsTr("SLIDE SIZE"); color: Theme.textMuted; font.pixelSize: Theme.fsLabel }
                        Label { text: qsTr("FIRST SLIDE"); color: Theme.textMuted; font.pixelSize: Theme.fsLabel }
                        ComboBox {
                            id: dimensions
                            objectName: "startDimensions"
                            Layout.fillWidth: true
                            model: [qsTr("Widescreen · 16:9"),qsTr("Standard · 4:3"),qsTr("Portrait · 9:16"),qsTr("Square · 1:1"),qsTr("Custom")]
                        }
                        ComboBox {
                            objectName: "startLayout"
                            Layout.fillWidth: true
                            model: [qsTr("Title"),qsTr("Title and body"),qsTr("Blank")]
                            currentIndex: root.layoutIndex
                            onActivated: root.layoutIndex = currentIndex
                        }
                        RowLayout {
                            visible: dimensions.currentIndex === 4
                            Layout.columnSpan: 2
                            NumField { id: customWidth; objectName: "startWidth"; Layout.fillWidth: true; label: qsTr("W"); value: 1920; onCommitted: v => value = Math.max(240,Math.min(10000,v)) }
                            NumField { id: customHeight; objectName: "startHeight"; Layout.fillWidth: true; label: qsTr("H"); value: 1080; onCommitted: v => value = Math.max(240,Math.min(10000,v)) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: root.themes[root.themeIndex]; font.weight: Theme.wHeading }
                            Label { text: Math.round(root.deckWidth) + " × " + Math.round(root.deckHeight) + qsTr(" · 3 layouts"); color: Theme.textSecondary }
                        }
                        Button { objectName: "createDeckButton"; text: qsTr("Create presentation"); highlighted: true; onClicked: root.createRequested() }
                    }
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap
                        text: qsTr("Original built-in themes · stored locally as .omashow decks")
                        color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    }

                    // ── Templates: decks kept to start from ──────────────────
                    Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: qsTr("Templates"); font.pixelSize: Theme.fsTitle; font.weight: Theme.wHeading }
                        Item { Layout.fillWidth: true }
                        TextField {
                            id: templateSearch
                            objectName: "templateSearch"
                            Layout.preferredWidth: 220
                            placeholderText: qsTr("Search templates")
                        }
                        ComboBox {
                            id: templateAspect
                            objectName: "templateAspect"
                            Layout.preferredWidth: 150
                            model: [qsTr("Any shape"), "16:9", "4:3", qsTr("Square"), qsTr("Portrait")]
                        }
                        Button { objectName: "installTemplate"; text: qsTr("Install…")
                                 onClicked: backend.installTemplateDialog() }
                    }
                    Label {
                        objectName: "templateNothing"
                        Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                        visible: root.shownTemplates.length === 0
                        text: qsTr("Nothing matches that. Install a deck as a template, or keep the one you are working on from File ▸ Keep as template.")
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: Math.max(2, Math.floor(templateScroll.availableWidth / 260))
                        columnSpacing: Theme.s3
                        rowSpacing: Theme.s3
                        Repeater {
                            model: root.shownTemplates
                            Rectangle {
                                id: templateCard
                                required property var modelData
                                required property int index
                                objectName: "template" + index
                                Layout.fillWidth: true
                                implicitHeight: templateBody.implicitHeight + Theme.s3 * 2
                                radius: Theme.rCard
                                color: chosen ? Theme.panelRaised : Theme.panelBg
                                border.width: chosen ? Theme.selectionRing : Theme.hairline
                                border.color: chosen ? Theme.accent : Theme.border
                                readonly property bool chosen: root.chosenTemplate === modelData.id
                                TapHandler { onTapped: root.chosenTemplate = templateCard.modelData.id }
                                ColumnLayout {
                                    id: templateBody
                                    anchors.fill: parent
                                    anchors.margins: Theme.s3
                                    spacing: Theme.s1
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: width * 9 / 16
                                        radius: Theme.rControl
                                        color: Theme.showBg
                                        clip: true
                                        Image {
                                            anchors.fill: parent
                                            fillMode: Image.PreserveAspectFit
                                            cache: false
                                            sourceSize.width: 360
                                            source: templateCard.modelData.broken ? ""
                                                    : "image://slides/template/" + encodeURIComponent(templateCard.modelData.id)
                                        }
                                        Label {
                                            anchors.centerIn: parent
                                            visible: templateCard.modelData.broken ?? false
                                            text: qsTr("Will not open"); color: Theme.accent
                                        }
                                    }
                                    Label { Layout.fillWidth: true; elide: Text.ElideRight
                                            text: templateCard.modelData.name; font.weight: Theme.wHeading }
                                    Label {
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                        color: Theme.textSecondary; font.pixelSize: Theme.fsLabel
                                        text: templateCard.modelData.broken
                                              ? templateCard.modelData.error
                                              : qsTr("%1 · %2 · %3 layouts")
                                                .arg(templateCard.modelData.source)
                                                .arg(templateCard.modelData.aspect)
                                                .arg((templateCard.modelData.layouts ?? []).length)
                                    }
                                    Label {
                                        Layout.fillWidth: true; wrapMode: Text.Wrap
                                        color: Theme.accent; font.pixelSize: Theme.fsLabel
                                        visible: (templateCard.modelData.missingFonts ?? []).length > 0
                                        text: qsTr("Not installed here: %1")
                                              .arg((templateCard.modelData.missingFonts ?? []).join(", "))
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: !!root.chosenTemplate
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label { text: root.chosen.name ?? ""; font.weight: Theme.wHeading }
                            Label {
                                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                                font.pixelSize: Theme.fsLabel
                                text: qsTr("%1 · %2 masters · %3 · typefaces: %4")
                                      .arg(root.chosen.size ?? "").arg(root.chosen.masters ?? 0)
                                      .arg((root.chosen.layouts ?? []).join(", "))
                                      .arg((root.chosen.fonts ?? []).join(", "))
                            }
                        }
                        Button { objectName: "removeTemplate"; text: qsTr("Remove")
                                 visible: (root.chosen.source ?? "") === "installed"
                                 onClicked: { backend.removeTemplate(root.chosenTemplate); root.chosenTemplate = "" } }
                        Button {
                            objectName: "useTemplate"
                            text: qsTr("Use this template"); highlighted: true
                            enabled: !(root.chosen.broken ?? false)
                            onClicked: root.templateRequested(root.chosenTemplate)
                        }
                    }
                }
            }
            Rectangle { Layout.fillHeight: true; implicitWidth: Theme.hairline; color: Theme.border }
            ColumnLayout {
                Layout.preferredWidth: Math.min(Theme.wInspector, root.width * .28)
                Layout.fillHeight: true
                spacing: Theme.s3
                Label { text: qsTr("Recent presentations"); font.pixelSize: Theme.fsTitle; font.weight: Theme.wHeading }
                Label {
                    visible: recentList.count === 0
                    Layout.fillWidth: true; wrapMode: Text.Wrap
                    text: qsTr("Saved and opened decks appear here. Pin the ones you use often.")
                    color: Theme.textMuted
                }
                ListView {
                    id: recentList
                    Layout.fillWidth: true; Layout.fillHeight: true
                    model: backend.recentFiles
                    clip: true; spacing: Theme.s2
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: recent
                        required property var modelData
                        required property int index
                        width: recentList.width
                        implicitHeight: recentContent.implicitHeight + Theme.s3 * 2
                        color: Theme.panelBg; radius: Theme.rControl
                        ColumnLayout {
                            id: recentContent
                            anchors.left: parent.left; anchors.right: parent.right
                            anchors.top: parent.top; anchors.margins: Theme.s3
                            spacing: Theme.s1
                            Button {
                                objectName: "recentDeck" + recent.index
                                Layout.fillWidth: true; flat: true
                                text: recent.modelData.name
                                enabled: !recent.modelData.missing
                                onClicked: root.recentRequested(recent.modelData.path)
                                ToolTip.visible: hovered; ToolTip.text: recent.modelData.path
                                contentItem: Label { text: parent.text; elide: Text.ElideMiddle; color: recent.modelData.missing ? Theme.textMuted : Theme.textPrimary; verticalAlignment: Text.AlignVCenter }
                            }
                            Label { visible: recent.modelData.missing; text: qsTr("File missing"); color: Theme.warning }
                            RowLayout {
                                Button { flat: true; text: recent.modelData.pinned ? qsTr("Unpin") : qsTr("Pin"); onClicked: backend.pinRecent(recent.modelData.path,!recent.modelData.pinned) }
                                Button { flat: true; text: qsTr("Reveal"); onClicked: backend.revealRecent(recent.modelData.path) }
                                Button { flat: true; text: qsTr("Remove"); onClicked: backend.removeRecent(recent.modelData.path) }
                            }
                        }
                    }
                }
            }
        }
    }
}

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

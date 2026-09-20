import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Everything the menus can do, by name. Typing narrows it; Enter runs it.
//
// Commands carry the words people actually use for them as well as their own
// label, so "film", "movie" and "video" all find the same thing.
Dialog {
    id: root
    objectName: "commandPalette"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - Theme.s5 * 2, 640)
    height: Math.min(parent.height - Theme.s5 * 2, 520)
    modal: true
    padding: 0
    property var commands: []
    property int highlighted: 0
    readonly property var matches: {
        const needle = filter.text.trim().toLowerCase()
        const rows = root.commands.filter(command => command.enabled === undefined || command.enabled)
        if (needle.length === 0) return rows.slice(0, 40)
        const words = needle.split(/\s+/)
        return rows.filter(command => {
            const hay = (command.name + " " + (command.also ?? "") + " " + command.group).toLowerCase()
            return words.every(word => hay.indexOf(word) >= 0)
        }).slice(0, 40)
    }
    function show() {
        filter.text = ""
        highlighted = 0
        open()
        filter.forceActiveFocus()
    }
    function runHighlighted() {
        const command = matches[Math.max(0, Math.min(highlighted, matches.length - 1))]
        if (!command) return
        close()
        command.run()
    }
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rMenu }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        TextField {
            id: filter
            objectName: "commandFilter"
            Layout.fillWidth: true
            Layout.margins: Theme.s3
            placeholderText: qsTr("What would you like to do?")
            onTextChanged: root.highlighted = 0
            Keys.onDownPressed: root.highlighted = Math.min(root.highlighted + 1, root.matches.length - 1)
            Keys.onUpPressed: root.highlighted = Math.max(0, root.highlighted - 1)
            Keys.onReturnPressed: root.runHighlighted()
            Keys.onEnterPressed: root.runHighlighted()
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: 0
                Label {
                    objectName: "commandNothing"
                    Layout.fillWidth: true
                    Layout.margins: Theme.s4
                    visible: root.matches.length === 0
                    wrapMode: Text.Wrap
                    color: Theme.textMuted
                    text: qsTr("Nothing here does that — try another word.")
                }
                Repeater {
                    model: root.matches
                    ItemDelegate {
                        required property var modelData
                        required property int index
                        objectName: "command" + index
                        Layout.fillWidth: true
                        highlighted: index === root.highlighted
                        onClicked: { root.close(); modelData.run() }
                        onHoveredChanged: if (hovered) root.highlighted = index
                        contentItem: RowLayout {
                            spacing: Theme.s3
                            Label {
                                Layout.preferredWidth: Theme.wFieldLabel
                                text: modelData.group
                                color: Theme.textMuted
                                font.pixelSize: Theme.fsLabel
                                elide: Text.ElideRight
                            }
                            Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight }
                            Label {
                                text: modelData.shortcut ?? ""
                                color: Theme.textMuted
                                font.family: Theme.monoFamily
                                font.pixelSize: Theme.fsLabel
                            }
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.margins: Theme.s3
            color: Theme.textMuted
            font.pixelSize: Theme.fsLabel
            text: qsTr("%1 of %2 · ↑↓ to choose · Enter to run").arg(root.matches.length).arg(root.commands.length)
        }
    }
}

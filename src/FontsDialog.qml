import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Typefaces the deck names that are not installed here, each with a
// replacement already chosen — the same family without a weight in its
// name when that is installed, else the nearest kind — and every installed
// family to choose from instead. Applying rewrites the deck; it is one undo.
Sheet {
    id: root
    objectName: "fontsDialog"
    parent: Overlay.overlay; anchors.centerIn: parent
    width: Math.min(parent.width - Theme.s5 * 2, 640)
    modal: true; title: qsTr("Typefaces not installed here")
    standardButtons: Dialog.Apply | Dialog.Cancel
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, .65) }

    property var rows: []
    property var choices: ({})
    readonly property var installed: Qt.fontFamilies()

    function show() {
        rows = backend.missingFonts()
        const chosen = {}
        for (const row of rows) chosen[row.family] = row.suggested
        choices = chosen
        open()
    }
    onOpened: standardButton(Dialog.Apply).objectName = "applyFonts"
    onApplied: {
        const map = {}
        for (const row of rows) { const pick = choices[row.family]; if (pick && pick !== "keep") map[row.family] = pick }
        backend.substituteFonts(map)
        close()
    }

    ColumnLayout {
        width: parent.width; spacing: Theme.s3
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
            text: root.rows.length === 0
                  ? qsTr("Every typeface this deck names is installed.")
                  : qsTr("The deck was designed with typefaces this computer does not have. Choose what stands in for each; \"Keep the name\" leaves the deck asking for the original, so it looks right on a computer that has it.")
        }
        Repeater {
            model: root.rows
            RowLayout {
                required property var modelData
                required property int index
                Layout.fillWidth: true; spacing: Theme.s3
                Label {
                    objectName: "missingFont" + index
                    Layout.preferredWidth: root.width * .38
                    wrapMode: Text.Wrap
                    text: modelData.family + " · " + qsTr("%n uses", "", modelData.uses)
                }
                ComboBox {
                    objectName: "fontChoice" + index
                    Layout.fillWidth: true
                    model: [qsTr("Keep the name")].concat(root.installed)
                    currentIndex: {
                        const pick = root.choices[modelData.family]
                        return pick === "keep" ? 0 : Math.max(0, model.indexOf(pick))
                    }
                    onActivated: {
                        const next = Object.assign({}, root.choices)
                        next[modelData.family] = currentIndex === 0 ? "keep" : model[currentIndex]
                        root.choices = next
                    }
                }
            }
        }
    }
}

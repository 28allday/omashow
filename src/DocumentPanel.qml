import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// The deck as a whole, beside the canvas: its theme, its size, where its
// masters are edited, the language it is written in, and how it is shown.
// The third of the sidebar's three switches, after Format and Animate.
Rectangle {
    id: root
    objectName: "documentPanel"
    color: Theme.panelBg

    signal editMastersRequested()
    signal slideSizeRequested()
    signal presenterSetupRequested()
    signal reviewRequested()

    Rectangle { anchors.left: parent.left; width: Theme.hairline; height: parent.height; color: Theme.border }

    ScrollView {
        anchors.fill: parent
        anchors.leftMargin: Theme.hairline
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width - Theme.s4 * 2
            x: Theme.s4
            spacing: Theme.s3
            Item { implicitHeight: Theme.s1 }
            Label { text: qsTr("Document"); font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading }

            SectionLabel { text: qsTr("THEME") }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: Theme.s2
                Repeater {
                    model: [qsTr("Midnight"), qsTr("Paper"), qsTr("Grove")]
                    Button {
                        required property int index
                        required property string modelData
                        objectName: "documentTheme" + index
                        Layout.fillWidth: true
                        text: modelData
                        checkable: true
                        checked: backend.design.name === modelData
                        onClicked: backend.applyTheme(index)
                    }
                }
            }
            Button {
                objectName: "editMasters"
                Layout.fillWidth: true
                icon.name: "palette"
                text: qsTr("Edit masters and layouts…")
                onClicked: root.editMastersRequested()
            }

            SectionLabel { text: qsTr("SLIDE SIZE") }
            Button {
                objectName: "documentSlideSize"
                Layout.fillWidth: true
                icon.name: "scan"
                text: qsTr("%1 × %2").arg(Math.round(backend.slideSize.width)).arg(Math.round(backend.slideSize.height))
                onClicked: root.slideSizeRequested()
            }

            SectionLabel { text: qsTr("LANGUAGE") }
            ComboBox {
                objectName: "documentLanguage"
                Layout.fillWidth: true
                readonly property var languages: backend.spellingLanguages()
                visible: languages.length > 0
                model: languages
                currentIndex: languages.indexOf(backend.deckLanguage)
                displayText: currentIndex < 0 ? qsTr("Not set") : currentText
                onActivated: backend.setDeckLanguage(currentText)
            }
            Label {
                visible: backend.spellingLanguages().length === 0
                Layout.fillWidth: true; wrapMode: Text.Wrap
                color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                text: qsTr("No dictionaries are installed, so spelling is not checked.")
            }
            CheckBox {
                objectName: "documentSmartPunctuation"
                Layout.fillWidth: true
                text: qsTr("Smart punctuation")
                checked: backend.smartPunctuation
                onToggled: backend.setSmartPunctuation(checked)
            }

            SectionLabel { text: qsTr("PRESENTING") }
            Button {
                objectName: "documentPresenterSetup"
                Layout.fillWidth: true
                icon.name: "monitor"
                text: qsTr("Displays, timings and shows…")
                onClicked: root.presenterSetupRequested()
            }

            SectionLabel { text: qsTr("CHECKING") }
            Button {
                objectName: "documentReview"
                Layout.fillWidth: true
                icon.name: "message-square-text"
                text: qsTr("Comments, findings and spelling…")
                onClicked: root.reviewRequested()
            }
            Item { implicitHeight: Theme.s3 }
        }
    }
}

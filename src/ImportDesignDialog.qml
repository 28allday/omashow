import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Slides and design from another deck. Everything here describes the import
// that is already prepared behind it, so the slide on the right is the slide
// that will arrive — not an impression of it.
Sheet {
    id: root
    objectName: "importDesignDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - Theme.s5 * 2, 1000)
    height: Math.min(parent.height - Theme.s5 * 2, 760)
    modal: true
    title: qsTr("Import from %1").arg(plan.source ?? "")
    standardButtons: Dialog.Apply | Dialog.Cancel
    readonly property var plan: backend.deckImport
    readonly property var slides: plan.slides ?? []
    readonly property var unresolved: (plan.fonts ?? []).filter(f => !f.resolved)
    property int reviewing: 0
    function show() { reviewing = 0; open() }
    onOpened: {
        standardButton(Dialog.Apply).objectName = "applyImport"
        standardButton(Dialog.Apply).text = Qt.binding(() => root.slides.length === 1
            ? qsTr("Import 1 slide") : qsTr("Import %1 slides").arg(root.slides.length))
        standardButton(Dialog.Apply).enabled = Qt.binding(() => !!root.plan.ok)
    }
    onApplied: if (backend.applyImport()) close()
    onClosed: backend.clearImport()
    Connections { target: backend; function onDeckImportChanged() { if (Object.keys(backend.deckImport).length === 0) root.close() } }

    component Heading: SectionLabel {}

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.s4
            // Which slides come across.
            ColumnLayout {
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                spacing: Theme.s2
                RowLayout {
                    Layout.fillWidth: true
                    Heading { text: qsTr("SLIDES"); Layout.fillWidth: true }
                    Button { objectName: "importSelectAll"; flat: true; text: qsTr("All"); onClicked: backend.setImportSlidesSelected(true) }
                    Button { objectName: "importSelectNone"; flat: true; text: qsTr("None"); onClicked: backend.setImportSlidesSelected(false) }
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.s1
                        Repeater {
                            model: root.plan.sourceSlides ?? []
                            RowLayout {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                spacing: Theme.s2
                                CheckBox {
                                    objectName: "importSlide" + index
                                    checked: modelData.selected
                                    onToggled: backend.setImportSlideSelected(modelData.index, checked)
                                }
                                Rectangle {
                                    implicitWidth: 74
                                    implicitHeight: 74 * backend.slideSize.height / Math.max(1, backend.slideSize.width)
                                    radius: Theme.rControl
                                    color: Theme.showBg
                                    border.width: Theme.hairline
                                    border.color: Theme.border
                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: Theme.hairline
                                        fillMode: Image.PreserveAspectFit
                                        cache: false
                                        sourceSize.width: 150
                                        source: "image://slides/import-source/" + modelData.index + "/" + (root.plan.revision ?? 0)
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label { Layout.fillWidth: true; elide: Text.ElideRight; text: modelData.title }
                                    Label {
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                        color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                                        text: modelData.layout + (modelData.skipped ? " · " + qsTr("skipped") : "")
                                    }
                                }
                            }
                        }
                    }
                }
            }
            // How it should arrive, and what that means.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.s2
                Heading { text: qsTr("DESIGN") }
                ComboBox {
                    id: design
                    objectName: "importDesign"
                    Layout.fillWidth: true
                    model: [qsTr("Reuse matching masters and layouts"),
                            qsTr("Import masters and layouts as copies"),
                            qsTr("No design · keep each slide's own look")]
                    currentIndex: root.plan.design ?? 0
                    onActivated: backend.setImportOption("design", currentIndex)
                }
                CheckBox {
                    objectName: "importTheme"
                    text: qsTr("Use that deck's theme (%1) for this whole deck").arg(root.plan.themeName ?? "")
                    checked: root.plan.importTheme ?? false
                    onToggled: backend.setImportOption("theme", checked)
                }
                CheckBox {
                    objectName: "importDropMedia"
                    visible: (root.plan.missingMedia ?? 0) > 0
                    text: qsTr("Leave out %1 linked files that are missing").arg(root.plan.missingMedia ?? 0)
                    checked: root.plan.droppedMedia > 0
                    onToggled: backend.setImportOption("dropMissingMedia", checked)
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.s2
                        Heading { text: qsTr("TYPEFACES"); visible: (root.plan.fonts ?? []).length > 0 }
                        Repeater {
                            model: root.plan.fonts ?? []
                            ColumnLayout {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                spacing: 0
                                Label {
                                    Layout.fillWidth: true; wrapMode: Text.Wrap
                                    objectName: "importFont" + index
                                    color: modelData.resolved ? Theme.textSecondary : Theme.accent
                                    text: modelData.family + " · " + (modelData.uses === 1 ? qsTr("1 use") : qsTr("%1 uses").arg(modelData.uses))
                                          + (modelData.detail ? " · " + modelData.detail : "")
                                }
                                ComboBox {
                                    objectName: "importFontChoice" + index
                                    visible: !modelData.available
                                    Layout.fillWidth: true
                                    model: [qsTr("Choose a replacement…"), qsTr("Keep the name")].concat(Qt.fontFamilies())
                                    currentIndex: modelData.substitute === "keep" ? 1
                                        : Math.max(0, model.indexOf(modelData.substitute))
                                    onActivated: backend.setImportOption("font/" + modelData.family,
                                        currentIndex === 0 ? "" : currentIndex === 1 ? "keep" : model[currentIndex])
                                }
                            }
                        }
                        Heading { text: qsTr("MASTERS AND LAYOUTS"); visible: (root.plan.masters ?? []).length + (root.plan.layouts ?? []).length > 0 }
                        Repeater {
                            model: (root.plan.masters ?? []).concat(root.plan.layouts ?? [])
                            Label {
                                required property var modelData
                                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                                text: (modelData.action === "reuse" ? qsTr("Reuse %1") : qsTr("Import %1")).arg(modelData.name)
                                      + " · " + modelData.detail
                            }
                        }
                        Heading { text: qsTr("PICTURES, FILM AND DATA"); visible: (root.plan.media ?? []).length > 0 }
                        Repeater {
                            model: root.plan.media ?? []
                            Label {
                                required property var modelData
                                Layout.fillWidth: true; wrapMode: Text.Wrap
                                color: modelData.missing && !modelData.dropped ? Theme.accent : Theme.textSecondary
                                text: modelData.name + " · " + modelData.kind + " · " + modelData.detail
                            }
                        }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            visible: (root.plan.sections ?? []).length > 0
                            text: qsTr("New sections: %1").arg((root.plan.sections ?? []).join(", "))
                        }
                    }
                }
            }
            // The slide as it will arrive.
            ColumnLayout {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                spacing: Theme.s2
                Heading { text: qsTr("ARRIVING") }
                ComboBox {
                    objectName: "importReviewSlide"
                    Layout.fillWidth: true
                    visible: root.slides.length > 1
                    model: root.slides
                    textRole: "name"
                    currentIndex: Math.min(root.reviewing, Math.max(0, root.slides.length - 1))
                    onActivated: root.reviewing = currentIndex
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: width * backend.slideSize.height / Math.max(1, backend.slideSize.width)
                    color: Theme.pasteboard
                    radius: Theme.rCard
                    border.width: Theme.hairline
                    border.color: Theme.border
                    Image {
                        objectName: "importPreview"
                        anchors.fill: parent
                        anchors.margins: Theme.s2
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        sourceSize.width: 600
                        source: !root.plan.ok || root.slides.length === 0 ? ""
                            : "image://slides/import/" + (root.slides[Math.min(root.reviewing, root.slides.length - 1)]?.index ?? 0)
                              + "/" + (root.plan.revision ?? 0)
                    }
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                    text: root.slides.length === 0 ? "" : qsTr("%1 · %2 objects")
                        .arg(root.slides[Math.min(root.reviewing, root.slides.length - 1)]?.layout ?? "")
                        .arg(root.slides[Math.min(root.reviewing, root.slides.length - 1)]?.objects ?? 0)
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.accent
                    text: (root.slides[Math.min(root.reviewing, root.slides.length - 1)]?.issues ?? []).join("\n")
                }
                Item { Layout.fillHeight: true }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    text: qsTr("Slides arrive after the current one. Colours and fonts linked to the theme follow this deck unless that deck's theme is used. The whole import is one undo step.")
                }
            }
        }
        Label {
            objectName: "importError"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            visible: !root.plan.ok
            color: Theme.textPrimary
            text: root.plan.error ?? ""
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.textMuted
            text: qsTr("%1 of %2 slides · %3 at %4 · %5 of pictures, film and data")
                .arg(root.plan.selectedCount ?? 0).arg((root.plan.sourceSlides ?? []).length)
                .arg(root.plan.scaled ? qsTr("scaled from %1").arg(root.plan.sourceSize ?? "") : qsTr("the same slide size"))
                .arg(backend.slideSize.width + " × " + backend.slideSize.height)
                .arg(Math.round((root.plan.bytes ?? 0) / 1024) + " kB")
        }
    }
}

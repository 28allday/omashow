import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

Sheet {
    id: root
    objectName: "layoutApplyDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - Theme.s5 * 2, 940)
    height: Math.min(parent.height - Theme.s5 * 2, 740)
    modal: true
    title: qsTr("Apply layout")
    standardButtons: Dialog.Apply | Dialog.Cancel
    property var mapping: ({})
    readonly property var preview: backend.layoutPreview
    readonly property var slides: preview.slides ?? []
    readonly property var slide: slides[Math.max(0, previewSlide.currentIndex)] ?? ({})
    readonly property var layouts: backend.design.layouts.map(l => ({
        id: l.id, name: (backend.design.masters.find(m => m.id === l.masterId)?.name ?? "") + " / " + l.name
    }))
    function refresh() {
        backend.previewLayoutAsync(layouts[target.currentIndex]?.id ?? "", scope.currentIndex, mapping, geometry.currentIndex)
    }
    function show(layoutId) {
        mapping = ({})
        target.currentIndex = Math.max(0, layouts.findIndex(l => l.id === layoutId))
        scope.currentIndex = backend.selectedSlides.length > 1 ? 1 : 0
        geometry.currentIndex = 0
        previewSlide.currentIndex = 0
        refresh()
        open()
    }
    onOpened: {
        standardButton(Dialog.Apply).objectName = "applyLayoutPreview"
        standardButton(Dialog.Apply).text = Qt.binding(() => qsTr("Apply to %1 slides").arg(root.slides.length))
        standardButton(Dialog.Apply).enabled = Qt.binding(() => !!root.preview.ok)
    }
    onApplied: if (backend.applyLayoutPreview()) close()
    onClosed: backend.clearLayoutPreview()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.s3
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Master / layout"); color: Theme.textSecondary }
                ComboBox {
                    id: target; objectName: "layoutTarget"; Layout.fillWidth: true
                    model: root.layouts; textRole: "name"
                    onActivated: { root.mapping = ({}); root.refresh() }
                }
            }
            ColumnLayout {
                Layout.preferredWidth: 210
                Label { text: qsTr("Apply to"); color: Theme.textSecondary }
                ComboBox {
                    id: scope; objectName: "layoutScope"; Layout.fillWidth: true
                    model: [qsTr("Current slide"), qsTr("Selected slides (%1)").arg(backend.selectedSlides.length), qsTr("All slides (%1)").arg(backend.slideCount)]
                    onActivated: { previewSlide.currentIndex = 0; root.refresh() }
                }
            }
        }
        ComboBox {
            id: geometry; objectName: "layoutGeometry"; Layout.fillWidth: true
            model: [qsTr("Follow layout · keep local position edits"), qsTr("Reapply all layout positions"), qsTr("Keep every object's current position")]
            onActivated: root.refresh()
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
            text: qsTr("Content, local styles and builds are kept. Unmatched objects remain editable in their current positions. Empty targets receive new placeholders.")
        }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            contentWidth: availableWidth; clip: true
            ColumnLayout {
                width: parent.width; spacing: Theme.s3
                SectionLabel { text: qsTr("PLACEHOLDER MAPPING"); visible: (root.preview.mappings ?? []).length > 0 }
                Repeater {
                    model: root.preview.mappings ?? []
                    RowLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true; Layout.preferredWidth: 1
                            text: modelData.name; elide: Text.ElideRight
                            ToolTip.text: text; ToolTip.visible: mappingHover.hovered
                            HoverHandler { id: mappingHover }
                        }
                        ComboBox {
                            objectName: "layoutMapping" + index
                            Layout.fillWidth: true; Layout.preferredWidth: 1
                            model: modelData.choices; textRole: "name"
                            currentIndex: model.findIndex(c => c.id === modelData.target)
                            onActivated: {
                                const next = Object.assign({}, root.mapping)
                                next[modelData.key] = model[currentIndex].id
                                root.mapping = next
                                root.refresh()
                            }
                        }
                    }
                }
                Label {
                    objectName: "layoutError"; Layout.fillWidth: true
                    visible: !root.preview.ok; text: root.preview.error ?? ""
                    color: Theme.textPrimary; wrapMode: Text.Wrap
                }
                Button { visible: !root.preview.ok; text: qsTr("Refresh preview"); onClicked: root.refresh() }
                RowLayout {
                    visible: !!root.preview.ok; Layout.fillWidth: true
                    SectionLabel { text: qsTr("REVIEW SLIDES"); Layout.fillWidth: true }
                    ComboBox {
                        id: previewSlide; objectName: "layoutPreviewSlide"; Layout.preferredWidth: 180
                        model: root.slides; textRole: "name"
                    }
                }
                RowLayout {
                    visible: !!root.preview.ok; Layout.fillWidth: true
                    Repeater {
                        model: [qsTr("Before"), qsTr("After")]
                        ColumnLayout {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true; Layout.preferredWidth: 1
                            Label { text: modelData; color: Theme.textSecondary }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: Math.min(210, root.width * 0.22)
                                color: Theme.pasteboard; radius: Theme.rControl
                                Image {
                                    anchors.fill: parent; anchors.margins: Theme.s2
                                    fillMode: Image.PreserveAspectFit; sourceSize.width: 800; cache: false
                                    source: !root.preview.ok ? "" : index === 0
                                        ? "image://slides/" + root.slide.index + "/" + (backend.navigator[root.slide.index]?.stamp ?? "")
                                        : "image://slides/layout-apply/" + root.slide.index + "/" + root.preview.revision
                                }
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true; visible: !!root.preview.ok; wrapMode: Text.Wrap
                    text: qsTr("%1 moved · %2 kept independent · %3 new placeholders").arg(root.slide.moved ?? 0).arg(root.slide.independent ?? 0).arg(root.slide.created ?? 0)
                    color: Theme.textSecondary
                }
                Label {
                    Layout.fillWidth: true; visible: !!root.preview.ok; wrapMode: Text.Wrap
                    text: (root.slide.issues ?? []).join("\n"); color: Theme.textPrimary
                }
            }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
            text: qsTr("%1 slides in this preview · %2 with text overflow · %3 with content outside the slide. Apply is one undo step.")
                .arg(root.slides.length).arg(root.slides.filter(s => s.overflow > 0).length).arg(root.slides.filter(s => s.outside > 0).length)
        }
    }
}

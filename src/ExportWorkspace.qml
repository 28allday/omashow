import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// What the deck can become, and what is being made right now. Each export is
// queued against the deck as it stands, so carrying on editing is safe.
Item {
    id: root
    objectName: "exportWorkspace"
    readonly property var queue: backend.exportQueue
    // Finding printers can take seconds; ask once, when this page first opens.
    Component.onCompleted: backend.refreshPrinters()
    readonly property int slides: backend.slideCount

    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }
    component ExportCard: Card {
        default property alias body: holder.data
        property alias spacing: holder.spacing
        Layout.fillWidth: true
        implicitHeight: holder.implicitHeight + Theme.s4 * 2
        ColumnLayout {
            id: holder
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Theme.s2
            spacing: Theme.s3
        }
    }
    component CardHeader: RowLayout {
        property alias title: heading.text
        property alias detail: sub.text
        property alias icon: glyph.name
        spacing: Theme.s3
        Rectangle {
            implicitWidth: Theme.hToolTile - Theme.s3; implicitHeight: implicitWidth; radius: Theme.rCard
            color: Theme.withAlpha(Theme.accent, 0.12); border.color: Theme.withAlpha(Theme.accent, 0.4)
            Icon { id: glyph; anchors.centerIn: parent; size: Theme.szIconLarge + 4; color: Theme.accent }
        }
        ColumnLayout {
            spacing: 2
            Label { id: heading; font.pixelSize: Theme.fsSection; font.weight: Theme.wHeading }
            Label { id: sub; Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary }
        }
    }

    // The slide range every export shares.
    function range(all, first, last) {
        return all ? ({ from: -1, to: -1 })
                   : ({ from: Math.min(first, last) - 1, to: Math.max(first, last) - 1 })
    }

    ScrollView {
        objectName: "exportScroll"
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: parent.width
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.maximumWidth: Theme.wInspector * 2.6
                Layout.margins: Theme.s5
                spacing: Theme.s3

                Label { text: qsTr("Export"); font.pixelSize: Theme.fsStartHeading; font.weight: Theme.wHeading }
                Label {
                    text: qsTr("%1 · %2 slides").arg(backend.fileName).arg(root.slides)
                    color: Theme.textSecondary; Layout.bottomMargin: Theme.s3
            }

            ExportCard {
                CardHeader { title: qsTr("PDF document"); icon: "file-text"
                             detail: qsTr("Real text, not outlines — searchable and quotable.") }
                Divider {}
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Pages"); color: Theme.textSecondary; Layout.preferredWidth: Theme.wFieldLabel }
                    ComboBox {
                        id: pdfLayout
                        objectName: "pdfLayout"
                        Layout.fillWidth: true
                        model: [qsTr("The slides"), qsTr("Slides with their notes"),
                                qsTr("An outline"), qsTr("Several slides a sheet")]
                    }
                    ComboBox {
                        id: pdfPerPage
                        objectName: "pdfPerPage"
                        visible: pdfLayout.currentIndex === 3
                        model: [2, 3, 4, 6, 9]
                        currentIndex: 2
                    }
                }
                CheckBox { id: stagePages; objectName: "pdfStages"; visible: pdfLayout.currentIndex === 0
                           text: qsTr("A page per build stage (handout)") }
                CheckBox { id: pdfSkipped; objectName: "pdfSkipped"; text: qsTr("Include skipped slides") }
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox { id: pdfAll; objectName: "pdfAllSlides"; text: qsTr("Every slide"); checked: true }
                    Label { visible: !pdfAll.checked; text: qsTr("From"); color: Theme.textSecondary }
                    SpinBox { id: pdfFrom; objectName: "pdfFrom"; visible: !pdfAll.checked; from: 1; to: root.slides; value: 1 }
                    Label { visible: !pdfAll.checked; text: qsTr("to"); color: Theme.textSecondary }
                    SpinBox { id: pdfTo; objectName: "pdfTo"; visible: !pdfAll.checked; from: 1; to: root.slides; value: root.slides }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "exportPdf"
                        icon.name: "file-output"; text: qsTr("Export PDF…"); highlighted: true
                        onClicked: {
                            const r = root.range(pdfAll.checked, pdfFrom.value, pdfTo.value)
                            backend.exportDialog({ kind: 0, stages: stagePages.checked,
                                                   layout: pdfLayout.currentIndex,
                                                   perPage: pdfPerPage.model[pdfPerPage.currentIndex],
                                                   includeSkipped: pdfSkipped.checked,
                                                   from: r.from, to: r.to })
                        }
                    }
                }
            }

            ExportCard {
                CardHeader { title: qsTr("Pictures"); icon: "file-image"
                             detail: qsTr("One file per slide, rendered by the same painter as the show.") }
                Divider {}
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Format"); color: Theme.textSecondary; Layout.preferredWidth: Theme.wFieldLabel }
                    ComboBox { id: imageFormat; objectName: "imageFormat"; Layout.fillWidth: true
                               model: [qsTr("PNG · lossless"), qsTr("JPEG · smaller")] }
                    Label { text: qsTr("Width"); color: Theme.textSecondary }
                    SpinBox { id: imageWidth; objectName: "imageWidth"; from: 160; to: 7680; stepSize: 160; value: 1920; editable: true }
                }
                CheckBox { id: transparent; objectName: "imageTransparent"; text: qsTr("Leave the background out (PNG only)")
                           enabled: imageFormat.currentIndex === 0 }
                CheckBox { id: imageSkipped; objectName: "imageSkipped"; text: qsTr("Include skipped slides") }
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox { id: imageAll; objectName: "imageAllSlides"; text: qsTr("Every slide"); checked: true }
                    Label { visible: !imageAll.checked; text: qsTr("From"); color: Theme.textSecondary }
                    SpinBox { id: imageFrom; objectName: "imageFrom"; visible: !imageAll.checked; from: 1; to: root.slides; value: 1 }
                    Label { visible: !imageAll.checked; text: qsTr("to"); color: Theme.textSecondary }
                    SpinBox { id: imageTo; objectName: "imageTo"; visible: !imageAll.checked; from: 1; to: root.slides; value: root.slides }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "exportImages"
                        icon.name: "file-image"; text: qsTr("Export pictures…")
                        onClicked: {
                            const r = root.range(imageAll.checked, imageFrom.value, imageTo.value)
                            backend.exportDialog({ kind: 1, format: imageFormat.currentIndex,
                                                   width: imageWidth.value, transparent: transparent.checked,
                                                   includeSkipped: imageSkipped.checked,
                                                   from: r.from, to: r.to })
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    text: qsTr("More than one slide is numbered from the name you choose: deck-01.png, deck-02.png. Each slide is drawn with its builds finished.")
                }
            }

            ExportCard {
                CardHeader { title: qsTr("Film"); icon: "file-play"
                             detail: backend.encoderAvailable
                                     ? qsTr("Every build, transition and hold, frame by frame, as H.264.")
                                     : qsTr("Install ffmpeg to encode film on this computer.") }
                Divider {}
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Width"); color: Theme.textSecondary; Layout.preferredWidth: Theme.wFieldLabel }
                    SpinBox { id: filmWidth; objectName: "filmWidth"; from: 320; to: 3840; stepSize: 160; value: 1920; editable: true }
                    Label { text: qsTr("Frames a second"); color: Theme.textSecondary }
                    ComboBox { id: filmFps; objectName: "filmFps"; model: [24, 25, 30, 60]; currentIndex: 2 }
                    Label { text: qsTr("Quality"); color: Theme.textSecondary }
                    ComboBox { id: filmQuality; objectName: "filmQuality"; model: [qsTr("Balanced"), qsTr("High")] }
                }
                CheckBox { id: filmSkipped; objectName: "filmSkipped"; text: qsTr("Include skipped slides") }
                RowLayout {
                    Layout.fillWidth: true
                    CheckBox { id: filmAll; objectName: "filmAllSlides"; text: qsTr("Every slide"); checked: true }
                    Label { visible: !filmAll.checked; text: qsTr("From"); color: Theme.textSecondary }
                    SpinBox { id: filmFrom; objectName: "filmFrom"; visible: !filmAll.checked; from: 1; to: root.slides; value: 1 }
                    Label { visible: !filmAll.checked; text: qsTr("to"); color: Theme.textSecondary }
                    SpinBox { id: filmTo; objectName: "filmTo"; visible: !filmAll.checked; from: 1; to: root.slides; value: root.slides }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "exportFilm"
                        icon.name: "file-play"; text: qsTr("Export film…")
                        enabled: backend.encoderAvailable
                        onClicked: {
                            const r = root.range(filmAll.checked, filmFrom.value, filmTo.value)
                            backend.exportDialog({ kind: 2, width: filmWidth.value,
                                                   fps: filmFps.model[filmFps.currentIndex],
                                                   quality: filmQuality.currentIndex,
                                                   includeSkipped: filmSkipped.checked,
                                                   from: r.from, to: r.to })
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    text: qsTr("Sound from film and audio on the slides is not included yet. The picture is frame-exact: the same evaluator draws it as drives the show.")
                }
            }

            ExportCard {
                CardHeader { title: qsTr("Print"); icon: "file-text"
                             detail: !backend.printersKnown ? qsTr("Looking for printers…")
                                     : backend.printers.length > 0
                                     ? qsTr("The same pages as the PDF, on paper.")
                                     : qsTr("No printer is set up on this computer.") }
                Divider {}
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Printer"); color: Theme.textSecondary; Layout.preferredWidth: Theme.wFieldLabel }
                    ComboBox {
                        id: printer
                        objectName: "printerChoice"
                        Layout.fillWidth: true
                        textRole: "name"
                        valueRole: "name"
                        model: backend.printers
                        currentIndex: Math.max(0, backend.printers.findIndex(p => p.default))
                    }
                    Label { text: qsTr("Copies"); color: Theme.textSecondary }
                    SpinBox { id: copies; objectName: "printCopies"; from: 1; to: 99; value: 1 }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Pages"); color: Theme.textSecondary; Layout.preferredWidth: Theme.wFieldLabel }
                    ComboBox {
                        id: printLayout
                        objectName: "printLayout"
                        Layout.fillWidth: true
                        model: [qsTr("The slides"), qsTr("Slides with their notes"),
                                qsTr("An outline"), qsTr("Several slides a sheet")]
                        currentIndex: 3
                    }
                    ComboBox {
                        id: printPerPage
                        objectName: "printPerPage"
                        visible: printLayout.currentIndex === 3
                        model: [2, 3, 4, 6, 9]
                        currentIndex: 2
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "printDeck"
                        icon.name: "file-text"; text: qsTr("Print")
                        enabled: backend.printers.length > 0
                        onClicked: backend.exportDialog({ kind: 4, printer: printer.currentValue ?? "",
                                                          copies: copies.value,
                                                          layout: printLayout.currentIndex,
                                                          perPage: printPerPage.model[printPerPage.currentIndex],
                                                          includeSkipped: false })
                    }
                }
            }

            ExportCard {
                CardHeader { title: qsTr("Package"); icon: "folder-open"
                             detail: qsTr("The deck with copies of everything it links to, and a manifest.") }
                Divider {}
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                    text: qsTr("Film small enough to live inside the deck is brought in, so the package opens complete elsewhere. Linked files you have not approved for reading stay out and are named in the manifest. Nothing on this computer is changed.")
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button {
                        objectName: "exportPackage"
                        icon.name: "save"; text: qsTr("Package deck…")
                        onClicked: backend.exportDialog({ kind: 3 })
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.s3
                SectionLabel { text: qsTr("EXPORTS"); Layout.fillWidth: true }
                Button {
                    objectName: "clearFinishedExports"
                    flat: true; text: qsTr("Clear finished")
                    visible: root.queue.some(job => job.finished)
                    onClicked: backend.clearFinishedExports()
                }
            }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                visible: root.queue.length === 0
                text: qsTr("Nothing has been exported this session.")
            }
            Repeater {
                model: root.queue
                Rectangle {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    implicitHeight: jobBody.implicitHeight + Theme.s3 * 2
                    radius: Theme.rCard
                    color: Theme.panelBg
                    border.width: Theme.hairline
                    border.color: modelData.state === "failed" ? Theme.accent : Theme.border
                    ColumnLayout {
                        id: jobBody
                        anchors.fill: parent
                        anchors.margins: Theme.s3
                        spacing: 2
                        RowLayout {
                            Layout.fillWidth: true
                            Label { objectName: "exportName" + index; text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label {
                                objectName: "exportState" + index
                                color: modelData.state === "done" ? Theme.textSecondary
                                     : modelData.state === "failed" ? Theme.accent : Theme.textMuted
                                text: modelData.state === "running" ? qsTr("%1%").arg(modelData.progress)
                                    : modelData.state === "queued" ? qsTr("waiting")
                                    : modelData.state
                            }
                        }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            text: modelData.detail + " · " + modelData.folder
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            visible: modelData.state === "running"
                            from: 0; to: 100; value: modelData.progress
                        }
                        Label {
                            objectName: "exportMessage" + index
                            Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.fsLabel
                            visible: modelData.message.length > 0 || modelData.log.length > 0
                            color: modelData.state === "failed" ? Theme.accent : Theme.textSecondary
                            text: [modelData.message].concat(modelData.log).filter(line => line.length > 0).join("\n")
                        }
                        RowLayout {
                            Button {
                                objectName: "cancelExport" + index
                                flat: true; text: qsTr("Cancel")
                                visible: !modelData.finished
                                onClicked: backend.cancelExport(modelData.id)
                            }
                            Button {
                                objectName: "retryExport" + index
                                flat: true; text: qsTr("Try again")
                                visible: modelData.finished && modelData.state !== "done"
                                onClicked: backend.retryExport(modelData.id)
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
            Item { implicitHeight: Theme.s5 }
            }
        }
    }
}

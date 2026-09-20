import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// Reading the deck rather than building it: the outline on the left, findings
// and search in the middle, and everything about the current slide — comments,
// notes, reading order and descriptions — on the right.
RowLayout {
    id: root
    objectName: "reviewWorkspace"
    spacing: 0
    readonly property var stats: backend.statistics
    readonly property var issues: backend.reviewIssues
    property string severity: ""
    property bool showDismissed: false
    readonly property var shownIssues: issues.filter(i => (root.showDismissed || !i.dismissed)
                                                       && (root.severity === "" || i.severity === root.severity))
    property string replyingTo: ""
    function clock(seconds) {
        return seconds < 90 ? qsTr("%1 seconds").arg(Math.round(seconds))
                            : qsTr("%1 minutes").arg((seconds / 60).toFixed(1))
    }

    component Heading: SectionLabel {}
    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: Theme.hairline; color: Theme.border }

    // --- the deck, as words --------------------------------------------------
    Rectangle {
        // The three columns have to fit the smallest window the app allows, so
        // they are allowed to shrink: without fillWidth a column is fixed at its
        // preferred width and the shell would be wider than the window.
        Layout.preferredWidth: Theme.wNavigator + Theme.s5 * 2
        Layout.minimumWidth: Theme.wNavigatorMin
        Layout.maximumWidth: Theme.wNavigator + Theme.s5 * 2
        Layout.fillWidth: true
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
                Heading { text: qsTr("OUTLINE") }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted
                    font.pixelSize: Theme.fsLabel
                    text: qsTr("Editing here changes the words on the slide and nothing else.")
                }
                Repeater {
                    model: backend.outline
                    ColumnLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 2
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Slide %1").arg(modelData.index + 1)
                                color: modelData.index === backend.currentSlide ? Theme.accent : Theme.textMuted
                                font.pixelSize: Theme.fsLabel
                            }
                            Label {
                                visible: modelData.comments > 0
                                text: "· " + qsTr("%n comments", "", modelData.comments)
                                color: Theme.accent; font.pixelSize: Theme.fsLabel
                            }
                            Label {
                                visible: modelData.skipped
                                text: "· " + qsTr("skipped"); color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            }
                            Item { Layout.fillWidth: true }
                        }
                        TextField {
                            objectName: "outlineTitle" + index
                            Layout.fillWidth: true
                            text: modelData.title
                            enabled: !!modelData.titleId
                            placeholderText: modelData.titleId ? qsTr("Title") : qsTr("This slide has no text to edit")
                            onActiveFocusChanged: if (activeFocus) backend.currentSlide = modelData.index
                            onEditingFinished: backend.setOutlineText(modelData.slideId, modelData.titleId, text)
                        }
                        TextArea {
                            objectName: "outlineBody" + index
                            Layout.fillWidth: true
                            visible: !!modelData.bodyId
                            text: modelData.body
                            wrapMode: TextArea.Wrap
                            textFormat: TextEdit.PlainText
                            onActiveFocusChanged: if (activeFocus) backend.currentSlide = modelData.index
                            onEditingFinished: backend.setOutlineText(modelData.slideId, modelData.bodyId, text)
                        }
                        Divider {}
                    }
                }
            }
        }
    }

    // --- findings and search --------------------------------------------------
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: Theme.wInspectorMin
        Layout.margins: Theme.s4
        spacing: Theme.s3
        TabBar {
            id: centreTabs
            objectName: "reviewTabs"
            Layout.fillWidth: true
            TabButton { text: qsTr("Findings") }
            TabButton { text: qsTr("Find and replace") }
            TabButton { text: qsTr("Statistics") }
        }

        // Findings
        ColumnLayout {
            visible: centreTabs.currentIndex === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.s2
            Flow {
                Layout.fillWidth: true
                spacing: Theme.s2
                Repeater {
                    model: [{ key: "", label: qsTr("Everything") }, { key: "must", label: qsTr("Must fix") },
                            { key: "should", label: qsTr("Should fix") }, { key: "info", label: qsTr("Worth knowing") }]
                    Button {
                        required property var modelData
                        objectName: "issueFilter" + modelData.key
                        text: modelData.label + " · " + (modelData.key === ""
                              ? root.issues.filter(i => root.showDismissed || !i.dismissed).length
                              : root.issues.filter(i => i.severity === modelData.key && (root.showDismissed || !i.dismissed)).length)
                        checked: root.severity === modelData.key
                        onClicked: root.severity = modelData.key
                    }
                }
                CheckBox {
                    objectName: "showDismissedIssues"
                    text: qsTr("Include ignored")
                    checked: root.showDismissed
                    onToggled: root.showDismissed = checked
                }
                Button { objectName: "exportReview"; text: qsTr("Export review…"); icon.name: "file-output"
                         onClicked: backend.exportReviewDialog() }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: Theme.s2
                    Label {
                        Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                        visible: root.shownIssues.length === 0
                        text: root.issues.length === 0
                              ? qsTr("Nothing to raise: every picture has a description, the text has contrast, and each slide has a title.")
                              : qsTr("Nothing matches this filter.")
                    }
                    Repeater {
                        model: root.shownIssues
                        Rectangle {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            implicitHeight: issueBody.implicitHeight + Theme.s3 * 2
                            radius: Theme.rCard
                            color: Theme.panelBg
                            border.width: Theme.hairline
                            border.color: Theme.border
                            ColumnLayout {
                                id: issueBody
                                anchors.fill: parent
                                anchors.margins: Theme.s3
                                spacing: 2
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        objectName: "issueTitle" + index
                                        Layout.fillWidth: true; wrapMode: Text.Wrap
                                        text: modelData.title
                                        color: modelData.dismissed ? Theme.textMuted
                                             : modelData.severity === "must" ? Theme.accent : Theme.textPrimary
                                    }
                                    Label {
                                        text: qsTr("Slide %1").arg(modelData.slide + 1)
                                        color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                                    }
                                }
                                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                                        font.pixelSize: Theme.fsLabel; text: modelData.detail }
                                RowLayout {
                                    Button { objectName: "issueGo" + index; flat: true; text: qsTr("Show me")
                                             onClicked: backend.goToIssue(modelData.key) }
                                    Button {
                                        objectName: "issueDismiss" + index
                                        flat: true
                                        text: modelData.dismissed ? qsTr("Raise it again") : qsTr("Not a problem")
                                        onClicked: backend.dismissIssue(modelData.key, !modelData.dismissed)
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Find and replace
        ColumnLayout {
            id: findTab
            visible: centreTabs.currentIndex === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.s2
            function search() {
                backend.findText(needle.text, { caseSensitive: matchCase.checked, wholeWords: wholeWords.checked,
                                                notes: searchNotes.checked, scope: scope.currentIndex })
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: needle
                    objectName: "findField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Find in this deck")
                    onTextChanged: findTab.search()
                }
                TextField {
                    id: replacement
                    objectName: "replaceField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Replace with")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                CheckBox { id: matchCase; objectName: "findCase"; text: qsTr("Match case"); onToggled: findTab.search() }
                CheckBox { id: wholeWords; objectName: "findWholeWords"; text: qsTr("Whole words"); onToggled: findTab.search() }
                CheckBox { id: searchNotes; objectName: "findNotes"; text: qsTr("Include notes"); checked: true; onToggled: findTab.search() }
                ComboBox {
                    id: scope
                    objectName: "findScope"
                    Layout.preferredWidth: 200
                    model: [qsTr("Whole deck"), qsTr("Selected slides"), qsTr("This slide")]
                    onActivated: findTab.search()
                }
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "replaceAll"
                    text: qsTr("Replace all")
                    enabled: (backend.findState.count ?? 0) > 0 && replacement.text.length > 0
                    onClicked: backend.replaceAllMatches(replacement.text)
                }
            }
            Label {
                objectName: "findSummary"
                Layout.fillWidth: true; color: Theme.textSecondary
                text: needle.text.length === 0 ? qsTr("Type to search slides, tables and notes.")
                    : qsTr("%n matches", "", backend.findState.count ?? 0)
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: parent.width
                    spacing: 2
                    Repeater {
                        model: backend.findState.matches ?? []
                        RowLayout {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("Slide %1").arg(modelData.slide + 1) + " · " + modelData.label
                                color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                                Layout.preferredWidth: 160
                            }
                            Label {
                                objectName: "findMatch" + index
                                Layout.fillWidth: true; elide: Text.ElideRight; text: modelData.context
                            }
                            Button { flat: true; objectName: "findGo" + index; text: qsTr("Show me")
                                     onClicked: backend.goToMatch(index) }
                            Button {
                                flat: true; objectName: "findReplace" + index; text: qsTr("Replace")
                                enabled: replacement.text.length > 0
                                onClicked: backend.replaceMatch(index, replacement.text)
                            }
                        }
                    }
                }
            }
        }

        // Statistics
        ScrollView {
            visible: centreTabs.currentIndex === 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.s2
                Heading { text: qsTr("THIS DECK") }
                Repeater {
                    model: [
                        { label: qsTr("Slides"), value: root.stats.slides + (root.stats.skipped > 0 ? qsTr(" · %1 skipped").arg(root.stats.skipped) : "") },
                        { label: qsTr("Sections"), value: root.stats.sections },
                        { label: qsTr("Words"), value: root.stats.words + qsTr(" · %1 in notes").arg(root.stats.noteWords ?? 0) },
                        { label: qsTr("Text boxes"), value: root.stats.textBoxes },
                        { label: qsTr("Pictures"), value: root.stats.pictures },
                        { label: qsTr("Film and sound"), value: (root.stats.films ?? 0) + " · " + (root.stats.sounds ?? 0) },
                        { label: qsTr("Tables and charts"), value: (root.stats.tables ?? 0) + " · " + (root.stats.charts ?? 0) },
                        { label: qsTr("Shapes"), value: root.stats.shapes },
                        { label: qsTr("Builds"), value: root.stats.builds },
                        { label: qsTr("Slides with notes"), value: root.stats.notes },
                        { label: qsTr("Comments"), value: (root.stats.comments ?? 0) + qsTr(" · %1 open").arg(root.stats.openComments ?? 0) },
                        { label: qsTr("Embedded pictures and film"), value: Math.round((root.stats.assetBytes ?? 0) / 1024) + " kB" },
                        { label: qsTr("Runs for"), value: root.clock(root.stats.duration ?? 0) + qsTr(" with every build and transition") },
                        { label: qsTr("Without skipped slides"), value: root.clock(root.stats.showDuration ?? 0) }
                    ]
                    FieldRow {
                        required property var modelData
                        label: modelData.label
                        Label { objectName: "stat_" + modelData.label; Layout.fillWidth: true; wrapMode: Text.Wrap
                                text: modelData.value; color: Theme.textSecondary }
                    }
                }
                Heading { text: qsTr("TYPEFACES") }
                Label {
                    objectName: "statFonts"
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textSecondary
                    text: (root.stats.fonts ?? []).join(", ")
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.accent
                    visible: (root.stats.missingFonts ?? []).length > 0
                    text: qsTr("Not installed here: %1").arg((root.stats.missingFonts ?? []).join(", "))
                }
            }
        }
    }

    // --- this slide -------------------------------------------------------------
    Rectangle {
        Layout.preferredWidth: Theme.wInspector
        Layout.minimumWidth: Theme.wInspectorMin
        Layout.maximumWidth: Theme.wInspector
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Theme.panelBg
        Rectangle { anchors.left: parent.left; width: Theme.hairline; height: parent.height; color: Theme.border; z: 1 }
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            TabBar {
                id: slideTabs
                objectName: "reviewSlideTabs"
                Layout.fillWidth: true
                TabButton { text: qsTr("Comments") }
                TabButton { text: qsTr("Notes") }
                TabButton { text: qsTr("Reading") }
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

                    // Comments
                    ColumnLayout {
                        visible: slideTabs.currentIndex === 0
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        Heading { text: qsTr("SLIDE %1").arg(backend.currentSlide + 1) }
                        FieldRow {
                            label: qsTr("You are")
                            TextField {
                                objectName: "reviewAuthor"
                                Layout.fillWidth: true
                                text: backend.reviewAuthor
                                placeholderText: qsTr("Your name")
                                onEditingFinished: backend.reviewAuthor = text
                            }
                        }
                        TextArea {
                            id: newComment
                            objectName: "newComment"
                            Layout.fillWidth: true
                            placeholderText: backend.selectionCount === 1
                                ? qsTr("Comment on the selected object")
                                : qsTr("Comment on this slide")
                            wrapMode: TextArea.Wrap
                            textFormat: TextEdit.PlainText
                        }
                        Button {
                            objectName: "addComment"
                            Layout.fillWidth: true
                            text: backend.selectionCount === 1 ? qsTr("Comment on the selection") : qsTr("Comment on the slide")
                            enabled: newComment.text.trim().length > 0
                            onClicked: if (backend.addComment(newComment.text, backend.selectionCount === 1)) newComment.clear()
                        }
                        Repeater {
                            model: backend.comments.filter(c => c.slide === backend.currentSlide)
                            ColumnLayout {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                spacing: 2
                                Divider {}
                                Label {
                                    Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.fsLabel
                                    color: Theme.textMuted
                                    text: (modelData.author.length > 0 ? modelData.author : qsTr("Someone"))
                                          + " · " + modelData.created.split("T")[0]
                                          + (modelData.objectId.length > 0 ? " · " + qsTr("on an object") : "")
                                          + (modelData.resolved ? " · " + qsTr("resolved") : "")
                                }
                                Label {
                                    objectName: "comment" + index
                                    Layout.fillWidth: true; wrapMode: Text.Wrap; text: modelData.text
                                    color: modelData.resolved ? Theme.textMuted : Theme.textPrimary
                                }
                                Repeater {
                                    model: modelData.replies
                                    Label {
                                        required property var modelData
                                        Layout.fillWidth: true; wrapMode: Text.Wrap; leftPadding: Theme.s4
                                        color: Theme.textSecondary
                                        text: "↳ " + (modelData.author.length > 0 ? modelData.author + ": " : "") + modelData.text
                                    }
                                }
                                RowLayout {
                                    Button { flat: true; objectName: "commentShow" + index; text: qsTr("Show me")
                                             onClicked: backend.goToComment(modelData.id) }
                                    Button { flat: true; objectName: "commentReply" + index; text: qsTr("Reply")
                                             onClicked: root.replyingTo = root.replyingTo === modelData.id ? "" : modelData.id }
                                    Button {
                                        flat: true; objectName: "commentResolve" + index
                                        text: modelData.resolved ? qsTr("Reopen") : qsTr("Resolve")
                                        onClicked: backend.resolveComment(modelData.id, !modelData.resolved)
                                    }
                                    Button { flat: true; objectName: "commentDelete" + index; text: qsTr("Delete")
                                             onClicked: backend.removeComment(modelData.id) }
                                }
                                RowLayout {
                                    visible: root.replyingTo === modelData.id
                                    Layout.fillWidth: true
                                    TextField {
                                        id: reply
                                        objectName: "replyField" + index
                                        Layout.fillWidth: true
                                        placeholderText: qsTr("Reply")
                                        // Replying rebuilds this list, so the delegate is
                                        // gone by the time the call returns: tidy up first.
                                        onAccepted: { const words = text; const thread = modelData.id
                                                      root.replyingTo = ""; backend.replyToComment(thread, words) }
                                    }
                                    Button {
                                        objectName: "replySend" + index
                                        text: qsTr("Send")
                                        enabled: reply.text.trim().length > 0
                                        onClicked: { const words = reply.text; const thread = modelData.id
                                                     root.replyingTo = ""; backend.replyToComment(thread, words) }
                                    }
                                }
                            }
                        }
                    }

                    // Notes
                    ColumnLayout {
                        visible: slideTabs.currentIndex === 1
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        Heading { text: qsTr("SPEAKER NOTES") }
                        TextArea {
                            objectName: "reviewNotes"
                            Layout.fillWidth: true
                            Layout.minimumHeight: Theme.hRow * 8
                            text: backend.slideNotes
                            wrapMode: TextArea.Wrap
                            textFormat: TextEdit.PlainText
                            placeholderText: qsTr("Notes for this slide — only you see them while presenting.")
                            onEditingFinished: if (text !== backend.slideNotes) backend.setSlideNotes(text)
                        }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            text: qsTr("Notes are plain text: they show in the presenter console and are searched by Find. They are not printed yet.")
                        }
                    }

                    // Reading order and descriptions
                    ColumnLayout {
                        visible: slideTabs.currentIndex === 2
                        Layout.fillWidth: true
                        spacing: Theme.s2
                        Heading { text: qsTr("READ IN THIS ORDER") }
                        Repeater {
                            model: backend.readingOrder
                            ColumnLayout {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                spacing: 2
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        objectName: "reading" + index
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                        text: (index + 1) + ". " + modelData.summary
                                        color: modelData.hidden ? Theme.textMuted : Theme.textPrimary
                                    }
                                    Button { flat: true; objectName: "readingUp" + index; icon.name: "arrow-up"
                                             display: AbstractButton.IconOnly; text: qsTr("Earlier")
                                             onClicked: backend.moveReadingOrder(modelData.id, -1) }
                                    Button { flat: true; objectName: "readingDown" + index; icon.name: "arrow-down"
                                             display: AbstractButton.IconOnly; text: qsTr("Later")
                                             onClicked: backend.moveReadingOrder(modelData.id, 1) }
                                }
                                TextField {
                                    objectName: "altText" + index
                                    visible: modelData.describable
                                    Layout.fillWidth: true
                                    text: modelData.alt
                                    placeholderText: qsTr("Describe this for someone who cannot see it")
                                    onEditingFinished: backend.describeObject(modelData.id, modelData.altTitle, text)
                                }
                            }
                        }
                        Button {
                            objectName: "resetReadingOrder"
                            Layout.fillWidth: true
                            text: qsTr("Read in stacking order")
                            enabled: (backend.readingOrder[0]?.custom ?? false)
                            onClicked: backend.resetReadingOrder()
                        }
                        Label {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.textMuted; font.pixelSize: Theme.fsLabel
                            text: qsTr("Reading order is kept with the slide and used by the findings here. PDF export does not carry it yet.")
                        }
                    }
                }
            }
        }
    }
}

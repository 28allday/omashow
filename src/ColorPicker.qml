import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Omashow 1.0

// COLOR-PICK-1: the colour chooser behind every colour swatch.
// Two tabs: Swatches (a grid of shades of each main colour, plus greys) and
// Wheel (any colour: hue and strength on the wheel, brightness below). The hex
// of what is chosen shows at the bottom, editable, before "Use colour".
// A colour's transparency (#AARRGGBB) is kept as it was.
Sheet {
    id: root
    objectName: "colorPicker"
    parent: Overlay.overlay; anchors.centerIn: parent
    modal: true
    title: qsTr("Choose a colour")
    background: Rectangle { color: Theme.panelBg; border.color: Theme.border; radius: Theme.rControl }
    Overlay.modal: Rectangle { color: Theme.withAlpha(Theme.showBg, .65) }

    signal chosen(string hex)

    // What the field held, and what is chosen now (hue 0-1, strength, brightness).
    property string original: ""
    property real hue: 0
    property real sat: 0
    property real val: 1
    readonly property string alphaPart: /^#[0-9a-fA-F]{8}$/.test(original) ? original.substr(1, 2) : ""
    readonly property color current: Qt.hsva(hue, sat, val, 1)
    readonly property string currentHex: hex(current)

    function hex(c) {
        const two = v => ("0" + Math.round(v * 255).toString(16)).slice(-2)
        return "#" + two(c.r) + two(c.g) + two(c.b)
    }
    function take(c) {
        // A grey has no hue of its own: keep the wheel where it was.
        if (c.hsvSaturation > 0) root.hue = Math.max(0, c.hsvHue)
        root.sat = c.hsvSaturation; root.val = c.hsvValue
    }
    function openWith(value) {
        root.original = value || ""
        const valid = /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(root.original)
        take(valid ? Qt.color(root.original) : Qt.color("#ffffff"))
        tabs.currentIndex = 0
        open()
    }
    function accept() {
        root.chosen(root.alphaPart ? "#" + root.alphaPart + root.currentHex.substr(1) : root.currentHex)
        close()
    }

    // Swatches: 10 colours across, 8 shades down from light to dark; greys on top.
    readonly property var hues: [0, 0.07, 0.14, 0.33, 0.47, 0.53, 0.6, 0.67, 0.78, 0.9]
    readonly property var shades: [0.92, 0.82, 0.71, 0.6, 0.5, 0.4, 0.3, 0.2]

    ColumnLayout {
        spacing: Theme.s3

        TabBar {
            id: tabs
            objectName: "colorPickerTabs"
            Layout.fillWidth: true
            TabButton { text: qsTr("Swatches") }
            TabButton { text: qsTr("Wheel") }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.preferredWidth: grid.implicitWidth
            Layout.preferredHeight: grid.implicitHeight

            Grid {
                id: grid
                objectName: "colorSwatches"
                columns: root.hues.length
                spacing: 3
                Repeater {
                    // Row 0 greys (white to black), then each shade row.
                    model: root.hues.length * (root.shades.length + 1)
                    Rectangle {
                        required property int index
                        readonly property int row: Math.floor(index / root.hues.length)
                        readonly property int column: index % root.hues.length
                        readonly property color swatch: row === 0
                            ? Qt.hsla(0, 0, 1 - column / (root.hues.length - 1), 1)
                            : Qt.hsla(root.hues[column], 0.85, root.shades[row - 1], 1)
                        width: 26; height: 26; radius: 3
                        color: swatch
                        border.width: root.hex(swatch) === root.currentHex ? 2 : Theme.hairline
                        border.color: root.hex(swatch) === root.currentHex ? Theme.accent : Theme.border
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: root.take(parent.swatch)
                            onDoubleClicked: { root.take(parent.swatch); root.accept() }
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: Theme.s2
                // The wheel: angle is the hue, distance from the centre the
                // strength. Drawn once; brightness darkens it from above.
                Item {
                    id: wheel
                    objectName: "colorWheel"
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 200; implicitHeight: 200
                    readonly property real radius: width / 2
                    Canvas {
                        anchors.fill: parent
                        // Two gradients, not a colour per pixel: hue around the circle
                        // (anticlockwise from the right, as pick() reads it), then white
                        // fading out towards the edge, which is the strength.
                        onPaint: {
                            const ctx = getContext("2d"), r = width / 2
                            ctx.reset()
                            const hues = ctx.createConicalGradient(r, r, 0)
                            for (let i = 0; i <= 6; ++i) hues.addColorStop(i / 6, Qt.hsva((i % 6) / 6, 1, 1, 1))
                            const strength = ctx.createRadialGradient(r, r, 0, r, r, r)
                            strength.addColorStop(0, Qt.rgba(1, 1, 1, 1))
                            strength.addColorStop(1, Qt.rgba(1, 1, 1, 0))
                            for (const fill of [hues, strength]) {
                                ctx.fillStyle = fill
                                ctx.beginPath()
                                ctx.arc(r, r, r, 0, 2 * Math.PI)
                                ctx.fill()
                            }
                        }
                    }
                    Rectangle { anchors.fill: parent; radius: width / 2; color: "black"; opacity: 1 - root.val }
                    Rectangle {
                        width: 14; height: 14; radius: 7
                        color: "transparent"; border.width: 2; border.color: root.val > 0.5 ? "black" : "white"
                        x: wheel.radius + Math.cos(root.hue * 2 * Math.PI) * root.sat * wheel.radius - 7
                        y: wheel.radius - Math.sin(root.hue * 2 * Math.PI) * root.sat * wheel.radius - 7
                    }
                    MouseArea {
                        anchors.fill: parent
                        function pick(mx, my) {
                            const dx = mx - wheel.radius, dy = wheel.radius - my
                            let h = Math.atan2(dy, dx) / (2 * Math.PI); if (h < 0) h += 1
                            root.hue = h
                            root.sat = Math.min(1, Math.sqrt(dx * dx + dy * dy) / wheel.radius)
                        }
                        onPressed: mouse => pick(mouse.x, mouse.y)
                        onPositionChanged: mouse => { if (pressed) pick(mouse.x, mouse.y) }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("Brightness"); color: Theme.textSecondary }
                    Slider {
                        objectName: "colorBrightness"
                        Layout.fillWidth: true
                        from: 0; to: 1
                        value: root.val
                        onMoved: root.val = value
                    }
                }
            }
        }

        // What is chosen, before it is used: the colour it had, the new one, its hex.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            Rectangle {
                implicitWidth: Theme.hControl; implicitHeight: Theme.hControl; radius: Theme.rControl
                color: /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(root.original) ? root.original : "transparent"
                border.color: Theme.borderStrong; border.width: Theme.hairline
                ToolTip.visible: oldHover.hovered; ToolTip.text: qsTr("The colour it has now")
                HoverHandler { id: oldHover }
            }
            Rectangle {
                implicitWidth: Theme.hControl * 2; implicitHeight: Theme.hControl; radius: Theme.rControl
                color: root.current
                border.color: Theme.borderStrong; border.width: Theme.hairline
            }
            TextField {
                id: hexField
                objectName: "colorPickerHex"
                Layout.fillWidth: true
                font.family: Theme.monoFamily
                text: root.currentHex
                Accessible.name: qsTr("Colour hex code")
                // Typing a full code moves the swatch and the wheel to it.
                onTextEdited: if (/^#[0-9a-fA-F]{6}$/.test(text)) root.take(Qt.color(text))
                onAccepted: root.accept()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button { objectName: "colorPickerCancel"; text: qsTr("Cancel"); onClicked: root.close() }
            Button { objectName: "colorPickerUse"; text: qsTr("Use colour"); highlighted: true; onClicked: root.accept() }
        }
    }
}

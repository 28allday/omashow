pragma Singleton

import QtQuick
import Omashow 1.0

// The single source of every colour, dimension, radius, duration and type size
// in OmaShow. Nothing outside this file may contain a colour literal or a magic
// dimension. The palette is the shared Oma design system — the same family as
// OmaEdit, OmaAudio, OmaRAW, OmaVector and OmaImage — and it is FIXED: an
// editing surface stays colour-stable whatever the desktop is doing, because a
// surround that changes hue changes how the work inside it reads. In a deck app
// that matters twice over: the slide on the canvas is the thing being judged,
// and interface tokens must never tint slide content or exported pixels.
//
// The one exception is the ACCENT, which follows the Omarchy theme. Buttons,
// selection, focus and the active workspace underline are chrome and belong to
// the desktop. Slide content is not chrome and never takes an interface colour.
QtObject {
    id: theme

    readonly property string appName: "OmaShow"

    // ── Colour ──────────────────────────────────────────────────────────────
    readonly property color windowBg: "#0B0E13"
    readonly property color panelBg: "#111722"
    readonly property color panelRaised: "#171E2A"
    readonly property color controlBg: "#1B2431"
    readonly property color controlHover: "#222D3D"
    readonly property color border: "#293241"
    readonly property color borderStrong: "#3A4658"

    readonly property color textPrimary: "#E7EDF7"
    readonly property color textSecondary: "#A7B1C1"
    readonly property color textMuted: "#727E90"

    // The desktop's accent when Omarchy is there, the Oma accent when it is not
    // (plain Arch, a container, CI) — and the app has to look finished either
    // way, so the fallback is the colour this always used.
    readonly property color omaAccent: "#27C2FF"
    readonly property color accent: OmarchyTheme.accentFollowed ? OmarchyTheme.accent : omaAccent
    // ⚠️ Derived, not a literal: a hand-picked shade of the Oma cyan means
    // nothing against an arbitrary accent.
    readonly property color accentPressed: Qt.darker(accent, 1.35)
    readonly property color aiAccent: "#8B5CF6"
    readonly property color warning: "#FBBF24"
    readonly property color danger: "#EF4444"
    readonly property color success: "#34D399"

    // ⚠️ Derived: an accent can be pale gold or deep violet and the label on top
    // has to read on both, so Rec.709 luminance decides near-black or near-white.
    readonly property color accentText:
        (0.2126 * accent.r + 0.7152 * accent.g + 0.0722 * accent.b) > 0.55 ? "#04121A" : "#F2F7FF"

    function hovered(base) { return Qt.lighter(base, 1.06) }
    function pressedOn(base) { return Qt.darker(base, 1.08) }
    function withAlpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
    readonly property real disabledOpacity: 0.38

    // ── Editing surfaces ────────────────────────────────────────────────────
    // The pasteboard the slide floats on: darker than the panels so the slide
    // reads as the object and the surround as nothing.
    readonly property color pasteboard: "#1A1F26"
    readonly property color showBg: "#000000"
    readonly property color selectionOutline: accent
    readonly property color selectionFill: withAlpha(accent, 0.10)
    readonly property color handleBg: windowBg
    readonly property color guide: accent
    readonly property color safeArea: warning

    // ── Spacing ─────────────────────────────────────────────────────────────
    // The brief's scale: 4, 8, 12, 16, 24. Named by step so call sites read as
    // intent rather than arithmetic.
    readonly property int s1: Math.round(4 * densityScale)
    readonly property int s2: Math.round(8 * densityScale)
    readonly property int s3: Math.round(12 * densityScale)
    readonly property int s4: Math.round(16 * densityScale)
    readonly property int s5: Math.round(24 * densityScale)

    // ── Radius ──────────────────────────────────────────────────────────────
    readonly property int rControl: 4
    readonly property int rMenu: 6
    readonly property int rCard: 6
    readonly property int rPanel: 0
    readonly property int rHandle: 2

    // ── Lines ───────────────────────────────────────────────────────────────
    readonly property int hairline: 1
    readonly property int focusRing: 1
    readonly property int selectionRing: 2
    readonly property int activeUnderline: 2

    // ── Density ─────────────────────────────────────────────────────────────
    // compact (default) | normal | comfortable. Scales spacing and control
    // heights only — never type size.
    property string density: "compact"
    readonly property real densityScale: density === "comfortable" ? 1.25
                                       : density === "normal" ? 1.12
                                       : 1.0

    // ── Type ────────────────────────────────────────────────────────────────
    readonly property string fontFamily: {
        const families = Qt.fontFamilies()
        if (families.indexOf("Inter") >= 0) return "Inter"
        if (families.indexOf("Noto Sans") >= 0) return "Noto Sans"
        return "sans-serif"
    }
    // Numeric readouts want tabular digits so values do not jitter as they change.
    readonly property string monoFamily: {
        const families = Qt.fontFamilies()
        if (families.indexOf("Inter") >= 0) return "Inter"
        if (families.indexOf("Noto Sans Mono") >= 0) return "Noto Sans Mono"
        return "monospace"
    }

    readonly property int fsStartTitle: 26
    readonly property int fsStartHeading: 22
    readonly property int hTemplateCard: 224
    readonly property int wTemplatePreview: 640
    readonly property int fsBase: 12
    readonly property int fsLabel: 12
    readonly property int fsControl: 13
    readonly property int fsHeading: 13
    readonly property int fsTitle: 15
    readonly property int fsSection: 14
    readonly property int fsPresenterNotes: 18
    readonly property int fsPresenterClock: 30
    readonly property color whiteout: "#ffffff"
    readonly property int fsCaption: 10
    readonly property int wHeading: Font.DemiBold
    readonly property int wNormal: Font.Normal
    readonly property real capsTracking: 0.8

    // ── Control metrics ─────────────────────────────────────────────────────
    readonly property int hTitleBar: 34
    readonly property int hWorkspaceBar: 40
    readonly property int hToolbar: Math.round(48 * densityScale)
    readonly property int hStatusBar: 28
    readonly property int hControl: Math.round(28 * densityScale)
    readonly property int hToolButton: Math.round(34 * densityScale)
    readonly property int hRow: Math.round(30 * densityScale)
    readonly property int hTab: Math.round(40 * densityScale)
    readonly property int hCardHeader: Math.round(36 * densityScale)
    readonly property int wFieldLabel: 84
    readonly property int hNotes: Math.round(76 * densityScale)
    readonly property int hNavHeader: Math.round(36 * densityScale)
    readonly property int hTransport: Math.round(76 * densityScale)
    readonly property int wNavigator: 248
    readonly property int wNavigatorMin: 180
    readonly property int wInspector: 320
    readonly property int wInspectorMin: 300
    readonly property int szHandle: 10
    readonly property int szHandleHit: 6
    readonly property int szIcon: 16
    readonly property int szIconLarge: 20
    readonly property int szIconHit: Math.round(28 * densityScale)
    readonly property int hToolTile: Math.round(58 * densityScale)
    readonly property int szStatusDot: 8
    readonly property int wSplitter: 4
    readonly property int nudge: 8
    // How close a drag has to come, in screen pixels, before it snaps.
    readonly property int snapPixels: 8

    readonly property int wAudienceWindow: 960
    readonly property int hAudienceWindow: 540
    readonly property int wPresenterWindow: 1280
    readonly property int hPresenterWindow: 800
    readonly property int wPresenterMinimum: 1000
    readonly property int wPresenterSide: 460
    readonly property int hPresenterMinimum: 650
    readonly property int hTimeline: 300
    readonly property int hTrack: 30
    readonly property int hRuler: 26
    readonly property int wTrackLabel: 180

    // ── Motion ──────────────────────────────────────────────────────────────
    readonly property int dFast: 100
    readonly property int dNormal: 130
    readonly property int dSlow: 160
    readonly property int easing: Easing.OutCubic
    readonly property int tooltipDelay: 450
}

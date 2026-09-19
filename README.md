# OmaShow

A native Qt 6 / QML presentation app for Omarchy: keyboard-first and offline.
The app opens a Start centre with theme previews, slide-size presets and recent files.

## What works

- **Start:** Midnight, Paper and Grove previews; widescreen, standard, portrait,
  square and custom sizes; title, body or blank first slide. Recent decks can be
  pinned, revealed or removed, with missing files clearly marked.
- **Edit:** text, native shapes and pictures; on-slide text editing, snap guides,
  resize, rotation handles (Shift snaps to 15°), numeric properties, undo/redo, multi-selection, nested groups,
  alignment/distribution, layer order, lock and hide. Presenter notes are edited
  under the slide (View → Presenter notes). Copy/cut/paste/duplicate
  preserve appearance, groups, animation targets and embedded pictures.
- **Canvas:** fit slide or selection, 100%, 10–800% zoom, pointer-centred Ctrl+wheel zoom, ordinary
  wheel/trackpad pan and middle-button drag. Zoom does not change the deck.
- **Text:** font family, size, weight, italic, underline and colour; horizontal
  and vertical alignment, line/paragraph spacing, indent, bullets and numbering.
  Leading tabs nest list items. Clip with an overflow warning, shrink to fit,
  or resize the box to fit its text.
- **Shapes:** 24 basic shapes, arrows, callouts, flowchart shapes and symbols;
  solid, linear/radial gradient, pattern and picture fills; stroke width, dash,
  join and cap; opacity and offset shadows. Saved object styles copy appearance
  without replacing content or geometry. Arrange → Combine shapes previews
  union, intersection, subtraction and divided overlaps, with undo.
- **Connectors and links:** select two objects and use Arrange → Connect objects.
  Straight, elbow and curved connectors follow attached sides through moves and
  animation. Detach into a path for free editing. Object links support web/email,
  stable slide targets, next/previous, first/last and end-show actions. External
  links pause the show and require a private presenter confirmation before opening.
- **Drawing:** Pen creates point paths; Freehand creates smooth curves. Finish or
  close a path, convert a shape to a path, drag anchors/control points, remove
  nodes, and open/close paths. Esc cancels; a drag is one undo step.
- **Pictures:** insert PNG/JPEG/WebP or static self-contained SVG using the toolbar,
  drag-and-drop or clipboard; fit/fill/stretch, editable source crop with eight on-slide handles, focal point,
  shape masks, reset and replace. Raster colour profiles convert to sRGB;
  brightness, contrast, saturation and tint are reversible. Replacement retains
  crop, frame, effects and builds. SVG remains vector content, including in PDF;
  unsupported SVG scripts, animation and external resources are rejected. Pictures are embedded
  in the deck and reach the canvas, slideshow, thumbnails, PNG and PDF exports.
- **Audio and video:** Media → Insert embeds a clip or links its local file.
  Codec, duration, poster and portability appear in the inspector. Trim, volume,
  one to 100 plays and automatic/on-next-click cues share the presentation
  timeline. Preview opens Animate; Stop returns to the cue start. Video frames
  are decoded at the requested time; audio follows pause, seek and playback rate.
  Presenter blank/freeze suppresses audio. Media preflight approves, locates,
  relinks or embeds linked files. Opening a deck resets linked-file permission.
  Import runs in the background with progress/cancel and one undo step.
  Supported containers: MP4/MOV, MKV/WebM, WAV, MP3, FLAC and Ogg, with locally
  available codecs. Clips need a known duration up to 24 hours; video is SDR up
  to 4096 × 4096. Embedding is limited to 64 MiB per clip; linking supports local
  files up to 2 GiB. HDR/wide-gamut video requires prior conversion to SDR.
- **Optimise assets:** compare original and compressed pictures/video before
  applying. JPEG/PNG picture presets preserve transparency when using PNG;
  H.264/AAC media presets show size, codec and playback quality. Keep original
  enables Restore after reload; discard it to reduce deck size. Jobs cancel,
  and apply/restore/discard undo together. Crop, effects and timing are retained.
- **Tables:** native editable cells, row/column insertion/deletion and sizing,
  merge/split, header row/column, alternating rows, cell fills/borders/padding,
  font/alignment and overflow/shrink controls. Double-click a table to edit.
  Keyboard ranges, copy/cut/paste/clear and undo; CSV/TSV text/file import previews
  before applying. Stable text/numeric sorting supports UK/German/French locales.
  Formulas remain literal text. Tables stay on one slide, up to 100 × 50 cells.
  Native accessibility exposes header relationships, positions and merged spans.
- **Charts:** ten native chart types, embedded cell editor, missing-value gaps,
  negative stacks, log scales, axis bounds/titles, locale-aware number formats
  and theme or per-series colours. Charts remain editable after save/reopen,
  keep their appearance when copied between themes, and export as vector PDF.
- **Linked CSV/TSV:** opt in when importing table or chart data. The editor
  shows its source path, previews whole-grid refresh, flags local data edits and
  supports disconnect/undo. Saved values remain available offline; opening a
  deck never reads linked data files automatically.
- **Diagrams:** preview and insert native process or hierarchy diagrams in
  either direction. Shapes, labels and attached connectors remain grouped and
  editable; outline validation and text-fit checks catch unreadable layouts.
- **Design:** Midnight, Paper and Grove theme previews; editable colour/font
  tokens; master management; text/shape placeholder layouts; layout changes
  that preserve content and local edits. Reset position and style separately.
- **Animate:** Fade and Rise builds in/out, individual or selected objects
  together, order, exact start/delay/duration/easing and click/with/after
  triggers. Drag timing clips, trim their ends, scrub, zoom and preview.
- **Slides:** range/toggle/select-all, batch duplicate/delete, multi-slide drag
  and keyboard reorder. Thumbnail, compact and outline navigation share the
  selection with the large Sorter workspace. Named sections travel with whole
  section selections; notes, builds, pictures and skipped states have badges.
  Duplicated objects retain the identities used by Morph.
- **Sections:** create, rename, select, move up/down, collapse/expand and remove
  while keeping slides. Use the arrow and **⋯** beside a section name; Actions
  also offers collapse/expand all. Collapsed summaries show hidden selection
  counts and share state between the navigator and sorter. Collapse is a session
  preference: it does not dirty the deck or change the show/export sequence.
- **Slide size:** Arrange → Slide size previews proportional fit/centre or
  unchanged content size/position. Applying resizes every slide, master and
  layout in one undo step.
- **Skip slides:** Slides → Actions → Skip in show keeps slides editable while
  presentation, PNG and PDF exports omit them. PDF's Include skipped slides
  checkbox and the CLI's `--include-skipped` explicitly include them.
- **Present:** separate audience and presenter windows, current/next previews,
  speaker notes, slide navigator, click-group navigation, elapsed/countdown,
  black/white and freeze. Named display routing, swapping and windowed rehearsal.
- **Files:** atomic `.omashow` saves, autosave recovery, PDF export with real
  text, build-stage handouts and headless PNG frame rendering.

Template-pack installation, recording, charts, interchange imports and
other release features remain on the roadmap. Text formatting applies to whole
boxes; per-character styles and advanced typography remain planned.

## Shared design and time

The document stores authored content, theme tokens, masters, layouts and local
property overrides. One resolver supplies the editor, thumbnails, animation and
export. Theme changes update linked properties; literal local colours stay put.
Layout changes retain unmatched objects as editable content. Copied objects keep
their resolved appearance and become independent of the original placeholders.
New text and shapes follow the current theme's font and colour tokens.

A visual state is a pure function of document time. Playback, scrubbing and PNG
export use the same evaluator. Morph matches stable object identities, then
similarity, and fades unmatched objects. Slide backgrounds blend too.

On-click groups wait for the speaker in Present. Preview and export use their
resolved timing. Ordinary PDF pages show authored content, including objects
with build-outs; build-stage handouts show the animation states.

## Build and run

Requirements: Qt 6 (`qt6-base`, `qt6-declarative`, `qt6-svg`, `qt6-multimedia`),
FFmpeg development libraries (`libavformat`, `libavcodec`, `libavutil`,
`libswscale`), zlib, a C++17 compiler, make, pkg-config and `qmake6`. Tests also
use the `ffmpeg` executable and `pdftotext`, `pdfimages` and `pdftoppm` from Poppler. WebP
requires its Qt image handler. Media optimisation also needs `ffmpeg` with
H.264 (`libx264`) and AAC encoders.

```sh
./bin/build
./build/omashow
./build/omashow talk.omashow
```

### Install as a package

On Arch / Omarchy, `./bin/install` builds the working tree and installs it
with `makepkg -si`, pulling in the runtime dependencies. OmaShow then appears
in the app launcher, opens `.omashow` files, and uninstalls with
`sudo pacman -R omashow`.

```sh
git clone https://github.com/28allday/omashow.git
cd omashow
./bin/install
```

## Keys

| Key | Action |
| --- | --- |
| `Ctrl+O` / `Ctrl+S` / `Ctrl+Shift+S` | Open / Save / Save As |
| `Ctrl+Shift+N` | Start centre (also available through Home) |
| `Ctrl+N` | Add slide |
| `Ctrl+D` | Duplicate selected objects, or the current slide when no objects are selected |
| `Ctrl+C` / `Ctrl+X` / `Ctrl+V` | Copy / cut / paste; text fields retain their own clipboard actions |
| `Ctrl++` / `Ctrl+-` | Zoom in / out |
| `Ctrl+0` / `Ctrl+1` | Fit slide / 100% |
| `Ctrl+Shift+0` | Fit selected objects |
| Shift-click / Ctrl-click in Slides or Sorter | Range / toggle selection |
| `Ctrl+A` / `Ctrl+D` / Delete in Slides or Sorter | Select all / duplicate / delete selected slides |
| `Ctrl+wheel` / middle-button drag | Zoom around pointer / pan |
| `Ctrl+Shift+↑` / `Ctrl+Shift+↓` | Reorder selected slides |
| `Ctrl+←` / `Ctrl+→` in Slides or Sorter | Collapse / expand current section |
| `Ctrl+Alt+↑` / `Ctrl+Alt+↓` in Slides or Sorter | Move current section up / down |
| `T` / `S` | Add text / shape in Edit |
| `Shift-click` / drag empty canvas | Extend selection / box-select |
| `Alt-click` | Select behind another object |
| `Ctrl+A` | Select all editable objects |
| `Ctrl+G` / `Ctrl+Shift+G` | Group / ungroup |
| `Enter` / `Escape` | Enter / leave a group |
| Arrow keys | Nudge in Edit, step time in Animate |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / redo |
| `F5` | Start show |
| `Space` / `→` / `PgDown` during show | Finish current build / advance cue |
| `←` / `PgUp` during show | Previous cue / slide |
| `B` / `W` / `F` on audience output | Black / white / freeze |
| `Escape` during show | End show |
| `Ctrl+E` | Export PDF |

## Presenter displays

Select the audience output in Present. With two displays, the console opens
on the other output. On one display, full-screen mode shows slides only;
**Rehearse in windows** also opens the console.

Display routing and swapping pass a two-screen Qt simulation. A physical
Hyprland test with the desktop and Acer display is still required. This
session exposed only a fallback output. Hot-plug behavior also needs that
hardware check.

## Headless export

```sh
./build/omashow talk.omashow --shot build/frames --times "0,0.5,1.0" --width 1920
./build/omashow talk.omashow --pdf build/talk.pdf
./build/omashow talk.omashow --pdf build/handout.pdf --stages
./build/omashow talk.omashow --pdf build/all-slides.pdf --include-skipped
./build/omashow talk.omashow --write build/copy.omashow
./build/omashow --at 3.05
```

Without a positional deck, headless exports use the built-in Morph fixture.

The native format is version 11, adding explicit local CSV links and cached data
to the existing picture, media, shape and playback fields. Versions 1–10 still
open; versions 1 and 2 retain their original text appearance. Older app builds
refuse version 11 to preserve its features.
Identical image assets are stored once under `assets/`, named by a content hash;
moving the original files cannot break a deck. PDF images use lossless encoding.

## Verification

```sh
./bin/test-all
```

This builds the app and runs core, persistence, export, QML interaction and
presentation tests, simulated two-display routing, and the 13-frame rendering
harness. The current checkpoint passes 149 tests. UI tests run offscreen and mock notification/idle changes. Captured
screens are in `build/qa/`; 100% reference captures for tables and charts are in `build/qa/table-scale-100/` and `build/qa/chart-scale-100/`.
All thirteen QML interaction flows also pass at 125% scaling.

## Licence

OmaShow is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE) for the full text, and
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for the work it builds on.

Copyright (C) 2026 Gavin Nugent.

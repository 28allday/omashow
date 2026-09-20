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
- **Text:** font family, size, weight, italic, underline and colour for a whole
  box — or for any stretch of it: select words while editing on the slide and
  the Text inspector formats just those, including strike-through and raised or
  lowered baselines. It says when the selection is not all the same. Stretches
  follow their letters when the words are rewritten, and survive saving,
  reopening, export and text builds. Horizontal
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
  decode on workers during playback; exports decode the exact requested time.
  Audio follows pause, seek and playback rate.
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
  tokens with contrast feedback; master management; text/shape placeholder layouts.
  Apply layouts to the current, selected or all slides with placeholder mapping,
  before/after previews and overflow/displacement warnings. Keep local position
  edits, reapply layout positions or keep every current position. The whole batch
  undoes together. Reset position and style separately.
- **Master fields:** Design → Fields controls slide numbers, a saved date and
  footer text, the starting number and first-slide visibility. Numbers follow
  deck order, including skipped slides. Fields use the theme's body font and
  muted colour in a footer strip that follows slide size. In Edit, deselect
  objects to toggle master artwork or fields for that slide, independently of
  its background colour. Fields stay as selectable text in PDF.
- **Import from another deck:** File → Import from deck, or Design → Import from
  deck, brings slides across with their masters, layouts, theme and content.
  Choose the slides, then reuse matching masters and layouts, import copies of
  them, or keep each slide's own appearance with no design at all. Missing
  typefaces must be replaced or explicitly kept, and missing linked files left
  out, before anything is inserted. A different slide size is scaled to this
  deck, sections arrive by name, and the whole import is one undo step. The
  preview on the right is the slide that will arrive.
- **Unused design:** Design → Unused lists masters no layout points at, layouts
  no slide uses, empty sections and originals kept from optimising a picture or
  film, with what each would give back. Removing the ones you pick is one undo
  step and never changes what a slide looks like.
- **Animate:** builds in and out — fade, rise, move from an offset, scale, spin,
  an emphasis that swells and settles in place, text that arrives a paragraph,
  a word or a character at a time, and travel along a path. A path is any shape
  or pen drawing on the same slide: draw the route, hide it, and the object
  follows it — editing its nodes changes the motion, and the object still lands
  where you placed it. It can travel the other way, and turn as it goes. Individual or selected objects
  together, order, exact start/delay/duration/easing and click/with/after
  triggers. Drag timing clips, trim their ends, scrub, zoom and preview.
- **Transitions:** cut, fade, push (in any of four directions) or morph, chosen
  per slide or handed to the whole deck, with their own duration. A slide can
  also move on by itself after a set number of seconds; blanking, freezing or
  pausing holds it where it is. Preview, present and export read the same clock,
  so a transition looks the same in all three.
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
- **Review:** the deck as words rather than pictures. The outline edits each
  slide's title and body in place; comments attach to a slide or to one object,
  take replies, resolve and reopen, and travel inside the deck with who wrote
  them and when. Find and replace covers slides, tables, chart data and notes,
  with whole-word, case and scope options, and refuses a match whose words have
  changed since it was found. Findings cover missing descriptions, text contrast
  below WCAG 2.2, type that is too small, text that does not fit, slides with no
  title, links that do not say where they go and a reading order that disagrees
  with the layout; each can be gone to, or set aside as not a problem — a
  decision the deck remembers. Reading order and alternative text are editable
  per slide, and statistics count slides, words, pictures, film, typefaces and
  how long the deck runs. The findings and open comments export as a plain-text
  review.
- **Drawing over the show:** a pointer the audience can follow, a spotlight that
  dims everything else, or freehand ink — with undo, clear, and an explicit
  "keep on the slide" that turns the drawing into an ordinary editable path.
  Nothing drawn changes the deck unless you keep it, and ending the show forgets
  it.
- **Rehearsing:** rehearse in a window and each slide is timed. The times are
  listed against what the slides do now, and can be applied in one undo step so
  each slide moves on by itself after the time it took — or discarded.
- **Custom shows:** named orders of the slides you already have. Choose one and
  presenting, previewing and exporting all follow it; choose the whole deck
  again and everything goes back. A show is an order, never a copy, and deleting
  a slide simply takes it out of the show.
- **Present:** separate audience and presenter windows, current/next previews,
  speaker notes, slide navigator, click-group navigation, elapsed/countdown,
  black/white and freeze. Named display routing, swapping and windowed rehearsal.
- **Export:** PDF with real text — the slides, slides with their notes, the deck
  as an outline, or several slides a sheet — and build-stage handouts; pictures as PNG or
  JPEG at any width, with or without the slide background; and film as H.264,
  every build, transition and hold rendered frame by frame by the same
  evaluator that drives the show. Each export takes the deck as it stands,
  queues behind the last one, shows its progress and can be cancelled or tried
  again. Sound is not in the film yet, and the export says so. The same pages
  print on paper, and a deck can be packaged as a zip carrying copies of
  everything it links to plus a manifest of what is inside, what was left out
  and why — nothing on your computer is changed by packaging.
- **Shell:** Ctrl+K finds any command by name or by the words you would use for
  it. The slide list and the inspector can be dragged wider, collapsed with a
  double-click on their edge and put back where they were next time. A deck
  opened in another window is another copy of the app, with its own selection,
  undo, playback and export queue.
- **Paste special:** keep what was copied, match this deck's text style, take
  the words only, or flatten it into a picture.
- **Accessibility:** the interface names itself to assistive technology — the
  canvas says which slide it is showing and what is selected, slide rows say
  what is special about them, workspaces are tabs and the status line is
  readable text. "Less movement" takes the animation out of the interface and
  makes a show arrive at each moment instead of travelling to it; "Stronger
  contrast" firms up edges and quiet text. Both are yours, not the deck's: a
  file saved with them on is the same file.
- **Files:** atomic `.omashow` saves, autosave recovery, and headless PNG frame
  rendering from the command line. A deck whose file is read-only, missing or
  changed by something else says so in a bar across the top, with the way out of
  it: reload, keep yours, or save somewhere else. Every save keeps the version
  it replaced beside it as `.bak`, and a deck open in another copy of OmaShow
  says so — advice, not a lock.

Template-pack installation, recording, interchange with other applications and
other release features remain on the roadmap. Tab stops, columns, text
direction, reusable text styles, spell-checking and text on a path are not
built yet. Exported PDFs carry real text but
not clickable link annotations. Notes are plain
text, and reading order is not yet carried into exported PDF. Text formatting applies to whole
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

## Rendering and responsiveness

The live canvas uses vendor-neutral OpenGL through Qt's GPU-backed QPainter on
Qt 6.9+: Intel, AMD and NVIDIA use the same rendering path. Explicit alternative
Qt backends retain Qt's raster fallback. Live video probes available VA-API
devices (including Intel and AMD), then optional CUDA devices, and falls back to
software if the installed drivers or codec cannot accelerate it. A failed GPU
does not prevent trying another GPU on a mixed system. No NVIDIA SDK or
vendor-specific driver is required to build or run OmaShow. Exports keep the
deterministic software decoder. GPU antialiasing can differ slightly from raster
exports.

Thumbnail rendering, picture adjustments, file image imports, batch layout previews,
open/recovery, save/autosave, object/picture clipboard processing and PDF export
run on workers. Playback caches resolved
slides, timings and transition matches; text layouts and navigator summaries are
cached too. Worker queues are bounded, and late file/preview results cannot discard
newer edits. PDF and deck saves commit atomically.

See [performance checks and limits](docs/performance.md) for reproduction commands,
measurements and renderer fallback switches.

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

Display routing and swapping pass a two-screen Qt simulation. Native rendering
has been checked on one physical ASUS 4K display. Two physical outputs, mixed
monitor scaling and unplug/reconnect still need hardware acceptance.

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

The native format is version 12, adding master number/date/footer fields and
per-slide artwork/field visibility to the existing local CSV links and cached
data, picture, media, shape and playback fields. Versions 1–11 still
open; versions 1 and 2 retain their original text appearance. Older app builds
refuse version 12 to preserve its features.
Identical image assets are stored once under `assets/`, named by a content hash;
moving the original files cannot break a deck. PDF images use lossless encoding.

## Verification

```sh
./bin/test-all
```

This builds the app and runs core, persistence, export, QML interaction and
presentation tests, simulated two-display routing, and the 13-frame rendering
harness. The suite includes large-deck responsiveness, background-job cancellation and stale-result checks. UI tests run offscreen and mock notification/idle changes. Captured
screens are in `build/qa/`; 100% reference captures for tables and charts are in `build/qa/table-scale-100/` and `build/qa/chart-scale-100/`.
To include the real Wayland/OpenGL canvas and hardware video check, run
`OMASHOW_TEST_GPU=1 ./bin/test-all` on a graphical session. The default run uses
software rendering and skips the hardware-dependent cases. Extended codec,
scaling, sustained GPU load and isolated package checks are documented in
[hardware validation](docs/hardware-validation.md).

## Licence

OmaShow is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE) for the full text, and
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for the work it builds on.

Copyright (C) 2026 Gavin Nugent.

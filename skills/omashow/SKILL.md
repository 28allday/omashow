---
name: omashow
description: >
  Making, editing, reading and exporting OmaShow presentations from the command
  line, with no window and no display — `omashow new`, `inspect`, `apply`,
  `export`, `review`, `ops`. Use when asked to build a deck, a slide show or a
  presentation on this machine, to change or check an existing `.omashow` file,
  or to turn one into a PDF, pictures, film, a package or a PowerPoint deck,
  or to bring a PowerPoint (.pptx) or Keynote (.key) deck into OmaShow. Triggers: omashow,
  make me a deck, build a presentation, slide deck, .omashow, presentation to
  PDF, export slides, presenter notes, slide transitions, check my slides,
  convert this pptx, open this keynote file.
  NOT for writing the app itself — that is an ordinary Qt project with its own
  notes beside the source.
---

# Driving OmaShow from the command line

OmaShow is a presentation app for Omarchy. Every verb runs headless, answers
with JSON on stdout, and exits 0 or 1. `docs/cli.md` in the repo is the full
reference; this is how to use it well.

```
omashow new <file> [--theme 0-2] [--size 16:9|1920x1080] [--layout 0-2] [--slides N]
omashow import <file.pptx|file.key> [--out <file.omashow>] [--force]
omashow inspect <file> [--slide N] [--full]
omashow apply <file> [ops.json|-] [--out <file>] [--dry-run] [--keep-going]
omashow export <file> --kind pdf|images|video|package|print|pptx --out <path>
omashow review <file>
omashow ops [--filter <text>]
```

## A deck from PowerPoint or Keynote

`omashow import talk.pptx` writes `talk.omashow` beside it and answers with
`warnings`: what could not be brought across, with slide numbers. Read them
back to the person. `inspect`, `review` and `export` also take a `.pptx` or
`.key` directly (the answer then carries `source`), but `apply` on one needs
`--out`, because the foreign file is never written to. Work on the
`.omashow` from then on. To hand a deck to someone with PowerPoint,
`export --kind pptx --out talk.pptx`; the answer's `log` names anything that
had to be approximated.

## The loop that works

1. **`new`** the deck, with as many slides as it needs.
2. **`inspect`** it. This is not optional: every edit needs the `id` of the thing
   it changes, and those ids are generated. A new deck's slides already carry
   placeholder text boxes — use them rather than adding new ones, because they
   inherit the layout's typography and position. Each says its role as
   `placeholderId` (`"title"`, `"body"`), so find them by role, not by order.
   `inspect` leaves out every property still at its default; `--full` gives all.
3. **`apply`** a list of operations.
4. **`inspect` or `review`** again to check what you actually made.
5. **`export`** it.

## Writing operations

```json
{"ops": [
  {"op": "setCurrentSlide", "args": [0]},
  {"op": "select", "args": ["placeholder-7f3a"]},
  {"op": "setSelectedProperty", "args": ["text", "Good morning"]},
  {"op": "setSelectedProperty", "args": {"name": "fontSize", "value": 72}}
]}
```

- `"slide": N` and `"select": "<id>"` on any operation happen first, which saves
  two lines every time.
- Arguments are positional or named; `omashow ops --filter <word>` gives the
  names and types for anything you are unsure of.
- Nothing is written unless every operation ran. Use `--dry-run` to rehearse and
  `--keep-going` only when a refusal is acceptable.
- Every result says `changed`. An edit that ran but changed nothing carries a
  `warning` (also gathered in the top-level `warnings`): a key, value or
  selection that did not suit it. Read them.
- A `"slide"` that does not exist, a `"select"` id that is not on that slide, and
  a `setSelectedProperty` key the object does not have are refused before
  anything runs, with the reason.

## The numbers and names

`omashow ops` (no filter) ends with a **`vocabulary`**: theme and layout numbers,
build effects/phases/triggers/easing, transition kinds/directions/keys, chart and
shape kinds, chart properties, table cell-style keys, and every property each
kind of object has. Look numbers up there — never guess `addChart(3)` or an
effect number.

## The operations worth knowing

| What | Operation |
|---|---|
| Move around | `setCurrentSlide`, `select`, `selectIds`, `selectAll` |
| Slides | `addSlide`, `duplicateSlide`, `deleteSlide`, `moveSlide`, `setSlidesSkipped` |
| Words | `addText`, then `setSelectedProperty` with `text`, `fontSize`, `fontWeight`, `textColor`, `textAlign`, `verticalAlign`, `lineHeight`, `listStyle` |
| Geometry | `setSelectedProperty` with `x`, `y`, `w`, `h`, `rotation`, `opacity` |
| Pictures and film | `insertImage` / `insertMedia` with a path |
| Shapes | `addShape` (a number from the gallery), `addRect` |
| Tables and charts | `addTable` / `addChart`, then **`table.setCells`** with a list of rows — the chart's row 0 is series names, column 0 categories. From the top-left it replaces the whole grid. Cell styling: `table.selectCell` then `table.formatCells`; `table.setTableOption headerRows true`. `inspect` shows the words as `cells` |
| Equations | `addEquation`, then `setSelectedProperty` `text` with TeX-ish source |
| Words on a shape | `putTextOnShape` with a text box and a shape selected |
| Design | `applyTheme`, `applyLayout`, `setThemeToken` (a colour or font of the theme) |
| Movement | `addBuild`, `setBuildProperty`, `setSlideTransition` |
| Speaker | `setSlideNotes` |
| Language | `setDeckLanguage`, `teachWord`, `setSmartPunctuation` |

## Things that will bite

- **Charts and tables land in the body's place.** A new one takes the layout's
  body area (or the space under the title), so delete the body placeholder first
  if the chart replaces it.
- **Describe charts, tables and pictures** with `setSelectedProperty altText` —
  `review` flags any without.
- **Placeholders beat new boxes.** `addText` puts an unstyled box in the middle
  of the slide. The placeholders a layout gives you are already the right size,
  font and position; set their `text` instead.
- **A new deck has one slide.** `--slides N` adds the rest.
- **Ids are not names.** Never guess one. Inspect, then aim.
- **`setSelectedProperty` acts on the selection**, so select first or pass
  `"select"`.
- **Straight quotes become curly** and `--` becomes an en dash when the words are
  committed. That is deliberate; `setSmartPunctuation false` turns it off.
- **Dialog operations are refused** (`insertImageDialog` and friends) — use the
  one that takes a path.
- **Film needs ffmpeg**, packaging needs `--approve-media` before it will read
  the files a deck links to, and spelling needs a Hunspell dictionary installed
  for the deck's language (`omashow review` says which ones exist).

## Reading the deck back

`inspect` gives slides, objects with geometry and text, builds, transitions and
notes. `review` gives findings (missing descriptions, contrast, type too small,
text that does not fit, slides with no title, unknown words) and statistics.
Both are how to check your own work before handing it over.

## If this skill is only on one machine

`omashow skill --link` puts it where every agent on the computer looks — the
same arrangement Omarchy uses for its own skills. The copy it links to is the
one that came with the app, so updating OmaShow updates this.

## If `omashow` is not on the path

Build it from a clone with `./bin/build` and call `./build/omashow`, or install
it as a package with `./bin/install` on Arch or Omarchy.

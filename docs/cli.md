# The command line

OmaShow without a window. Every verb answers with JSON on standard output and
exits 0 when it worked and 1 when it did not, so a script — or an agent — can
make a deck, change it, read back what it made, and export it, with nothing on
screen and no display attached.

There is no command-line subset of the app. The verbs drive the same backend the
interface drives, and `omashow ops` lists every operation there is by asking that
backend what it can do. Anything a person can do in the app, a script can do
here, under the same name.

```
omashow new <file> [--theme 0-2] [--size 16:9|1920x1080] [--layout 0-2] [--slides N] [--force]
omashow import <file.pptx|file.key> [--out <file.omashow>] [--force]
omashow inspect <file> [--slide N] [--full]
omashow apply <file> [ops.json|-] [--out <file>] [--dry-run] [--keep-going] [--force]
omashow export <file> --kind pdf|images|video|package|print|pptx --out <path> [...]
omashow review <file> [--include-dismissed]
omashow ops [--filter <text>]
omashow help
```

## Making one

```
$ omashow new talk.omashow --theme 1 --size 16:9 --slides 3
{ "ok": true, "file": "/home/you/talk.omashow", "slides": 3, "theme": "Paper", … }
```

Themes are 0, 1 and 2; layouts are 0, 1 and 2; sizes are `16:9`, `4:3`, `16:10`,
`portrait` or `<width>x<height>` in slide units.

## Bringing one in

A PowerPoint deck (`.pptx`) or a Keynote deck (`.key`) reads anywhere a deck is
named — `inspect`, `review` and `export` take one directly — and `import`
turns it into an OmaShow deck beside it:

```
$ omashow import quarterly.pptx
{ "ok": true, "file": "/home/you/quarterly.omashow", "slides": 14,
  "source": { "kind": "PowerPoint", "file": "…/quarterly.pptx", "warnings": [ … ] },
  "warnings": [ "A SmartArt graphic was left out (slide 7)" ], … }
```

`--out` names another destination (it must end in `.omashow`); an existing
file is only replaced with `--force`. Every answer that read a foreign deck
carries `source`, with the same `warnings`: what was left out or approximated,
one line each, with the slides it happened on. Nothing is ever written back to
the foreign file — `apply` on one needs `--out`.

A deck cannot bring its typefaces. `statistics.missingFonts` names the ones
this computer lacks, `missingFonts` (an operation) suggests a stand-in for
each, and `substituteFonts` rewrites the deck — every box, stretch, master,
layout, style, table cell and the theme — in one step:

```
[ { "op": "missingFonts" },
  { "op": "substituteFonts", "args": [ { "Gill Sans": "Inter", "Lora": "Noto Serif" } ] } ]
```

What comes across: slides and their order, hidden slides, sections, the slide
size and theme colours and fonts, text with its inherited typography and
per-range formatting, lists, shapes and their fills, lines and shadows,
pictures (with crops and masks), films, tables (with merged cells and cell
fills), charts (with their data and series colours), notes, comments,
transitions (fade, fade through black, push, cover, uncover, zoom, newsflash
as whirl and morph; others become the nearest of those), entrance, exit and
emphasis builds, and links. What does not: embedded typefaces, SmartArt,
embedded objects and pictures in EMF/WMF — each is named in `warnings`.
A Keynote deck gives its slides, text, shapes, pictures, tables and notes;
libetonyek does not report its builds, transitions or charts.

The other way round, `export --kind pptx` writes a PowerPoint deck (see
Exporting), so a deck can make the round trip.

## Reading one

`inspect` is how a script finds what to aim at. Every object has an `id`, and
that id is what operations take.

```
$ omashow inspect talk.omashow --slide 0
{
  "ok": true,
  "slide": {
    "index": 0, "id": "slide-…", "title": "Your title",
    "transition": "morph", "transitionIsItsOwn": false, "notes": "",
    "objects": [ { "id": "placeholder-…", "type": "text", "text": "Your title",
                   "x": 160, "y": 300, "w": 1600, "h": 200, "fontSize": 96, … } ],
    "builds": []
  }
}
```

By default each object lists only what differs from a new object of its kind,
plus its id, type, geometry, text and `placeholderId` (the layout role it fills:
`title`, `body`, …). Tables and charts add their words as `cells`, a list of
rows. Heavy properties — path data, full table and chart structure, per-range
formatting — and the defaults come back with `--full`. Each layout lists its
placeholders by role.

## Changing one

`apply` runs a list of operations in order. Each is `{"op": …}` with arguments
either in order or by name:

```json
{"ops": [
  {"op": "setCurrentSlide", "args": [0]},
  {"op": "select", "args": ["placeholder-1234"]},
  {"op": "setSelectedProperty", "args": ["text", "Good morning"]},
  {"op": "setSelectedProperty", "args": {"name": "fontSize", "value": 72}},
  {"op": "addEquation"},
  {"op": "setSlideTransition", "args": ["kind", 1]}
]}
```

Two conveniences, because nearly every edit wants them first: `"slide": N` sets
the current slide before the operation runs, and `"select": "<id>"` (or a list of
ids) selects before it runs.

```
$ omashow apply talk.omashow ops.json
{ "ok": true, "applied": 6, "of": 6, "written": true, "file": "…", "results": [ … ] }
```

- Nothing is written until every operation has run. One refusal stops the lot
  and the file on disk is untouched; `--keep-going` carries on past refusals and
  writes what worked.
- `--out` writes the result somewhere else and leaves the original alone.
- `--dry-run` runs everything and writes nothing, which is how to check a plan.
- The ops file can be `-` to read from standard input.
- Each result says whether the deck `changed`. An edit that ran and changed
  nothing gets a `warning`, gathered again under the top-level `warnings`.
- A `"slide"` out of range, a `"select"` id not on that slide, and a
  `setSelectedProperty` key the selected object does not have are refused
  before anything runs. When the backend explains a refusal (a missing file, an
  unreadable picture) that explanation is the error.

Wherever an operation takes a numbered choice — a shape or chart kind, a
build's phase, effect, trigger, easing or reveal unit, a transition's kind or
direction — its name from `omashow ops` works as well as its number, in any
case and with any spacing: `["kind", "fade-through-black"]`,
`addShape("star")`, `addChart("Stacked column")`. The same goes for a text box's
or shape's numbered settings through `setSelectedProperty` — `listStyle`,
`textAlign`, `verticalAlign`, `textFit`, `direction`, `imageMode` and
`fillStyle` — listed under `vocabulary.objectChoices`: `["listStyle", "bullets"]`,
`["textAlign", "centre"]`, `["fillStyle", "linear"]`. A name that is not a
choice is refused with the list of those that are.

A master's footer, date and slide number are set with `setMasterField(masterId,
key, value)`, the keys listed under `vocabulary.masterFields`. Each shows only
once its `show…` key is `true`: `["<masterId>", "footer", "Acme"]` then
`["<masterId>", "showFooter", true]`. `inspect` lists each master's `fields`.

## Tables and chart data

Cells belong to the table model, which follows the selection. Its operations
are named `table.<method>`:

```json
{"ops": [
  {"op": "addChart", "slide": 1, "args": [0]},
  {"op": "table.setCells", "args": [[["Quarter", "2025", "2026"],
                                     ["Q1", "12", "15"], ["Q2", "14", "19"]]]},
  {"op": "table.selectCell", "args": [0, 0]},
  {"op": "table.selectCell", "args": [0, 2, true]},
  {"op": "table.formatCells", "args": ["fontWeight", 700]}
]}
```

`table.setCells(rows, row, column)` writes a grid from that cell, growing the
table; from the top-left corner it replaces the whole table. A new table or
chart is placed in the layout's body area, or under the title.

## What it will not do

Operations that would ask the desktop for a file, or that finish later on
another thread, are refused by name with the alternative given:

```
$ echo '[{"op": "insertImageDialog"}]' | omashow apply talk.omashow -
… "error": "insertImageDialog asks the desktop for a file; give the path to the
   operation that takes one instead"
```

Use `insertImage` with a path. The same goes for `openAsync` (open the deck as
the file argument), `saveAsync` (`saveTo`), `pasteAsync` (`paste`) and the rest.

## Exporting

`apply` only edits the deck: operations that would write a file, export, print,
change templates or the recent list, or touch a recovery journal are refused
with a note saying which verb does that instead, so `--dry-run` really is dry.

```
omashow export talk.omashow --kind pdf     --out talk.pdf [--layout notes] [--stages]
omashow export talk.omashow --kind images  --out frames/slide.png --width 1920 [--format jpeg]
omashow export talk.omashow --kind video   --out talk.mp4 --fps 30 [--quality 1]
omashow export talk.omashow --kind package --out talk.zip --approve-media
omashow export talk.omashow --kind print   --printer "Office" --copies 2
omashow export talk.omashow --kind pptx    --out talk.pptx
```

`pptx` writes a PowerPoint deck: every slide as it would be drawn, with its
design applied, plus notes, comments, sections, custom shows, transitions and
builds. What PowerPoint has no form for — an equation, words on a path, a
morph, a picture fill — is written in the nearest form, and the answer's `log`
says so, slide by slide. The log also names the typefaces the deck asks for,
since a `.pptx` carries their names and not the fonts themselves.

`--from` and `--to` take a slide range, `--include-skipped` includes the slides
the deck skips, `--per-page` sets handout sheets and `--layout` is `slides`,
`notes`, `outline` or `handout`. The folder named by `--out` is created if it is
not there.

An existing `--out` file is only replaced with `--force`.

Reading a file the deck links to is the author's decision here as it is in the
app: packaging leaves linked media out and says so, and pictures, film and PDF
show a linked film's poster, unless `--approve-media` is given. Even then a film is included only when the file on disk opens as media
and matches the one the deck recorded; the answer's `log` names every path it
approved or refused. A linked table or chart file travels only when its bytes
still match the ones the table was linked to — refresh the table first if the
file has changed.

## Reviewing

```
$ omashow review talk.omashow
{ "ok": true,
  "findings": [ { "check": "description", "severity": "must", "title": "…", … } ],
  "statistics": { "slides": 3, "words": 42, … },
  "language": "en_GB", "spellingAvailable": false, "dictionaries": [] }
```

Spelling is checked only when a Hunspell dictionary for the deck's language is
installed; `dictionaries` says which ones this computer has.

## Driving it from an agent

`skills/omashow/SKILL.md` is an agent skill for this command line — the working
loop, the operations worth knowing and the things that bite. It ships with the
app, packaged to `/usr/share/omashow/skills/`, and linking it is part of
installing OmaShow: a skill nobody links is a skill nobody uses. `./bin/install`
does it as you, and a package install does it for whoever ran sudo, never as
root. It is this command either way, and it is worth knowing:

```sh
$ omashow skill          # where it is, and which agents have it
$ omashow skill --link   # put it where they look
```

Omarchy keeps one skill directory per agent and links its own skills into each,
so `--link` does the same: `~/.agents/skills`, `~/.claude/skills`, and
`~/.codex/skills`, `~/.pi/agent/skills`, `~/.hermes/skills` and each Hermes
profile for whichever of those agents are installed. No directory is made for an
agent that is not there.

```sh
$ omashow skill --link
{ "ok": true, "skill": "/usr/share/omashow/skills/omashow/SKILL.md", "linked": 5,
  "places": [ { "place": "/home/you/.agents/skills/omashow", "linked": true }, … ],
  "advice": "The agents on this computer will find it in a new session." }
```

Asking twice is not an error. Something already sitting at one of those names is
left alone, and named in the answer, unless you say `--force`.

## Finding the operation you want

```
$ omashow ops --filter text
{ "operations": [ { "op": "addText", "args": [], "returns": "" },
                  { "op": "setSelectedProperty", "args": [ {"name": "name", "type": "QString"},
                                                           {"name": "value", "type": "QVariant"} ],
                    "returns": "" }, … ] }
```

The list comes from the backend's own meta-object, so it is never out of date: a
feature added to the app appears here the day it is added.

Without `--filter` the answer also carries a `vocabulary`: what the numbers mean
(themes, new-deck layouts, build effects, phases, triggers, easing, transition
kinds and directions, chart and shape kinds) and which keys each setter takes
(object properties by kind, chart properties, build properties, transition
keys, table cell styles and options). Property setters are
listed under the name a script would guess (`setCurrentSlide`, `setSnapEnabled`),
and `get`/`set` read and write any property directly.

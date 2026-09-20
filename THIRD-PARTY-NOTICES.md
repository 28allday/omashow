# Third-party notices

OmaShow is licensed under the GNU General Public License, version 3 or (at
your option) any later version — see `LICENSE`. It builds on the following
work, each under its own licence.

## Vendored in this repository

| Component | Where | Licence |
|---|---|---|
| [Lucide](https://lucide.dev) icons (some derived from [Feather](https://feathericons.com)) | `src/ui/icons/Icons.qml`, generated from `tools/icons.txt` | ISC, with Feather's portions under MIT — full text in `src/ui/icons/LICENSE.lucide` |

## Linked at run time (system packages, not redistributed here)

| Component | Licence |
|---|---|
| [Qt 6](https://www.qt.io) — Core, Gui, Qml, Quick, Quick Controls, Svg, Multimedia, Concurrent, DBus | LGPL-3.0 / GPL-3.0 |
| [FFmpeg](https://ffmpeg.org) — libavformat, libavcodec, libavutil, libswscale | LGPL-2.1-or-later; GPL-3.0 as built with `--enable-gpl --enable-version3` by Arch Linux |
| [zlib](https://zlib.net) | zlib licence |
| [Hunspell](https://hunspell.github.io) | GPL-2.0-or-later / LGPL-2.1-or-later / MPL-1.1 |

Because the FFmpeg build OmaShow links against is GPL-3.0, OmaShow as a whole
is distributed under GPL-3.0-or-later terms. No "nonfree" component is used.

## Spelling dictionaries

OmaShow bundles no dictionaries. It reads the Hunspell dictionaries installed on
the system, each under its own licence, and says so when there are none for the
language a deck is written in. `tests/fixtures/dictionaries` holds a ten-word
dictionary written for the tests; it is not a language.

## Fonts

OmaShow bundles no fonts. It uses the fonts installed on the system (Inter
when present). PDF export embeds subsets of the fonts a deck uses; the licence
of a font chosen for a deck is that font's own.

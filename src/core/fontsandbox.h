#pragma once

// OmaShow's own font folder.
//
// Extra fonts (the free stand-ins for Office fonts) live in a folder that only
// OmaShow reads: $OMASHOW_FONTS_DIR if set, else $XDG_DATA_HOME/omashow-fonts
// (~/.local/share/omashow-fonts). fontconfig does not scan it (it is not
// ~/.local/share/fonts or ~/.fonts), so nothing installed there changes how
// the rest of Omarchy draws text. At startup every .ttf/.otf/.ttc/.otc under
// it (any depth) is handed to QFontDatabase::addApplicationFont, which makes
// the families visible to this process only.
//
// Sharing: the folder is a plain, documented path, so another app can opt in
// later without touching the system, e.g. with its own fontconfig file that
// includes the system config and adds <dir>~/.local/share/omashow-fonts</dir>,
// passed to that app alone via FONTCONFIG_FILE (see docs/fonts-sandbox.md).

#include <QString>

namespace FontSandbox {

// The folder (not created here).
QString dir();
// Load every font file under dir(); returns the number of files loaded.
// Safe to call more than once: a file already loaded is skipped.
int load();
// Load one font file that belongs to the family folder <folder> (a map name
// without spaces); returns the families inside it (empty: not a font).
QStringList loadFile(const QString &path, const QString &folder);
// The map's family is usable: installed, or loaded under another internal
// name (google/fonts "Fjord One" is "Fjord" inside its file; "Nanum Gothic"
// is "NanumGothic"). Such names get a QFont substitution, so text set in the
// map's name draws with the file's family.
bool available(const QString &family);

} // namespace FontSandbox

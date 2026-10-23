# OmaShow's own font folder (sandboxed fonts)

OmaShow draws Microsoft Office fonts (Calibri, Cambria, Segoe UI…) with free,
openly licensed alternatives (Carlito, Caladea, Noto Sans…). These are **not**
Microsoft's fonts. Extra alternatives live in a folder only OmaShow reads:

    $OMASHOW_FONTS_DIR                    (if set)
    $XDG_DATA_HOME/omashow-fonts          (otherwise)
    ~/.local/share/omashow-fonts          (XDG_DATA_HOME unset)

* fontconfig does **not** scan it (it is neither `~/.local/share/fonts` nor
  `~/.fonts`), so putting fonts there never changes how Omarchy or any other
  app draws text, and no `fc-cache` run is needed.
* At startup OmaShow hands every `.ttf/.otf/.ttc/.otc` under it (any depth;
  one subfolder per family is suggested: `Carlito/Carlito-Regular.ttf`) to
  `QFontDatabase::addApplicationFont` - visible to the OmaShow process only.
* OmaShow creates it only when it downloads a font or when you click **Fonts
  folder** in the font-check popup or Format ▸ Free fonts…. Downloaded families go in one subfolder
  each, with their licence file, and are loaded at once (no restart).

## Sharing with other office apps (opt-in, per app - not configured)

The folder is a plain documented path, so any app can be pointed at it
without touching system fonts. Recommended per app, e.g. for app `foo`:

1. A fontconfig file only that app uses,
   `~/.config/omashow-fonts/fontconfig/foo.conf`:

   ```xml
   <?xml version="1.0"?>
   <!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
   <fontconfig>
     <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
     <dir prefix="xdg">omashow-fonts</dir>
     <cachedir prefix="xdg">fontconfig-omashow-fonts</cachedir>
   </fontconfig>
   ```

   It includes the normal system config (so the user's usual settings still
   apply) and adds the folder, with its own cache dir so the shared cache is
   untouched.
2. Launch only that app with it: `FONTCONFIG_FILE=~/.config/omashow-fonts/fontconfig/foo.conf foo`
   (a wrapper script or a `.desktop` override in `~/.local/share/applications`).
3. Qt apps could instead read `$OMASHOW_FONTS_DIR` themselves, as OmaShow does.

Apps not launched this way are unaffected. Removing the conf file or wrapper
undoes it. LibreOffice also honours its own per-user `fonts` dir in its
profile, which could be a symlink to this folder instead.

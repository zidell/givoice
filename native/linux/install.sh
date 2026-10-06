#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
binary="${GIVOICE_BUILD_DIR:-$root/dist-native/linux}/givoice"
if [[ ! -x "$binary" ]]; then "$root/native/linux/build.sh"; fi
prefix="${GIVOICE_INSTALL_PREFIX:-$HOME/.local}"
install -Dm755 "$binary" "$prefix/bin/givoice"
install -Dm644 "$root/docs/readme.txt" "$prefix/share/doc/givoice/readme.txt"
install -Dm644 "$root/native/linux/README.md" "$prefix/share/doc/givoice/linux-readme.md"
install -Dm644 "$root/native/linux/vendor/tomlc17/LICENSE" "$prefix/share/doc/givoice/tomlc17-LICENSE"
mkdir -p "$prefix/share/applications"
python3 - "$prefix" "$root/native/linux/com.videostew.givoice.desktop" <<'PY'
import pathlib,sys
prefix=pathlib.Path(sys.argv[1]).resolve()
# Desktop entry Exec uses its own quoting/escaping rules, not shell quoting.
exe=str(prefix/'bin/givoice').replace('\\','\\\\\\\\').replace('"','\\"').replace('`','\\`').replace('$','\\$').replace('%','%%')
text=pathlib.Path(sys.argv[2]).read_text().replace('Exec=givoice',f'Exec="{exe}"')
(prefix/'share/applications/com.videostew.givoice.desktop').write_text(text)
PY
if command -v update-desktop-database >/dev/null; then update-desktop-database "$prefix/share/applications"; fi
extension=givoice-escape@videostew.com
for file in extension.js metadata.json; do
  install -Dm644 "$root/native/linux/gnome-extension/$file" "$prefix/share/gnome-shell/extensions/$extension/$file"
done
if [[ "${XDG_CURRENT_DESKTOP:-}" == *GNOME* ]] && command -v gsettings >/dev/null; then
  python3 - "$extension" <<'PYTHON'
import sys
from gi.repository import Gio
settings = Gio.Settings.new('org.gnome.shell')
name = sys.argv[1]
enabled = settings.get_strv('enabled-extensions')
if name not in enabled:
    settings.set_strv('enabled-extensions', enabled + [name])
disabled = settings.get_strv('disabled-extensions')
if name in disabled:
    settings.set_strv('disabled-extensions', [item for item in disabled if item != name])
Gio.Settings.sync()
PYTHON
  echo 'GNOME: log out and back in once to load the new Escape cancellation extension.'
fi
kwin_extension="$(dirname "$binary")/givoice-escape.so"
if [[ -f "$kwin_extension" ]]; then
  install -Dm755 "$kwin_extension" "$prefix/lib/qt6/plugins/kwin/effects/plugins/givoice-escape.so"
  bootstrap="$prefix/share/kwin/scripts/givoice-escape-bootstrap"
  install -Dm644 "$root/native/linux/kwin-extension/bootstrap-metadata.json" "$bootstrap/metadata.json"
  install -Dm644 "$root/native/linux/kwin-extension/bootstrap.qml" "$bootstrap/contents/ui/main.qml"
  install -Dm755 "$(dirname "$binary")/libgivoicebootstrap.so" "$bootstrap/contents/ui/bootstrap/libgivoicebootstrap.so"
  printf 'module GivoiceBootstrap\nplugin givoicebootstrap\n' > "$bootstrap/contents/ui/bootstrap/qmldir"
  if command -v kwriteconfig6 >/dev/null; then
    kwriteconfig6 --file kwinrc --group Plugins --key givoice-escape-bootstrapEnabled true
  fi
fi
# Both backends can coexist; the desktop selects its own at login.
if command -v dpkg-query >/dev/null; then
  for backend in xdg-desktop-portal-gnome xdg-desktop-portal-kde; do
    if [[ "$(dpkg-query -W -f='${Status}' "$backend" 2>/dev/null || true)" != 'install ok installed' ]]; then
      printf 'Missing desktop backend: %s (install with sudo apt install %s)\n' "$backend" "$backend" >&2
    fi
  done
fi
printf 'Installed %s/bin/givoice\n' "$prefix"

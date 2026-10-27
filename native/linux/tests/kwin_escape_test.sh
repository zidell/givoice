#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../../.." && pwd)"
output="${GIVOICE_BUILD_DIR:-$root/dist-native/linux}"
if pgrep -x givoice >/dev/null; then
  echo 'Quit idle Givoice before the private KDE Escape test.' >&2
  exit 1
fi
python3 "$root/native/linux/kwin-extension/build.py" --test-driver
install -Dm755 "$output/givoice-shortcut-driver.so" "$HOME/.local/lib/qt6/plugins/kwin/effects/plugins/givoice-shortcut-driver.so.new"
mv -f "$HOME/.local/lib/qt6/plugins/kwin/effects/plugins/givoice-shortcut-driver.so.new" "$HOME/.local/lib/qt6/plugins/kwin/effects/plugins/givoice-shortcut-driver.so"
trap 'busctl --user call org.kde.KWin /Effects org.kde.kwin.Effects unloadEffect s givoice-shortcut-driver >/dev/null; rm -f "$HOME/.local/lib/qt6/plugins/kwin/effects/plugins/givoice-shortcut-driver.so"' EXIT
busctl --user call org.kde.KWin /Effects org.kde.kwin.Effects loadEffect s givoice-shortcut-driver
test_source=escape_smoke
if [[ "${1:-}" == --shortcuts ]]; then test_source=shortcut_smoke; fi
"${CC:-cc}" -std=c11 -Wall -Wextra -DGIVOICE_KDE_ESCAPE_TEST $(pkg-config --cflags gtk+-3.0 gio-unix-2.0) -I"$root/native/linux/src" "$root/native/linux/tests/$test_source.c" "$root/native/linux/src/portal.c" -o "$output/kwin-$test_source" $(pkg-config --libs gtk+-3.0 gio-unix-2.0)
GDK_BACKEND=wayland timeout 55s "$output/kwin-$test_source"

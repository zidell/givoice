#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
output="${GIVOICE_BUILD_DIR:-$root/dist-native/linux}"
version="${GIVOICE_VERSION:-0.1.2}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo 'Version must be MAJOR.MINOR.PATCH' >&2
  exit 1
fi
mkdir -p "$output"
settings_sources=("$root/native/linux/src/settings.c" "$root/native/linux/vendor/tomlc17/tomlc17.c")
packages=(gtk+-3.0 ayatana-appindicator3-0.1 libpulse-mainloop-glib libpulse-simple libcurl json-glib-1.0 gio-unix-2.0)
if ! command -v "${CC:-cc}" >/dev/null || ! command -v pkg-config >/dev/null; then
  echo 'Install build dependencies: sudo apt install build-essential pkg-config libgtk-3-dev libayatana-appindicator3-dev libpulse-dev libcurl4-openssl-dev libjson-glib-dev' >&2
  exit 1
fi
pkg-config --exists "${packages[@]}"
read -r -a cflags <<< "$(pkg-config --cflags "${packages[@]}")"
read -r -a libs <<< "$(pkg-config --libs "${packages[@]}")"
python3 "$root/native/linux/embed_sounds.py" "$output/sounds.h"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Wpedantic ${CFLAGS:-} "${cflags[@]}" -I"$output" \
  "-DGIVOICE_VERSION=\"$version\"" \
  "$root/native/linux/src/main.c" "$root/native/linux/src/core.c" "${settings_sources[@]}" "$root/native/linux/src/portal.c" "$root/native/linux/src/output.c" "$root/native/linux/src/overlay.c" \
  -o "$output/givoice" ${LDFLAGS:-} "${libs[@]}" -lm
if [[ "${1:-}" == --test ]]; then
  "$root/native/linux/test.sh"
fi
printf 'Built %s\n' "$output/givoice"

#!/bin/bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo 'Usage: render_site.sh OUTPUT_DIRECTORY' >&2
    echo 'Optional: GH_TOKEN for API limits.' >&2
    exit 1
fi

output_dir="$1"
project_root="$(cd "$(dirname "$0")/.." && pwd)"
repo='zidell/givoice'

# Each platform ships from its own tag series (macos-v*, linux-v*, windows-v*) with fixed
# asset names, so the page links straight to the files of the newest tag of each.
latest_version() {
    git -C "$project_root" ls-remote --tags --refs origin "refs/tags/$1-v*" |
        sed "s#.*refs/tags/$1-v##" | grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' | sort -V | tail -n 1 || true
}
macos_version="$(latest_version macos)"
linux_version="$(latest_version linux)"
windows_version="$(latest_version windows)"
for platform in macos linux; do
    version_var="${platform}_version"
    if [[ -z "${!version_var}" ]]; then
        echo "$platform 릴리스 버전을 찾지 못했습니다" >&2
        exit 1
    fi
done

auth=()
if [[ -n "${GH_TOKEN:-}" ]]; then auth=(-H "Authorization: Bearer $GH_TOKEN"); fi
macos_release="$(curl -fsSL ${auth[@]+"${auth[@]}"} "https://api.github.com/repos/$repo/releases/tags/macos-v$macos_version")"
asset_size() {
    jq -r --arg name "$1" '.assets[] | select(.name == $name) | .size' <<< "$macos_release" |
        awk '{ printf "%.1f MB", $1 / 1000000 }'
}
macos_arm64_size="$(asset_size Givoice-macos-arm64.dmg)"
macos_x64_size="$(asset_size Givoice-macos-x64.dmg)"
macos_arm64_label="${macos_arm64_size:+$macos_arm64_size · }DMG"
macos_x64_label="${macos_x64_size:+$macos_x64_size · }DMG"

mkdir -p "$output_dir"
sed -e "s/__MACOS_VERSION__/$macos_version/g" \
    -e "s/__LINUX_VERSION__/$linux_version/g" \
    -e "s/__WINDOWS_VERSION__/$windows_version/g" \
    -e "s/__MACOS_ARM64_SIZE__/$macos_arm64_label/g" \
    -e "s/__MACOS_X64_SIZE__/$macos_x64_label/g" \
    "$project_root/site/index.html" > "$output_dir/index.html"
# Until the first windows-v* release exists, leave the Windows download out.
if [[ -z "$windows_version" ]]; then
    sed -i.bak -e '/data-windows/d' "$output_dir/index.html"
    rm -f "$output_dir/index.html.bak"
fi

# Sparkle in the macOS app reads appcast-<arch>.xml; the Linux app compares linux-version.txt.
for arch in arm64 x64; do
    if ! curl -fsL -o "$output_dir/appcast-$arch.xml" \
        "https://github.com/$repo/releases/download/macos-v$macos_version/appcast-$arch.xml"; then
        rm -f "$output_dir/appcast-$arch.xml"
        echo "macos-v$macos_version 릴리스에 appcast-$arch.xml이 없어 업데이트 피드를 건너뜁니다." >&2
    fi
done
printf '%s\n' "$linux_version" > "$output_dir/linux-version.txt"

cp "$project_root/assets/givoice.svg" "$output_dir/logo.svg"
mkdir -p "$output_dir/screenshots"
cp "$project_root/assets/screenshots/givoice-flow.gif" "$output_dir/screenshots/givoice-flow.gif"
cp "$project_root/assets/screenshots/givoice-windows-flow.gif" "$output_dir/screenshots/givoice-windows-flow.gif"
cp "$project_root/assets/screenshots/macos-menu-preview.png" "$output_dir/screenshots/macos-menu-preview.png"
cp "$project_root/assets/screenshots/windows-menu-preview.png" "$output_dir/screenshots/windows-menu-preview.png"
cp "$project_root/site/robots.txt" "$output_dir/robots.txt"
cp "$project_root/site/sitemap.xml" "$output_dir/sitemap.xml"
touch "$output_dir/.nojekyll"

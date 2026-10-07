# Releasing

Releases are automatic. When a push to `main` passes CI, `auto-release.yml` checks
each platform for changes since its last tag, raises the patch version by one, tags the
tested commit and starts that platform's release workflow. A push that changes only
documentation releases nothing, and `[skip release]` in the commit message skips a
release on purpose.

| Platform | Release when these change |
| --- | --- |
| macOS | `native/macos`, `packaging/macos-entitlements.plist` |
| Ubuntu | `native/linux`, `packaging/build-deb.sh` |
| Windows | `native/windows` |

`assets`, `docs/readme.txt` and `config.toml.example` count for every platform. To ship
a minor or major version, push the tag by hand (for example `macos-v0.2.0`); automatic
releases continue from the highest tag.

Each platform ships from its own tag series:

| Platform | Tag | Workflow | Release files | How installed apps update |
| --- | --- | --- | --- | --- |
| macOS | `macos-vX.Y.Z` | `native-macos-release.yml` | `Givoice-macos-arm64.dmg`, `Givoice-macos-x64.dmg` (signed, notarized) | Sparkle reads `appcast-<arch>.xml` from the landing page and offers the update |
| Ubuntu | `linux-vX.Y.Z` | `native-linux-release.yml` | `Givoice-ubuntu-amd64.deb`, `SHA256SUMS` | The tray menu shows a download item when `linux-version.txt` is newer |
| Windows | `windows-vX.Y.Z` | `native-windows-release.yml` | `Givoice-windows-x64.exe` (portable, unsigned), `SHA256SUMS` | No in-app check; users download the new EXE from the landing page |

Release file names carry no version, so the landing page links straight to the files of
each platform's newest tag (`releases/download/<tag>/<file>`). GitHub's
`releases/latest/download/` cannot serve three tag series at once, so the page is
re-rendered instead: every release workflow redeploys it when it finishes, and the
download links, `appcast-*.xml` and `linux-version.txt` follow the newest tag. Until the
first `windows-v*` tag exists the page leaves the Windows download out. Tags created by
the workflow token do not trigger workflows on their own, so `auto-release.yml`
dispatches the release workflow itself.

The Windows EXE is not code-signed, so Windows SmartScreen warns on first launch
(More info → Run anyway); the landing page says so.

## Secrets

Repository secrets (values never go in the repository):

- macOS signing and notarization: `GIVOICE_MAC_CERTIFICATE_BASE64`,
  `GIVOICE_MAC_CERTIFICATE_PASSWORD`, `GIVOICE_MAC_SIGNING_IDENTITY`,
  `GIVOICE_APPLE_ID`, `GIVOICE_APPLE_APP_PASSWORD`, `GIVOICE_APPLE_TEAM_ID`.
- Sparkle update signing: `GIVOICE_SPARKLE_PRIVATE_KEY`. Its public half is
  `SUPublicEDKey` in `native/macos/Info.plist`. The maintainer's login keychain holds
  the original under the `givoice` account (`generate_keys --account givoice`).
  **If this key is lost, installed macOS apps can no longer accept updates**, so keep
  an offline backup (`generate_keys --account givoice -x <file>`).

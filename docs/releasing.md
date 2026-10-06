# Releasing

Releases are automatic. When a push to `main` passes CI, `auto-release.yml` checks
each platform for changes since its last tag, raises the patch version by one, tags the
tested commit and starts that platform's release workflow. A push that changes only
documentation releases nothing, and `[skip release]` in the commit message skips a
release on purpose. Windows joins the automatic releases once
`GIVOICE_WINDOWS_STORE_URL` is set.

| Platform | Release when these change |
| --- | --- |
| macOS | `native/macos`, `packaging/macos-entitlements.plist` |
| Ubuntu | `native/linux`, `packaging/build-deb.sh` |
| Windows | `native/windows`, `packaging/build-msix.ps1`, `packaging/msix-assets` |

`assets`, `docs/readme.txt` and `config.toml.example` count for every platform. To ship
a minor or major version, push the tag by hand (for example `macos-v0.2.0`); automatic
releases continue from the highest tag.

Each platform ships from its own tag series:

| Platform | Tag | Workflow | Where users get it | How installed apps update |
| --- | --- | --- | --- | --- |
| macOS | `macos-vX.Y.Z` | `native-macos-release.yml` | GitHub Release (signed, notarized DMGs) | Sparkle reads `appcast-<arch>.xml` from the landing page and offers the update |
| Ubuntu | `linux-vX.Y.Z` | `native-linux-release.yml` | GitHub Release (`.deb`) | The tray menu shows a download item when `linux-version.txt` is newer |
| Windows | `windows-vX.Y.Z` (X ≥ 1) | `native-windows-release.yml` | Microsoft Store | The Store updates it after certification |

The macOS and Ubuntu workflows redeploy the landing page when they finish, so
the download links, `appcast-*.xml` and `linux-version.txt` follow the newest tag.
The Store requires a nonzero major version, so Windows tags start at `windows-v1.0.0`
and must increase with every submission. Tags created by the workflow token do not trigger
workflows on their own, so `auto-release.yml` dispatches the release workflow itself.

## Secrets and variables

Repository secrets (values never go in the repository):

- macOS signing and notarization: `GIVOICE_MAC_CERTIFICATE_BASE64`,
  `GIVOICE_MAC_CERTIFICATE_PASSWORD`, `GIVOICE_MAC_SIGNING_IDENTITY`,
  `GIVOICE_APPLE_ID`, `GIVOICE_APPLE_APP_PASSWORD`, `GIVOICE_APPLE_TEAM_ID`.
- Sparkle update signing: `GIVOICE_SPARKLE_PRIVATE_KEY`. Its public half is
  `SUPublicEDKey` in `native/macos/Info.plist`. The maintainer's login keychain holds
  the original under the `givoice` account (`generate_keys --account givoice`).
  **If this key is lost, installed macOS apps can no longer accept updates**, so keep
  an offline backup (`generate_keys --account givoice -x <file>`).
- Microsoft Store package identity: `GIVOICE_STORE_PACKAGE_NAME`,
  `GIVOICE_STORE_PUBLISHER`, `GIVOICE_STORE_PUBLISHER_DISPLAY_NAME` (Partner Center →
  the app → Product identity).
- Microsoft Store submission API: `GIVOICE_STORE_TENANT_ID`, `GIVOICE_STORE_SELLER_ID`,
  `GIVOICE_STORE_PRODUCT_ID`, `GIVOICE_STORE_CLIENT_ID`, `GIVOICE_STORE_CLIENT_SECRET`.
  The client is a Microsoft Entra app registration added to Partner Center with the
  Manager role. **Its client secret expires** (at most 24 months); create a new one
  before the expiry date and replace `GIVOICE_STORE_CLIENT_SECRET`, or Store
  submissions start failing.

Repository variable:

- `GIVOICE_WINDOWS_STORE_URL`: the public Store listing URL. Until it is set the
  landing page says the Windows version is coming instead of linking to the Store.

## First Microsoft Store submission

Automated submissions only update an app that is already live. For the first one,
push a `windows-v1.0.0` tag, download the `windows-store-x64` artifact from the
workflow run, and submit it in Partner Center together with the listing (description,
screenshots, privacy policy URL `https://github.com/zidell/givoice/blob/main/docs/privacy.md`).
After it is certified, set `GIVOICE_WINDOWS_STORE_URL`; later tags are submitted
automatically when all five submission secrets exist.

# tools/winget/ — App Installer bundle for winget bootstrap (you add this)

A fresh Windows 10 install often has **no winget**. When a profile has a winget-only app
(no local fallback), the provisioner tries to install winget from the packages in this
folder before continuing.

## What to put here

From <https://github.com/microsoft/winget-cli/releases> (latest stable), download and
drop into this folder:

1. **`Microsoft.DesktopAppInstaller_*.msixbundle`** — the App Installer (winget) itself.
2. **`Microsoft.DesktopAppInstaller_*_License1.xml`** — optional, not used by the
   Add-AppxPackage path.
3. The dependency packages from the release's `DesktopAppInstaller_Dependencies.zip`
   (extract them here):
   - `Microsoft.VCLibs.x64.14.00.Desktop.appx`
   - `Microsoft.UI.Xaml.2.8.x64.appx`  (version may differ — match the release notes)

Any `*.msixbundle`, `*.msix`, `*.appx`, `*.appxbundle` in this folder is installed
(sorted by name, so `VCLibs`/`UI.Xaml` go before `DesktopAppInstaller`), then a second
pass retries anything that failed on a missing dependency.

## Notes

- ~150-250 MB total. `build-release.ps1` copies this whole folder into
  `dist\ShiftechWinProvisioner\tools\winget\`.
- The `.msix*` / `.appx*` files are gitignored (large, Microsoft-licensed) — this README
  is what's tracked.
- Bootstrap needs the run to be **elevated** (App Installer is a machine package on some
  builds). It is skipped in `--dry-run`.
- If this folder is empty, winget apps with no local fallback are simply skipped with a
  clear message — the run still completes.

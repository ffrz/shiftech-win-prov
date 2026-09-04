# apps/ — local apps for the USB drive

Each subfolder is one app the provisioner can install/deploy without winget. Referenced
from a profile as `{ "id": "<folder>", "source": "local", "enabled": true }`.

```
apps/
  winrar/        app.json + winrar-x64-550.exe        (kind: installer)
  adobe-reader/  app.json + AcroRdrDC1800920044_en_US.exe
  7zip/          app.json + 7z2408-x64.exe   <- DOWNLOAD from 7-zip.org, /S silent
  wu10man/       app.json + Wu10Man_2.1.0.msi
  aact/          app.json + aact-4.0-portable.7z       (kind: portable -> %DESKTOP%\AAct)
  kmsoffline/    app.json + kmsoffline-2.4.7.7z        (kind: portable -> %DESKTOP%\KMSOffline)
```

## The installer / archive files are NOT in git

`*.exe`, `*.msi`, `*.7z`, `*.zip` under `apps/` are gitignored. Only the `app.json`
manifests are tracked. Put the actual files in each folder on the USB drive.
`build-release.ps1` copies whatever is present into `dist/`.

If a manifest's `installer`/`archive` file is missing, that app is **skipped with a clear
message** ("installer file not found: …") — it is not silently treated as installable.

## Manifest reference

See [../docs/PROFILES.md](../docs/PROFILES.md#local-apps-on-the-usb-drive-apps) for the
full `app.json` schema (both `kind: installer` and `kind: portable`).

## `.7z` portable apps need 7za.exe

Extracting a `.7z` archive requires `tools/7za.exe` (bundled in the release) or 7-Zip
installed on the target. `.zip` archives use the built-in `tar` and need nothing.

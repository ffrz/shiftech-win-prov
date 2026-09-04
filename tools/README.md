# tools/

Small bundled helpers copied into `dist/` by `scripts/build-release.ps1`.

## `7za.exe` (you add this)

The standalone 7-Zip CLI — needed to extract `.7z` **portable** apps
(`apps/<id>/app.json` with `"kind": "portable"` and a `.7z` archive) on machines that do
**not** have 7-Zip installed. `.zip` portable apps use the Windows built-in `tar` and need
nothing here.

Get it from the **7-Zip Extra** package: <https://www.7-zip.org/download.html> →
"7-Zip Extra: standalone console version …" → the `7za.exe` inside (~500 KB, free, LGPL).
Drop it here as `tools/7za.exe`.

`7za.exe` is a single file (no `7z.dll`), so it stays portable on the USB drive.

`.exe` / `.dll` in this folder are gitignored — this README and the download instructions
are what's tracked.

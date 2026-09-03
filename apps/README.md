# apps/ — local installers for the USB drive

Each subfolder is one app the provisioner can install without winget. Referenced from a
profile as `{ "id": "<folder>", "source": "local" }`.

```
apps/
  winrar/
    app.json          <- manifest (committed here as a template)
    winrar-x64-701.exe <- the actual installer (NOT committed; add it on the USB drive)
  adobe-reader/
    app.json
    AcroRdrDCx64.exe
```

`app.json` fields — see [../docs/PROFILES.md](../docs/PROFILES.md#local-installers-on-the-usb-drive):

| field | meaning |
|-------|---------|
| `name` | display name |
| `installer` | file next to `app.json`, **`.exe` or `.msi` only** |
| `silentArgs` | args for a silent install (`/S`, `/qn`, `/sAll`, …) |
| `detect.type` | `registry` / `file` / `arp` |
| `detect.keys` | registry key paths or file paths to check |
| `detect.name` | (arp) Add/Remove Programs display-name substring |
| `expectedExitCodes` | success codes (default `[0, 1641, 3010]`; 3010/1641 ⇒ reboot) |

The `.exe`/`.msi` files themselves are gitignored — put them on the flash drive next to
the manifest. `provisioner.exe drivers`/`apps` resolve `apps/` relative to the executable.

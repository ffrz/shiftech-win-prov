# Provisioning profiles

A profile is a **checklist** a technician picks before a run. Three sections, each item
individually toggleable (`enabled`), mirroring the DriverPack-style workflow:

- **drivers** — how to resolve drivers (provider chain), plus per-device include/exclude
- **applications** — winget packages and/or local installers from the USB drive
- **config** — tested Windows tweaks (clean taskbar, disable password expiry, …)

Shipped in `profiles/`, loaded by `ProfileLoader`, chosen with `--profile <name>`
(CLI) or the profile dropdown (GUI). Format is **JSON only** (ADR-0002).

---

## File shape

```json
{
  "name": "standard",
  "description": "Standard workstation",

  "drivers": {
    "enabled": true,
    "providerOrder": "localcache,windowsupdate,mirror",
    "installUnsigned": false,
    "exclude": ["USB\\VID_0BDA&PID_8153"]
  },

  "applications": [
    { "id": "Google.Chrome",   "source": "winget", "wingetId": "Google.Chrome",  "enabled": true,  "required": true },
    { "id": "7zip",            "source": "winget", "wingetId": "7zip.7zip",       "enabled": true,  "required": true },
    { "id": "winrar",          "source": "local",  "enabled": true,  "required": false },
    { "id": "adobe-reader",    "source": "local",  "enabled": true,  "required": false },
    { "id": "vlc",             "source": "winget", "wingetId": "VideoLAN.VLC",    "enabled": false, "required": false }
  ],

  "config": [
    { "id": "disable-password-expiry",  "enabled": true },
    { "id": "clean-taskbar-pins",       "enabled": true },
    { "id": "show-file-extensions",     "enabled": true },
    { "id": "disable-startup-item",     "enabled": false, "args": { "match": "OneDrive" } }
  ]
}
```

### `drivers`
| Field | Meaning |
|-------|---------|
| `enabled` | run the driver stages at all |
| `providerOrder` | comma-separated chain (default `localcache,windowsupdate,mirror`) |
| `installUnsigned` | if `true`, install packages with no catalog anyway (default `false` — ADR-0006 warn+skip) |
| `exclude` | Hardware/Instance IDs to leave alone even if they need a driver |

### `applications[]`
| Field | Meaning |
|-------|---------|
| `id` | unique key within the profile; also the local-app folder name for `source: "local"` |
| `source` | `"winget"` or `"local"` |
| `wingetId` | winget package Id (required when `source: "winget"`) |
| `enabled` | include this app in the run (the checkbox) |
| `required` | a failure here → "SUCCESS WITH WARNINGS"; optional failures are informational |

### `config[]`
| Field | Meaning |
|-------|---------|
| `id` | one of the built-in tweak ids (see the catalog below) |
| `enabled` | apply this tweak |
| `args` | tweak-specific parameters (only some tweaks take them) |

Unknown top-level keys, unknown tweak ids, duplicate app ids, or a `local` app with no
`apps/<id>/app.json` on disk → validation error (fail fast).

---

## Local apps on the USB drive (`apps/`)

```
apps/
  winrar/        app.json + winrar-x64-550.exe       (kind: installer)
  adobe-reader/  app.json + AcroRdrDC....exe          (kind: installer)
  7zip/          app.json + 7z2408-x64.exe            (kind: installer)
  wu10man/       app.json + Wu10Man_2.1.0.msi         (kind: installer)
  aact/          app.json + aact-4.0-portable.7z      (kind: portable)
  kmsoffline/    app.json + kmsoffline-2.4.7.7z       (kind: portable)
```

The folder name is the app **id**. Profiles reference it as
`{ "id": "<folder>", "source": "local", "enabled": true }`.
The installer / archive files are **gitignored** — you drop them in on the USB drive.
A missing installer/archive or a bad `app.json` ⇒ the app is **skipped with a clear
reason** (not silently "would install").

### `kind: "installer"` — run an .exe/.msi

```json
{
  "name": "WinRAR",
  "kind": "installer",
  "installer": "winrar-x64-550.exe",
  "silentArgs": ["/S"],
  "detect": {
    "type": "registry",
    "keys": ["HKLM\\SOFTWARE\\WinRAR", "HKLM\\SOFTWARE\\WOW6432Node\\WinRAR"]
  },
  "expectedExitCodes": [0]
}
```

| Field | Meaning |
|-------|---------|
| `installer` | file next to `app.json`. **`.exe` or `.msi` only** |
| `silentArgs` | silent-install args (`/S`, `/sAll /rs /msi EULA_ACCEPT=YES`, …). MSI always gets `/qn /norestart` too |
| `detect.type` | `registry` (a listed key exists) · `file` (a listed path exists) · `arp` (Add/Remove Programs display-name contains `detect.name`) |
| `expectedExitCodes` | success codes (default `[0, 1641, 3010]`; `3010`/`1641` ⇒ reboot) |

### `kind: "portable"` — extract an archive to a folder

For tools that ship as a `.zip` / `.7z` and just need unpacking (activators, portable
apps). `.zip` uses the built-in `tar`; **`.7z` needs `7za.exe`** — bundled in the release
(`tools/7za.exe`, see [tools/README.md](../tools/README.md)), or 7-Zip installed on the
target.

```json
{
  "name": "KMSOffline (activation tool)",
  "kind": "portable",
  "archive": "kmsoffline-2.4.7.7z",
  "extractTo": "%DESKTOP%\\KMSOffline",
  "flattenSingleRoot": true,
  "shortcutExe": "KMSoffline_x64.exe",
  "shortcutName": "KMSOffline",
  "detect": { "type": "folder", "keys": ["%DESKTOP%\\KMSOffline"] }
}
```

| Field | Meaning |
|-------|---------|
| `archive` | `.zip` or `.7z` next to `app.json` |
| `extractTo` | destination; supports `%USERPROFILE%`, `%DESKTOP%`, `%PUBLIC%`, `%PUBLIC_DESKTOP%`, `%PROGRAMDATA%`, `%PROGRAMFILES%`, and generic `%ENV%` |
| `flattenSingleRoot` | if the archive is one wrapper folder, hoist its contents up into `extractTo` |
| `shortcutExe` | optional; path (relative to `extractTo`) — a Desktop shortcut is created to it |
| `shortcutName` | shortcut file name (default = `name`) |
| `detect.type: "folder"` | "installed" = `extractTo` (or a listed path) exists and is non-empty |

`LocalInstallerProvider` never runs anything that isn't the declared installer/archive
tool. `provisioner reset` does **not** auto-undo local apps — remove installers via
Add/Remove Programs, portable folders by deleting them.

---

## Config tweak catalog (built-in, tested)

Each tweak knows how to **apply**, **check if already applied**, and describes what it
changes (logged). None disable Defender / signature enforcement / do security bypasses.

| id | What it does | Mechanism | `args` |
|----|--------------|-----------|--------|
| `disable-password-expiry` | local accounts: password never expires | `net accounts /maxpwage:unlimited` + `wmic UserAccount set PasswordExpires=false` | — |
| `clean-taskbar-pins` | remove pinned taskbar apps (leaves Start/Search/Task View) | clear `HKCU\...\Taskband` + restart explorer | — |
| `clean-startup-items` | disable all non-Microsoft startup entries | set `HKCU`/`HKLM` `...\Run` values' `StartupApproved` bytes | — |
| `disable-startup-item` | disable one startup entry by name | same, filtered | `match` (substring, required) |
| `show-file-extensions` | Explorer: show known file extensions | `HKCU\...\Advanced` `HideFileExt=0` | — |
| `show-hidden-files` | Explorer: show hidden files | `HKCU\...\Advanced` `Hidden=1` | — |
| `disable-fast-startup` | turn off hybrid shutdown | `HKLM\...\Power` `HiberbootEnabled=0` | — |
| `set-power-high-performance` | active power plan → High performance | `powercfg /setactive SCHEME_MIN` | — |
| `disable-hibernate` | remove hiberfil.sys | `powercfg /hibernate off` | — |
| `set-timezone` | set the system time zone | `tzutil /s "<id>"` | `id` (e.g. `"SE Asia Standard Time"`, required) |
| `enable-rdp` | allow Remote Desktop + firewall rule | `HKLM\...\Terminal Server` `fDenyTSConnections=0` + `netsh advfirewall` | — |
| `set-computer-name` | rename the computer (reboot required) | `Rename-Computer` | `name` (required) |

Tweaks needing elevation fail clearly when the run isn't elevated (like the driver stages).
`disable-password-expiry`, `clean-startup-items`, `enable-rdp`, `set-computer-name` need
elevation; the Explorer/`HKCU` ones don't.

---

## Shipped profiles

| File | Sections |
|------|----------|
| `profiles/standard.json` | drivers (WU chain), core apps (Chrome/7zip/WinRAR/Reader), light config |
| `profiles/office.json` | + office suite, RDP off, timezone set |
| `profiles/technician.json` | diagnostic tools, high-performance power, RDP on |
| `profiles/developer.json` | dev tools, show extensions/hidden, fast-startup off |

Edit the `enabled` flags to taste, or (once the GUI 3-tab picker lands) tick them per run.

---

## Behaviour recap

- Driver stage: chain resolve → download → extract → validate → install → verify. `exclude`
  IDs skipped. `installUnsigned` overrides the warn+skip default.
- App stage: for each `enabled` app, detect → skip if present, else install
  (winget silent, or local installer + `silentArgs`), retry once on transient, continue
  past failures. `required` drives the warnings status.
- Config stage: for each `enabled` tweak, check → skip if already applied, else apply,
  record before/after in the log. A tweak failure is a warning, never fatal.
- Report gains a **Config** section (applied / already / failed / skipped).

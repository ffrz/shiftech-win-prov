# Provisioning profiles

A profile is a **checklist** a technician picks before a run. Three sections, each item
individually toggleable (`enabled`), mirroring the DriverPack-style workflow:

- **drivers** — how to resolve drivers (provider chain), plus per-device include/exclude
- **applications** — local installers from the USB drive, each with an optional winget
  fallback (local is tried first)
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
    { "id": "chrome",       "local": "chrome", "winget": "Google.Chrome", "enabled": true,  "required": true },
    { "id": "zoom",         "local": "zoom",   "winget": "Zoom.Zoom",     "enabled": true,  "required": false },
    { "id": "7zip",         "local": "7zip",                              "enabled": true,  "required": true },
    { "id": "winrar",       "local": "winrar",                            "enabled": true,  "required": false },
    { "id": "adobe-reader", "local": "adobe-reader",                      "enabled": true,  "required": false }
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

One entry, up to **two sources**. The provisioner tries `local` first and only falls
back to `winget` when the local installer is absent or fails.

| Field | Meaning |
|-------|---------|
| `id` | unique display key within the profile |
| `local` | `apps/<local>/` folder id — omit for a winget-only app |
| `winget` | winget package Id — omit for a local-only app (e.g. `7zip`) |
| `enabled` | include this app in the run (the checkbox) |
| `required` | a failure here → "SUCCESS WITH WARNINGS"; optional failures are informational |

At least one of `local` / `winget` must be present. **Resolution order per app:**
`local` (if `apps/<local>/app.json` + payload present) → `winget` (if available) →
if winget is missing, bootstrap it from `tools/winget/` then retry → otherwise the app
is **skipped with a clear reason**.

Legacy `{ "id", "source": "winget"|"local", "wingetId" }` entries are still accepted
(`source: "local"` ⇒ `local` = `id`; `source: "winget"` ⇒ `winget` = `wingetId` or `id`).

### `config[]`
| Field | Meaning |
|-------|---------|
| `id` | one of the built-in tweak ids (see the catalog below) |
| `enabled` | apply this tweak |
| `args` | tweak-specific parameters (only some tweaks take them) |

Unknown top-level keys, unknown tweak ids, duplicate app ids, or an app entry with
neither `local` nor `winget` → validation error (fail fast). A `local` id whose
`apps/<id>/app.json` is missing is not a load-time error — the app is skipped at run
time (with `winget` used instead when present).

---

## Local apps on the USB drive (`apps/`)

```
apps/
  chrome/        app.json + ChromeStandaloneSetup64.exe   (installer)
  zoom/          app.json + ZoomInstallerFull.msi          (installer)
  firefox/       app.json + "Firefox Setup.exe"            (installer)
  winrar/        app.json + winrar-x64-550.exe             (kind: installer)
  adobe-reader/  app.json + AcroRdrDC....exe               (kind: installer)
  7zip/          app.json + 7z2408-x64.exe                 (kind: installer)
  wu10man/       app.json + Wu10Man_2.1.0.msi              (kind: installer)
  aact/          app.json + "AAct 4.0 Portable.kuyhAa.7z"    (kind: portable)
  kmsoffline/    app.json + KMSOffline_2.4.7.kuyhAa.7z       (kind: portable)
  office-2016/   app.json + config.xml + office-2016-proplusvl-x86.iso   (kind: iso)
  office-2019/   app.json + configuration.xml + setup.exe (ODT) + <2019 VL>.iso  (kind: iso)
```

The folder name is the app **id**. Profiles reference it as
`{ "id": "<folder>", "local": "<folder>", "enabled": true }` (optionally with a
`"winget"` fallback).
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

### `kind: "iso"` — mount an .iso/.img and run its setup

For products delivered as a disc image — chiefly **Microsoft Office** volume media.
The provisioner mounts the image with `Mount-DiskImage` (native Windows 8+, no driver),
runs the setup program with your silent answer file, then always dismounts — even if
setup fails.

```json
{
  "name": "Microsoft Office 2016 (ProPlus VL, MSI)",
  "kind": "iso",
  "image": "office-2016-proplusvl-x86.iso",
  "setup": "setup.exe",
  "setupArgs": ["/config", "%APP%\\config.xml"],
  "detect": {
    "type": "registry",
    "keys": ["HKLM\\SOFTWARE\\Microsoft\\Office\\16.0\\Common\\InstallRoot"]
  },
  "expectedExitCodes": [0, 3010]
}
```

| Field | Meaning |
|-------|---------|
| `image` | `.iso` or `.img` next to `app.json` (gitignored — staged on the USB drive) |
| `setup` | setup program to run (default `setup.exe`) |
| `setupFrom` | `"iso"` (default — `setup` is relative to the mounted image root) or `"app"` (`setup` is a program bundled in `apps/<id>/`, e.g. the Office Deployment Tool; the image only supplies payload) |
| `setupArgs` | args passed to setup. `%ISO%` ⇒ the mounted drive root (e.g. `D:\`), `%APP%` ⇒ the `apps/<id>/` folder |
| `detect.type` | same options as `installer` — usually `registry` on the Office `InstallRoot` key |
| `expectedExitCodes` | success codes (default `[0, 1641, 3010]`) |

The Office answer files (`config.xml` / `configuration.xml`) live in each
`apps/office-*/` folder and **are tracked in git** — only the ISO is not. See
`apps/office-2016/README.md` (etc.) for the exact media each expects and how to build
a MAK answer file. No auto-reboot (V1 rule) regardless of the answer file.

`LocalInstallerProvider` never runs anything that isn't the declared installer/archive/
setup tool. `provisioner reset` does **not** auto-undo local apps — remove installers via
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

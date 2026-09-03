# CLI — provisioner.exe

The CLI is a thin front-end over `shiftech_core`. Every capability is reachable here.
It subscribes to `ProvisioningEvent`s and prints them; the GUI is the same engine with a
different front-end.

Exit codes: `0` success · `1` success with warnings · `2` fatal environment error ·
`3` bad usage.

## Commands (all implemented)

### `provisioner scan [--json]`
System header + a table of **all** present devices; devices needing a driver
(no driver / problem code / unknown) grouped and flagged first.

```
Shiftech Win Provisioner - scan

System
  Windows 11 Pro (build 22621)  x64   Elevated: no
  winget: yes   pnputil: yes

Devices needing a driver (2)
  [UNKNOWN]  ACPI\VEN_PNP&DEV_0A0A   Unknown Device   (problem 28)
  ...
```
`--json` → `{ system, devices:[Device…], needingDriver:[…] }`.
Exit 1 if any device needs a driver.

### `provisioner drivers scan [--json] [--provider <name>] [--driver-index <path>]`
Runs the **provider chain** (`localcache → windowsupdate → mirror`) per device and prints
the match or `NOT FOUND (<aggregated reasons>)`. `--provider mock --driver-index <file>`
forces a single mock provider; `--provider localcache` etc. forces one chain entry.

### `provisioner drivers resolve [--download] [--json] [--provider-order <csv>] [--cache-dir <p>] [--driver-index <p>] [--mirror-url <u>]`
Resolve every needy device via the chain. With `--download`, fetch the winning package
into the **portable cache** (`<cache>/drivers/<packageId>/…` + `index.json`) and report
`resolved` / `downloaded` / `cache hit` / `not found`. Never aborts on one failure.

### `provisioner drivers install [--dry-run] [--json] [--only <instanceId>] [--provider-order <csv>] [--cache-dir <p>] [--driver-index <p>]`
Per device: resolve → download → extract (zip/cab/folder) → `InfValidator` → (dry-run
stops here) → `pnputil /add-driver /install` per INF → re-enumerate + verify.
Unsigned/invalid INFs are skipped (ADR-0006). **Requires elevation** for a real install;
`--dry-run` works unelevated. Reboot-required is reported, never auto-actioned.

### `provisioner apps install --profile <name> [--dry-run] [--json] [--profiles-dir <dir>]`
Load `profiles/<name>.json`, per app `winget list` → skip if present, else
`winget install --silent …` (one retry on a transient failure). winget missing → every
app `skipped_no_winget`. Exit 1 only if a **required** app fails or all are skipped.

### `provisioner provision --profile <name> [--dry-run] [--skip-drivers] [--skip-apps] [--json] [--provider-order <csv>] [--cache-dir <p>] [--profiles-dir <p>] [--log-dir <p>] [--driver-index <p>]`
The full pipeline: System Check → Hardware Scan → Driver{Analysis,Resolution,Download,
Install,Verify} → App{Detection,Install} → Final Verify → Report → Done. Streams
`[HH:MM:SS] LEVEL  message  (NN%)` events; writes `logs/<runId>/{run.json,state.json}`;
prints the report. Exit 0/1/2 = SUCCESS / SUCCESS WITH WARNINGS / FAILED.

### `provisioner report [--last | --run <id>] [--json] [--log-dir <p>]`
Replay a past run's report from its `run.json`.

## Output contract

- Human: one line per event `[HH:MM:SS] LEVEL  message`, `(NN%)` for stage progress, then
  the report block.
- `--json`: a single JSON object.
- Every `provision` run writes `logs/<runId>/` regardless of `--json`.
- Paths (`cache/`, `logs/`, `profiles/`) resolve relative to the executable by default so
  the whole folder is portable (USB drive).

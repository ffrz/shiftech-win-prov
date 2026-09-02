# CLI — provisioner.exe

The CLI is a thin front-end over `shiftech_core`. Every capability is reachable here
before any GUI work. It subscribes to `ProvisioningEvent`s and prints them.

## Global options

| Option | Meaning |
|--------|---------|
| `--json` | emit machine-readable JSON to stdout instead of human text |
| `--log-dir <path>` | override `logs/` |
| `--cache-dir <path>` | override `cache/` |
| `--verbose` / `-v` | debug-level console output |
| `--yes` / `-y` | assume yes for confirmations |
| `--no-color` | disable ANSI color |

Exit codes: `0` success, `1` success with warnings, `2` fatal environment error,
`3` bad usage.

## Commands

### `provisioner.exe scan`  *(Milestone 1)*
Prints system info header + a table of **all** present devices. Devices that need a driver
(no driver / problem code / unknown) are grouped and clearly flagged at the top.

```
Shiftech Win Provisioner — scan

System
  Windows 11 Pro (build 22621)  x64   UEFI   Elevated: no
  winget: yes   pnputil: yes   internet: yes

Devices needing a driver (2)
  [NO DRIVER]  PCI\VEN_10EC&DEV_8168        Ethernet Controller        (problem 28)
  [UNKNOWN ]  USB\VID_0BDA&PID_8153        Unknown Device             (problem 28)

All devices (42)
  [OK]  Display   Intel(R) Iris(R) Xe Graphics       31.0.101.4502  Intel  2023-08-01
  ...
```

`--json` emits `{ system: {...}, devices: [ {Device...} ], needingDriver: [...] }`.

### `provisioner.exe drivers scan`  *(Milestone 2)*
Like `scan` but only the driver-relevant view, plus provider resolution results
(what the configured `DriverProvider` found for each Hardware ID).

### `provisioner.exe drivers install [--dry-run] [--only <instanceId>]`  *(Milestone 3–4)*
Resolve → download → `pnputil` install → verify, for every device needing a driver.
`--dry-run` stops before `pnputil`. Per-device status printed; one failure never aborts.

### `provisioner.exe apps install --profile <name> [--dry-run]`  *(Milestone 5)*
Load `profiles/<name>.yaml|json`, detect installed, install missing via winget.

### `provisioner.exe provision --profile <name> [--dry-run] [--skip-drivers] [--skip-apps]`  *(Milestone 6)*
Full pipeline (System Check → … → Report).

### `provisioner.exe report [--last | --run <id>] [--json]`
Print the report for the most recent (or a specific) run from `logs/`.

### `provisioner.exe system`  
Print only the `SystemInspector` result (used for pre-flight checks).

## Output contract

- Human mode: one line per event, `[HH:MM:SS] SEVERITY  message`, progress bars for stages.
- `--json` mode: a single JSON object at the end, plus optional NDJSON event stream on
  stderr when `--verbose`.
- Every run also writes `logs/<timestamp>.json` regardless of `--json`.

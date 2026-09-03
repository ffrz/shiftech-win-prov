# Roadmap

Build and test at the end of every milestone. Report a change summary + test results, then
wait for review before starting the next one. Do not jump ahead.

Each milestone has a concrete step-by-step checklist in
[tasks/](tasks/) — `tasks/MILESTONE-<n>.md`. Give the implementing agent that file when the
previous milestone is accepted. The scaffold's `GEMINI_INITIAL_PROMPT.md` (repo root) is
the kickoff for Milestone 1.

## Milestone 1 — Skeleton + hardware detection ✅ DONE (2026-09-03)
**Scope:** root `CMakeLists.txt`; `shiftech_core` lib; `provisioner` exe; test suite.
`SystemInspector`. `DeviceEnumerator` + `Device` via SetupAPI/CfgMgr32. `provisioner scan`.
**Result:** clean `-Werror` build; `scan` lists 128 devices / flags 2; tests green.
Details + deviations in [tasks/MILESTONE-1.md](tasks/MILESTONE-1.md).

## Milestone 2 — DriverProvider abstraction + mock ✅ DONE (2026-09-03)
**Scope:** `DriverProvider` interface, `TargetSystem`, `MockDriverProvider`,
`provisioner drivers scan`.
**Result:** `pickBest` ranks HardwareId > CompatibleId (via new `matchedVia` field);
`drivers scan` + `--json` work; `driverpack` provider gated (ADR-0007).
Details in [tasks/MILESTONE-2.md](tasks/MILESTONE-2.md).

*Also present (Gemini, hardened): profiles (`ProfileLoader`, JSON-only per ADR-0002),
`WinGetProvider`, `provisioner apps install --profile <name> [--dry-run]` — this is
Milestone 5 groundwork, working and tested but not yet formally reviewed against
[tasks/MILESTONE-5.md](tasks/MILESTONE-5.md).*

## Milestone 3 — Portable driver cache + downloader + provider chain ✅ DONE (2026-09-03)
**Scope:** `DriverCache` (deterministic portable id), `DriverDownloader` (retry/resume/
checksum/`file://`), `LocalCacheProvider`, `ProviderChain` + `DriverProviderFactory`,
`WindowsUpdate`/`Mirror` stubs, `provisioner drivers resolve [--download]`.
**Result:** 11 test suites green; portable cache verified end-to-end on this machine
(download → relocate → resolve offline). DriverPack rejected (ADR-0007).
Real online providers are [tasks/MILESTONE-3.5.md](tasks/MILESTONE-3.5.md) (deferrable).
Details in [tasks/MILESTONE-3.md](tasks/MILESTONE-3.md).

## Milestone 3.5 — Real WindowsUpdate + Mirror providers ✅ DONE (2026-09-03)
**Scope:** `WindowsUpdateProvider` (COM `IUpdateSearcher`, IDispatch late-binding for
driver props), `MirrorProvider` (HTTP/file `index.json`).
**Result:** WU query verified live on this machine; `test_mirrorprovider` (5 cases) green;
19 suites total; no stub reasons left in the chain. Offline `wsusscn2.cab` scanning
deferred (MinGW headers lack `IUpdateServiceManager`). Details in
[tasks/MILESTONE-3.5.md](tasks/MILESTONE-3.5.md).

## Milestone 4 — Driver installation + verification ✅ DONE (2026-09-03)
**Scope:** `PackageExtractor` (zip/cab/folder), `InfValidator` (security gate + ADR-0006
unsigned→skip), `DriverInstaller` (`pnputil /add-driver /install` per INF, elevation-gated,
per-INF outcome), `DriverVerifier` (re-enumerate, `classifyTransition` →
`DriverInstallStatus`), `provisioner drivers install [--dry-run] [--only <id>]`.
**Result:** 15 test suites green; full dry-run pipeline verified on this machine with a
signed fixture zip. Real install is elevation-gated and left for a VM (procedure in the
task file). Details in [tasks/MILESTONE-4.md](tasks/MILESTONE-4.md).

## Milestone 5 — Application provisioning ✅ DONE (2026-09-03)
**Scope:** `ApplicationProvider` iface, `WinGetProvider` (retry-on-transient),
`ProfileLoader` + validation (JSON, ADR-0002), shipped profiles,
`provisioner apps install --profile <name> [--dry-run] [--json]`.
**Result:** `apps install --dry-run` works on this machine; winget-unavailable →
all-skipped + warnings exit; required-vs-optional failure semantics; 15 suites green.
Details in [tasks/MILESTONE-5.md](tasks/MILESTONE-5.md).

## Milestone 6 — Provisioning pipeline ✅ DONE (2026-09-03)
**Scope:** `ProvisioningEvent` + `EventSink`, `ProvisioningState` (14-stage machine +
JSON), `ReportBuilder` (Report + SUCCESS/WARNINGS/FAILED), `StructuredLogger`
(`logs/<runId>/{run.json,state.json}` + redaction), `ProvisioningEngine` (full pipeline),
`provisioner provision [--dry-run] [--skip-drivers] [--skip-apps] [--json]`,
`provisioner report [--last|--run <id>] [--json]`.
**Result:** full dry-run pipeline verified end-to-end on this machine (events, report, log
dir, state history); skip modes work; 18 test suites green. Business logic entirely in
`provisioning/`. Details in [tasks/MILESTONE-6.md](tasks/MILESTONE-6.md).

## Milestone 7 — Qt GUI ✅ DONE (2026-09-03)
**Scope:** `shiftech_gui` (Qt Widgets, opt-in `-DSHIFTECH_BUILD_GUI=ON`),
`EngineController` (engine on a QThread → Qt signals), `MainWindow` dashboard (system
panel, profile picker, dry-run/skip toggles, driver/app progress bars, current task,
Start/Cancel/Save-report, coloured log + report panes).
**Result:** GUI launches and drives a dry-run with live progress (screenshot verified);
`src/core` diff = the sanctioned cancel token only (24 lines); `test_mainwindow_smoke`
green (19 suites total); ADR-0003 resolved (CLI-only on Win7/8). Manual QA:
[GUI_QA.md](GUI_QA.md). Details in [tasks/MILESTONE-7.md](tasks/MILESTONE-7.md).

**V1 is feature-complete.**

## Milestone 8 — Checklist profiles: local installers + config tweaks ✅ DONE (2026-09-03)
**Scope:** 3-section profile format (`drivers` / `applications[] enabled` / `config[]`),
`LocalInstallerProvider` (`apps/<id>/app.json` on the USB drive), config tweak catalog
(12 tested tweaks), engine Config stage, `provisioner config list`.
**Result:** `provision --dry-run` runs all three sections; backward-compatible with the
old profile format; 22 test suites green. Details in
[tasks/MILESTONE-8.md](tasks/MILESTONE-8.md).

## Milestone 9 — GUI 3-tab checklist picker ✅ DONE (2026-09-04)
**Scope:** `ChecklistTabs` (Drivers / Applications / Config tabs, tick which items to run),
Start runs an in-memory effective `Profile`. **Picker, not editor** — read-only tables,
no profile editing (edit the `.json` for that).
**Result:** the DriverPack-style picker works; 22 test suites green; one thin core field
(`ProvisioningOptions::profileObject`). Details in
[tasks/MILESTONE-9.md](tasks/MILESTONE-9.md).

**All milestones (1–9) complete.**

## Out of scope for V1
Auto-reboot; resume-after-reboot execution (design only); ARM; a service host; any
server/backend; GUI-click automation.

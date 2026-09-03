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

## Milestone 3.5 — Real WindowsUpdate + Mirror providers (deferrable)
**Scope:** replace the two chain stubs with working implementations (COM `IUpdateSearcher`
for Windows Update; internal HTTP/share mirror with a JSON index).
**Not on the critical path** to an end-to-end dry run — sequence it whenever a real online
driver source is actually needed. See [tasks/MILESTONE-3.5.md](tasks/MILESTONE-3.5.md).

## Milestone 4 — Driver installation + verification ✅ DONE (2026-09-03)
**Scope:** `PackageExtractor` (zip/cab/folder), `InfValidator` (security gate + ADR-0006
unsigned→skip), `DriverInstaller` (`pnputil /add-driver /install` per INF, elevation-gated,
per-INF outcome), `DriverVerifier` (re-enumerate, `classifyTransition` →
`DriverInstallStatus`), `provisioner drivers install [--dry-run] [--only <id>]`.
**Result:** 15 test suites green; full dry-run pipeline verified on this machine with a
signed fixture zip. Real install is elevation-gated and left for a VM (procedure in the
task file). Details in [tasks/MILESTONE-4.md](tasks/MILESTONE-4.md).

## Milestone 5 — Application provisioning 🟡 PARTIAL (groundwork done, review pending)
**Scope:** `ApplicationProvider` iface, `WinGetProvider`. `ProfileLoader` + validation.
Shipped profiles. `provisioner.exe apps install --profile <name>`.
**Status:** all of the above exist and pass tests (`test_profileloader`, `test_wingetparse`);
`apps install --dry-run` works on this machine. Still to do for a full M5:
per-app retry-on-transient wired through the command, a `required`-failure → warnings exit
code, integration test on a VM. Do a proper pass against
[tasks/MILESTONE-5.md](tasks/MILESTONE-5.md) after M3/M4.
**Exit criteria:** profile parsing unit tests pass ✅; `apps install --dry-run` lists
correct install/skip decisions ✅; a failing package doesn't stop the rest (needs verifying).

## Milestone 6 — Provisioning pipeline
**Scope:** `ProvisioningEngine`, full pipeline, `ProvisioningState` state machine,
`ProvisioningEvent` stream, `StructuredLogger` JSON output, report builder (human + JSON).
`provisioner.exe provision --profile <name>`. `provisioner.exe report`.
**Exit criteria:** end-to-end `provision --dry-run` on this machine produces a full log +
report; state persisted at every transition; provisioning-state and error-handling unit
tests pass.

## Milestone 7 — Qt GUI
**Scope:** `shiftech_gui` (Qt Widgets), dashboard per the initial spec (system info,
driver/app progress bars, current task, Pause/Cancel), log view, report view. GUI is a
pure front-end over engine events — no business logic in widgets.
**Exit criteria:** GUI drives a `--dry-run` provision run and shows live progress; no
`src/core` change was needed to add it (proves the event API is sufficient). Revisit the
Windows 7 Qt 6 risk (ADR-0003) here.

## Out of scope for V1
Auto-reboot; resume-after-reboot execution (design only); ARM; a service host; any
server/backend; GUI-click automation.

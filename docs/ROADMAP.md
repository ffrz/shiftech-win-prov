# Roadmap

Build and test at the end of every milestone. Report a change summary + test results, then
wait for review before starting the next one. Do not jump ahead.

Each milestone has a concrete step-by-step checklist in
[tasks/](tasks/) — `tasks/MILESTONE-<n>.md`. Give the implementing agent that file when the
previous milestone is accepted. The scaffold's `GEMINI_INITIAL_PROMPT.md` (repo root) is
the kickoff for Milestone 1.

## Milestone 1 — Skeleton + hardware detection
**Scope:** root `CMakeLists.txt`; `shiftech_core` lib target; `provisioner` exe target;
`shiftech_tests` target (all building). `SystemInspector` (basic). `DeviceEnumerator` +
`Device` model via SetupAPI/CfgMgr32. `provisioner.exe scan`.
**Exit criteria:**
- `scripts\build.bat` succeeds clean.
- `provisioner.exe scan` on this machine lists devices and flags those needing a driver.
- Unit tests pass: Hardware ID parsing, device classification.

## Milestone 2 — DriverProvider abstraction + mock
**Scope:** `DriverProvider` interface, `TargetSystem`, result types. `MockDriverProvider`
backed by `tests/fixtures/driver_index.json`. `provisioner.exe drivers scan` showing
resolution results.
**Exit criteria:** driver matching unit tests pass; `drivers scan` prints per-device
provider results; no real DriverPack code.

## Milestone 3 — DriverPack investigation + downloader + cache
**Scope:** complete the checklist in [DRIVER_PROVIDER.md](DRIVER_PROVIDER.md), file ADR-0004.
`DriverDownloader` (progress, retry, timeout, checksum, resume, deterministic cache).
Real provider **only if** the investigation yields a reliable + permitted mechanism;
otherwise `WindowsUpdateProvider` / `MirrorProvider` proposal + mock stays.
**Exit criteria:** downloader unit tests (against local fixture server) pass; cache
dedupe verified; ADR-0004 written and reviewed.

## Milestone 4 — Driver installation + verification
**Scope:** `DriverInstaller` (`pnputil /add-driver /install` per INF, capture
stdout/stderr/exit, timeout, aggregate). `DriverVerifier` (re-enumerate, diff,
`DriverInstallStatus`). `provisioner.exe drivers install [--dry-run]`.
**Exit criteria:** `--dry-run` works on this machine; integration test (VM only, gated)
installs a benign INF and verifies; one failing INF does not abort the batch.

## Milestone 5 — Application provisioning
**Scope:** `ApplicationProvider` iface, `WinGetProvider`. `ProfileLoader` + validation.
Shipped profiles. `provisioner.exe apps install --profile <name>`.
**Exit criteria:** profile parsing unit tests pass; `apps install --dry-run` lists
correct install/skip decisions; a failing package doesn't stop the rest.

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

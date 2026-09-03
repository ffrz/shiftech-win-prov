# Milestone 6 — Provisioning Pipeline + Logging + Report

**Goal:** `ProvisioningEngine` orchestrates the full pipeline, emits structured events,
persists state at every transition, writes a JSON log, and produces a human + JSON report.

Prereq: Milestone 5 accepted. Read [../ARCHITECTURE.md](../ARCHITECTURE.md) §3–§6 and the
"Pipeline", "Error Handling", "Reboot Handling", "Logging", "Report" sections of
`../../initial-prompt.md`.

---

## Task 1 — Events + sink

- [ ] `src/core/provisioning/ProvisioningEvent.h`: struct exactly as in
      [../ARCHITECTURE.md](../ARCHITECTURE.md) §3 (`timestamp, category, severity, message,
      progress`).
- [ ] Event sink: a `std::function<void(const ProvisioningEvent&)>` on the engine (keeps
      core Qt-Widgets-free). If a Qt signal is also wanted, the engine may be a `QObject`
      emitting `void event(ProvisioningEvent)` — but the `std::function` path must work for
      non-Qt consumers.

## Task 2 — State machine + persistence

- [ ] `src/core/provisioning/ProvisioningState.h/.cpp`: `enum class Stage { Init,
      SystemCheck, HardwareScan, DriverAnalysis, DriverResolution, DriverDownload,
      DriverInstall, DriverVerify, AppDetection, AppInstall, FinalVerify, Report, Done,
      Failed }`. Legal-transition table; illegal transition is a hard error.
- [ ] Per-item results held in state: `map<instanceId, DriverInstallStatus>`,
      `map<appId, InstallResult-summary>`, counters, `bool rebootRequired`, timings.
- [ ] `toJson()` / `fromJson()` round-trip — this is the resume foundation (resume
      execution itself is V2; just persist faithfully).
- [ ] Engine writes state to `logs/<run>/state.json` after every transition.

## Task 3 — ProvisioningEngine

- [ ] `src/core/provisioning/ProvisioningEngine.h/.cpp`:
      `struct ProvisioningOptions { std::string profile; bool dryRun; bool skipDrivers;
      bool skipApps; std::string providerName; ... }`.
      `ProvisioningResult run(const ProvisioningOptions&)`.
- [ ] Stages in order (see [../ARCHITECTURE.md](../ARCHITECTURE.md) §5). Only fatal
      environment errors (unsupported OS, not elevated when drivers requested, cannot
      enumerate at all) → `Failed`. Every per-item driver/app failure is caught, logged,
      recorded, and the pipeline continues.
- [ ] Emit an event at the start/end of each stage and for each meaningful item, with a
      `progress` 0–100 for the current stage.
- [ ] `--dry-run`: resolve/plan everything, skip `pnputil` and `winget install`.

## Task 4 — StructuredLogger

- [ ] `src/core/logging/StructuredLogger.h/.cpp`: one run dir `logs/<YYYY-MM-DD_HHMMSS>/`
      with `run.json` (system info, all events, per-device + per-app results, durations,
      final report object) and `state.json`. Also mirror events to console at a level.
- [ ] **No credentials / PII.** Redact env-derived paths only if they contain a username?
      (document the policy — usernames in paths are acceptable; tokens/passwords never).

## Task 5 — Report builder

- [ ] `src/core/provisioning/ReportBuilder.h/.cpp`: from `ProvisioningState` →
      `struct Report { ... }` with the exact sections from the spec (Hardware / Drivers /
      Applications / Reboot required / Duration / Status).
- [ ] `Status`: `SUCCESS` (nothing failed), `SUCCESS WITH WARNINGS` (a not-found driver or a
      failed app, or a `required` app failed), `FAILED` (fatal env error).
- [ ] `toText()` and `toJson()`.

## Task 6 — CLI

- [ ] `provisioner.exe provision --profile <name> [--dry-run] [--skip-drivers]
      [--skip-apps] [--provider <p>]` — runs the engine, streams events to the console,
      prints the report at the end, writes the log dir. Exit 0 / 1 / 2.
- [ ] `provisioner.exe report [--last | --run <id>] [--json]` — reads a past run's
      `run.json` and prints the report.

## Task 7 — Tests

- [ ] Unit: state-machine legal/illegal transitions; state JSON round-trip;
      report-builder totals + status logic; error-handling (inject a failing driver + a
      failing app, assert the run completes and the report counts them).
- [ ] Unit: logger writes valid JSON, contains no injected fake secret.
- [ ] Integration (gated): full `provision --profile standard --dry-run` on this machine
      produces a complete `run.json` + report.
- [ ] `scripts\test.bat` green.

---

## Exit criteria — DONE (2026-09-03)

- [x] `provision --profile standard --dry-run` runs the whole pipeline on this machine:
      SystemCheck → HardwareScan → Driver{Analysis,Resolution,Download,Install,Verify} →
      App{Detection,Install} → FinalVerify → Report → Done. Streams `[HH:MM:SS] LEVEL msg
      (NN%)` events, prints the full report, writes `logs/<runId>/{run.json,state.json}`.
- [x] State persisted after every transition — `state.json` `stageHistory` shows the full
      ordered path ending at `Done` (or the stage reached before `Failed`).
- [x] `--skip-drivers` / `--skip-apps` walk the remaining stages correctly (SUCCESS /
      SUCCESS WITH WARNINGS as appropriate).
- [x] Per-item driver "not found" and app failures are recorded and do NOT abort the run;
      report → "SUCCESS WITH WARNINGS". Only a fatal env error (unsupported arch, not
      elevated for real install, empty enumeration, bad profile) → `Failed` / exit 2.
- [x] `report --last` and `report --run <id>` reproduce the report from `run.json`.
- [x] `--json` on `provision` and `report`.
- [x] No business logic in CLI classes — `ProvisioningEngine` owns the pipeline; the CLI
      only builds `ProvisioningOptions`, subscribes an `EventSink`, and renders the result.
- [x] Unit tests pass: `test_provisioningstate` (transitions + JSON round-trip),
      `test_reportbuilder` (totals + status logic), `test_structuredlogger` (valid JSON,
      secret redaction, latest-run lookup). 18 suites total.

### Deviations / notes
- Transition rule: strictly forward through the ordered `Stage` enum (skips allowed for
  `--skip-*`), never backward, no escape from `Done`/`Failed`. `transitionTo` throws
  `std::logic_error` on violation.
- `StructuredLogger::redact` scrubs `password/token/secret/api-key = <val>` and long
  hex/base64 blobs from event messages. Usernames in filesystem paths are kept (not
  secrets), per the task-file policy.
- Resume-after-reboot execution is V2; `state.json` is written faithfully so it is
  possible later, but the engine does not consume it yet.
- Real (non-dry-run) `provision` needs elevation for the driver stages; not exercised on
  the dev machine.

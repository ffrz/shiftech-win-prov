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

## Exit criteria

- [ ] `provision --profile standard --dry-run` on this machine runs the whole pipeline,
      streams events, prints a full report, writes a valid `logs/<run>/` dir.
- [ ] State is persisted after every transition (`state.json` shows the last stage).
- [ ] Injected driver + app failures are recorded and do NOT stop the run; report shows
      "SUCCESS WITH WARNINGS".
- [ ] `report --last` reproduces the report from the log.
- [ ] No business logic added to any CLI class — it all lives in `provisioning/`.
- [ ] Unit tests pass. Change summary. STOP for review.

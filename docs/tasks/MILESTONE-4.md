# Milestone 4 — Driver Installation + Verification

**Goal:** install driver packages via `pnputil` and verify the result by re-enumeration.
One failing package never aborts the batch.

Prereq: Milestone 3 accepted. Read [../WINDOWS_APIS.md](../WINDOWS_APIS.md) (drivers/) and
[../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md) "Security gate", [../TESTING.md](../TESTING.md)
(safety rules — **integration tests are VM-only, gated**).

---

## Task 1 — Package extraction

- [ ] `src/core/drivers/PackageExtractor.h/.cpp`: extract `zip`/`cab`/folder into
      `cache/drivers/<packageId>/extracted/`. Per ADR-0005: `tar.exe` for zip,
      `expand.exe` for cab, Shell fallback for old OS, folder = copy/no-op.
- [ ] `std::vector<std::string> findInfFiles(extractedDir)` — recursive, returns `*.inf`.
- [ ] All external calls (`QProcess`) have timeouts + captured output.

## Task 2 — INF sanity check (security gate)

- [ ] `src/core/drivers/InfValidator.h/.cpp` — pure-ish check: file parses, has `[Version]`,
      `Class`/`ClassGUID` present, `CatalogFile` referenced (warn if absent → unsigned),
      no obviously hostile directives (`RunOnce`, arbitrary `AddReg` to sensitive hives —
      warn/flag, don't hard-block unless clearly malicious).
- [ ] Result: `enum class InfVerdict { Ok, Warn, Reject }` + messages.

## Task 3 — DriverInstaller

- [ ] `src/core/drivers/DriverInstaller.h/.cpp`:
      - input: extracted dir + INF list (+ the `Device` for context);
      - for each INF: `pnputil /add-driver "<inf>" /install` via `QProcess`, capture
        stdout/stderr/exit code, per-INF timeout (default 180s);
      - aggregate: `struct InstallOutcome { std::string inf; int exitCode; std::string log;
        bool rebootRequired; bool ok; }`;
      - detect reboot-required from pnputil output / exit code `3010`;
      - **elevation check first** — if not elevated, return a clear failure, do not run;
      - one INF failure → record, continue to the next.
- [ ] Never execute a bundled `.exe`.

## Task 4 — DriverVerifier

- [ ] `src/core/drivers/DriverVerifier.h/.cpp`: takes a "before" device snapshot, re-runs
      `DeviceEnumerator`, matches by instance ID, and produces per device:
      `enum class DriverInstallStatus { AlreadyInstalled, Installed, Failed, NotFound,
      Skipped, RequiresReboot }` (exactly as in the initial spec).
      - problem code 28/other → 0 = `Installed`;
      - still problem, driver version unchanged = `Failed`;
      - `DN_NEED_RESTART` or installer said reboot = `RequiresReboot`.

## Task 5 — CLI

- [ ] `provisioner.exe drivers install [--dry-run] [--only <instanceId>] [--provider <p>]`:
      resolve → download → extract → validate → (dry-run stops here) → install → verify,
      for every device needing a driver. Per-device status table. Summary:
      already/installed/failed/notfound/skipped + `Reboot required: YES/NO`.
      Exit 0 / 1 (warnings) / 2 (fatal env, e.g. not elevated when install requested).

## Task 6 — Tests

- [ ] Unit: `InfValidator` verdicts on fixture INFs (ok / unsigned / hostile);
      `DriverVerifier` status logic on synthetic before/after snapshots;
      installer output parsing (reboot detection, exit-code mapping) with canned pnputil text.
- [ ] Integration (gated `SHIFTECH_INTEGRATION_TESTS=ON`, **VM only**): install a benign
      signed test INF, verify via re-enumeration, then `pnputil /delete-driver` cleanup.
- [ ] `scripts\test.bat` green (unit); integration documented as VM-run.

---

## Exit criteria

- [ ] `drivers install --dry-run` runs on this machine end-to-end (no actual install) and
      prints the plan + per-device status.
- [ ] One deliberately-broken package in the batch does not stop the others.
- [ ] Reboot-required is surfaced, never auto-actioned.
- [ ] Installer refuses to run unelevated with a clear message.
- [ ] Unit tests pass; integration test procedure documented and run on a VM (paste result).
- [ ] Change summary. STOP for review.

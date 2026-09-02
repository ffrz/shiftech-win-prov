# Milestone 5 — Application Provisioning (WinGet + Profiles)

**Goal:** install a profile's applications via WinGet — detect installed, install missing,
skip present, continue past failures.

Prereq: Milestone 4 accepted. Read [../PROFILES.md](../PROFILES.md),
[../WINDOWS_APIS.md](../WINDOWS_APIS.md) (applications/), [../DECISIONS.md](../DECISIONS.md)
ADR-0002.

---

## Task 1 — Profile loading

- [ ] Resolve ADR-0002 first: decide JSON-only vs add a YAML parser. Update the ADR
      (`accepted`, or superseded). If YAML: it must be a single-header / tiny vendored
      parser with an ADR — otherwise ship `.json` and keep `.yaml` as references.
- [ ] `src/core/profiles/Profile.h`: `struct AppEntry { std::string id; bool required; };`
      `struct Profile { std::string name; std::string description;
      std::vector<AppEntry> applications; };`
- [ ] `src/core/profiles/ProfileLoader.h/.cpp`: `load(path)` → `Profile` or an error.
      Validate: `name` matches file stem, `description` present, each `id` non-empty and
      unique, `required` defaults false, unknown keys → error. No OS calls — pure parsing.

## Task 2 — ApplicationProvider + WinGetProvider

- [ ] `src/core/applications/ApplicationProvider.h`: abstract —
      `bool isInstalled(const std::string& id)`,
      `struct InstallResult { bool ok; int exitCode; std::string log; bool alreadyInstalled; }`
      `InstallResult install(const std::string& id, const InstallOptions&)`,
      `std::string name() const`.
- [ ] `src/core/applications/WinGetProvider.h/.cpp`:
      - `isInstalled`: `winget list --id <id> --exact` — parse exit code + output
        (careful: winget exit codes vary; also treat "No installed package found" text).
      - `install`: `winget install --id <id> --exact --silent
        --accept-package-agreements --accept-source-agreements --disable-interactivity`
        via `QProcess`, capture output + exit code, timeout (default 600s).
      - retry once on a transient failure (source/network exit codes).
      - if `winget` unavailable (Win7/8) → provider reports unavailable; caller records all
        apps as `Skipped (winget unavailable)`.

## Task 3 — CLI

- [ ] `provisioner.exe apps install --profile <name> [--dry-run] [--profiles-dir <path>]`:
      load profile, for each app `isInstalled?` → skip / `install`. `--dry-run` prints the
      plan only. Per-app status; summary: installed / already installed / failed (with the
      `required` flag). One failure never stops the rest.
      Exit 0 / 1 (a `required` app failed → warnings) / 2 (profile invalid or winget missing
      when install requested).

## Task 4 — Tests

- [ ] Unit: `ProfileLoader` — valid file, missing `description`, duplicate id, unknown key,
      `required` default, name/stem mismatch. Use `tests/fixtures/profiles/`.
- [ ] Unit: winget output parsing — `isInstalled` true/false from canned `winget list` text;
      install exit-code → `InstallResult` mapping; transient-failure retry decision.
- [ ] Integration (gated, VM/safe machine): `isInstalled` for a known-present and
      known-absent package; optionally install a tiny package + detect + uninstall.
- [ ] `scripts\test.bat` green.

---

## Exit criteria

- [ ] `apps install --profile standard --dry-run` on this machine lists correct
      install/skip decisions.
- [ ] Profile validation rejects the bad fixtures with clear messages.
- [ ] A failing package (bad id in a test profile) does not stop the others.
- [ ] winget-unavailable path handled gracefully.
- [ ] Unit tests pass; integration run on a VM (paste result).
- [ ] Change summary + ADR-0002 resolution. STOP for review.

# Milestone 8 — Checklist profiles: local installers + config tweaks

**Goal:** a profile is a **three-section checklist** (drivers / applications / config), each
item individually toggleable — the DriverPack-style workflow. App installers can come from
winget **or** local files on the USB drive. A catalog of tested Windows config tweaks.

Prereq: Milestone 6 accepted. Read [../PROFILES.md](../PROFILES.md) (the new format is
documented there).

---

## Status — DONE (2026-09-03)

### Profile format (`src/core/profiles/`)
- [x] `Profile` gains `DriverSection` (`enabled`, `providerOrder`, `installUnsigned`,
      `exclude`), `applications[]` now `AppEntry{ id, source, wingetId, enabled, required }`,
      and `config[]` `ConfigEntry{ id, enabled, args }`.
- [x] `ProfileLoader` validates: unknown top-level/section keys, bad `source`, unknown
      config tweak id, duplicate app/config ids, name/stem mismatch. Backward compatible —
      an old `applications: [{id, required}]` array still loads (`source` defaults to
      winget, `wingetId` falls back to `id`).
- [x] `Profile::enabledApps()` / `enabledConfig()` helpers.

### Local installers (`src/core/applications/LocalInstallerProvider`)
- [x] `apps/<id>/app.json` manifest: `installer` (.exe/.msi only), `silentArgs`,
      `detect` (`registry` / `file` / `arp`), `expectedExitCodes`.
- [x] `isInstalled()` runs the detect rule; `install()` runs the declared installer
      (EXE + silentArgs, or `msiexec /i … /qn`), 15-min timeout, exit-code check,
      reboot detection (3010/1641).
- [x] `available()` lists manifested apps (for the GUI).
- [x] `apps/` resolved relative to the exe (portable), with repo-relative fallbacks.

### Config tweak catalog (`src/core/config/`)
- [x] `ConfigTweak` interface (info / check / apply). `catalog()`, `isKnownTweak()`,
      `runTweak(id, args, elevated)` (handles unknown id, missing required args,
      not-elevated → Skipped, already-applied → AlreadyApplied).
- [x] 12 tweaks implemented: `disable-password-expiry`, `clean-taskbar-pins`,
      `clean-startup-items`, `disable-startup-item`, `show-file-extensions`,
      `show-hidden-files`, `disable-fast-startup`, `set-power-high-performance`,
      `disable-hibernate`, `set-timezone`, `enable-rdp`, `set-computer-name`.
      None touch Defender / signature enforcement. Elevation-gated where needed.

### Engine + CLI
- [x] `ProvisioningEngine` loads the profile once; the driver stage honours
      `providerOrder` / `exclude` / `installUnsigned`; a **Config stage** runs the enabled
      tweaks; the app stage picks winget vs `LocalInstallerProvider` per entry.
- [x] `ProvisioningState` + `ReportBuilder` gain a `config` section
      (applied / already / failed / skipped); config failure/skip → SUCCESS WITH WARNINGS.
- [x] `provisioner config list [--json]` — the catalog.
- [x] `provision` / `apps install` handle the new format; `--apps-dir` added.

### Tests (22 suites, all offline)
- [x] `test_configtweak` — catalog uniqueness, unknown id, missing-arg, elevation gate.
- [x] `test_localinstaller` — manifest parse (valid / non-exe / missing file / missing
      manifest), `available()`, `isInstalled` on a missing app.
- [x] `test_profileloader` — 3-section parse, `enabledApps`/`enabledConfig`, bad tweak id,
      bad source, backward-compat old format.

## Exit criteria — met

- [x] `provision --profile standard --dry-run` runs all three sections on this machine:
      drivers (chain), apps (winget + local), config (would-apply). Verified.
- [x] `config list` prints the catalog.
- [x] Shipped profiles converted to the 3-section format and still validate.
- [x] Backward compatibility: an old-style profile still loads.
- [x] 22 test suites green; clean `-Werror` build (GUI on & off).

### Deviations / notes
- Config tweak `check()` is best-effort — some tweaks return `Unknown` and always run
  `apply()` (which itself no-ops / reports AlreadyApplied where it can tell).
- The **GUI 3-tab checklist picker** is Milestone 9 — this milestone is the backend it
  needs. Right now selection is by editing `enabled` in the profile JSON.
- Real config-tweak application needs elevation and isn't run on the dev machine (dry-run
  only). The tweak *logic* (catalog, gating, arg validation) is unit-tested.

# GUI manual QA checklist

Build: `scripts\build-release.ps1` (or `cmake -S . -B build -DSHIFTECH_BUILD_GUI=ON`).
Run: `build\app.exe` (or `scripts\run-app.ps1 -Gui`).

The GUI is a **picker, not an editor**. It never changes profile settings — a technician
picks a profile and ticks which items to run on this PC. To add profiles or change their
settings (provider order, config args, `required` flags), edit the `profiles\*.json` files.

## Layout

- **System panel** — Windows edition/build, arch, elevation, winget/pnputil.
- **Profile dropdown** — `(custom — start from blank)` + every `profiles/*.json`.
  Picking one re-seeds all three tabs. A hint line points power users at the profile files.
- **Three tabs, all read-only except the tick-boxes:**
  - **Drivers** — "Run the driver stage" toggle, a **read-only** "Provider order" line
    (from the profile), "Install unsigned" toggle, and a table of *devices needing a
    driver* — untick a row to add its instance id to `exclude` for this run.
  - **Applications** — `Install? | Application | Source`. Source = `winget` (Windows
    Package Manager, needs internet) or `local drive` (`apps\<id>\` on the medium).
    `required` apps are carried from the profile (shown in the row tooltip), not edited here.
  - **Config** — `Apply? | Tweak`. Only the tweaks the *profile lists* are shown. Hover a
    row for what it changes / whether it needs admin / what value it needs.
- **Dry run** checkbox (default on), progress bars (aligned grid), Start / Cancel,
  colour-coded log, report pane, **Save report…**.

## Smoke — dry run

1. Launch. Pick **standard** from the dropdown.
   - **Expect:** Drivers tab shows the provider-order line + the device list; Applications
     tab shows chrome/7zip/winrar/adobe-reader ticked (+ vlc/sumatra unticked); Config tab
     shows the profile's tweaks with disable-password-expiry / clean-taskbar-pins /
     show-file-extensions ticked, clean-startup-items unticked.
2. Untick a few items.
3. Keep **Dry run** checked, click **Start**.
   - **Expect:** the log streams `[HH:MM:SS] category  message`, progress bars advance,
     "Current task" tracks the latest event.
   - The run acts on exactly the ticked set — check the log lines
     (`would install via winget` / `would install via local installer` / `would apply`).
4. On finish: report pane shows the full report (incl. a **Config** section on a real run)
   and status; **Save report…** enables.

## Cancel

1. Pick **standard** (its app-detection loop gives a visible window), Start.
2. **Cancel** mid-run → "Cancelling…", the run stops before the next item, a
   `cancelled: during …` warning appears, a partial report shows. `logs/<run>/` is still valid.

## What this proves

- The GUI is a pure front-end: `ChecklistTabs` builds an in-memory `Profile` from the
  ticked state (never editing the profile's own settings), `MainWindow` hands it to
  `ProvisioningEngine` via `ProvisioningOptions::profileObject` and renders the signals.
- `src/core` additions for the whole GUI: the cancel token (M7) + the optional
  `profileObject` on `ProvisioningOptions` (M9) — both thin. (`Profile::toJson()` also
  exists as a library helper but the GUI no longer calls it.)
- `test_mainwindow_smoke` covers tab construction and profile-seeding headlessly.

## Not covered here

- Real (non-dry-run) provisioning needs elevation + a VM.
- Windows 7/8: GUI is not shipped there (ADR-0003) — use the CLI.

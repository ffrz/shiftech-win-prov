# GUI manual QA checklist

Build: `scripts\configure.bat` then reconfigure with `-DSHIFTECH_BUILD_GUI=ON`, or
`cmake -S . -B build -DSHIFTECH_BUILD_GUI=ON && cmake --build build`.
Run: `build\shiftech_gui.exe`.

## Smoke — dry run

1. Launch `shiftech_gui.exe`. **Expect:** window titled "Shiftech Win Provisioner".
2. **System panel** shows the correct Windows edition/build, architecture, and
   `Elevated: no` (unless launched elevated), `winget`/`pnputil` yes/no.
3. **Profile** dropdown is populated from `profiles/` (standard/office/technician/developer).
4. **Dry run** is checked by default.
5. Click **Start**.
   - **Expect:** Start disables, Cancel enables, the log fills with
     `[HH:MM:SS] category  message` lines coloured by severity.
   - "Current task" updates to the latest event / stage.
   - Drivers and Applications progress bars advance 0→100.
6. When the run finishes:
   - **Expect:** "Done — SUCCESS" / "SUCCESS WITH WARNINGS" in Current task.
   - The report pane shows the full `Provisioning Complete … Status: …` text.
   - **Save report…** enables. Click it, choose a path → a `report.json` is written.
7. Start is re-enabled; you can run again.

## Cancel

1. Start a run with **Dry run** unchecked is NOT recommended on your own machine — keep
   Dry run checked. To exercise cancel meaningfully, pick the `standard` profile (the app
   detection loop calls `winget list` per app, giving a visible window to cancel in).
2. Click **Cancel** mid-run.
   - **Expect:** "Cancelling…", then the run stops before the next item, a
     `cancelled: during …` warning appears in the log, and a partial report is shown.
   - The log dir still contains a valid `run.json` / `state.json`.

## Skip modes

- **Skip drivers** checked → the run jumps straight from Hardware Scan to the app stages;
  driver counts are all zero.
- **Skip applications** checked (or no profile) → app stages are walked but no app work is
  done.

## What this proves

- The GUI is a pure front-end: it builds `ProvisioningOptions`, subscribes an event sink,
  and renders signals. The only `src/core` change for the GUI was the cooperative
  `cancelRequested` flag on `ProvisioningOptions` (ADR sign-off in MILESTONE-7.md).
- `test_mainwindow_smoke` covers the signal→widget wiring headlessly in CI.

## Not covered here

- Real (non-dry-run) provisioning needs elevation and a VM — see MILESTONE-4/6 procedures.
- Windows 7/8: the GUI is **not** shipped there (ADR-0003) — use the CLI.

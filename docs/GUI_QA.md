# GUI manual QA checklist

Build: `scripts\build-release.ps1` (or `cmake -S . -B build -DSHIFTECH_BUILD_GUI=ON`).
Run: `build\shiftech_gui.exe` (or `scripts\run-app.ps1 -Gui`).

## Layout

- **System panel** — Windows edition/build, arch, elevation, winget/pnputil.
- **Profile dropdown** — `(custom — start from blank)` + every `profiles/*.json`.
  Picking one re-seeds all three tabs.
- **Three tabs:**
  - **Drivers** — "Run the driver stage" toggle, editable "Provider order",
    "Install unsigned" toggle, and a table of *devices needing a driver* — untick a row
    to add it to the profile's `exclude`.
  - **Applications** — table of apps (profile's + any `apps/<id>/` on the medium):
    checkbox, name, source (winget / local drive), Required checkbox.
  - **Config** — every tweak from the catalog: checkbox, title, `admin` badge, and an
    inline args field for tweaks that need one (timezone id, computer name, …).
- **Dry run** checkbox (default on), **Save as profile…**, progress bars, Start/Cancel,
  colour-coded log, report pane, **Save report…**.

## Smoke — dry run

1. Launch. Pick **standard** from the dropdown.
   - **Expect:** Drivers tab shows the WU chain + the device list; Applications tab shows
     chrome/7zip/winrar/adobe-reader ticked (+ vlc/sumatra unticked); Config tab shows
     disable-password-expiry / clean-taskbar-pins / show-file-extensions ticked.
2. Untick a few items, tick a couple of extras.
3. Keep **Dry run** checked, click **Start**.
   - **Expect:** the log streams `[HH:MM:SS] category  message`, progress bars advance,
     "Current task" tracks the latest event.
   - The run installs/applies exactly the ticked set — check the log lines
     (`would install via winget` / `would install via local installer` / `would apply`).
4. On finish: report pane shows the full report incl. a **Config** section (real run) and
   status; **Save report…** enables.

## Save as profile

1. Set up a selection across the three tabs.
2. **Save as profile…** → choose `profiles\myprofile.json`.
   - **Expect:** a valid 3-section JSON whose `name` matches the file stem. Re-open it from
     the dropdown → the same items are ticked (round-trips through `ProfileLoader`).

## Cancel

1. Pick **standard** (its app detection loop gives a visible window), Start.
2. **Cancel** mid-run → "Cancelling…", the run stops before the next item, a
   `cancelled: during …` warning appears, a partial report shows. `logs/<run>/` is still valid.

## What this proves

- The GUI is a pure front-end: `ChecklistTabs` builds an in-memory `Profile` from the
  ticked state, `MainWindow` hands it to `ProvisioningEngine` via
  `ProvisioningOptions::profileObject` and renders the event signals.
- `src/core` additions for the whole GUI: the cancel token (M7) + the optional
  `profileObject` on `ProvisioningOptions` and `Profile::toJson()` (M9) — all thin.
- `test_mainwindow_smoke` covers tab construction and profile-seeding headlessly.

## Not covered here

- Real (non-dry-run) provisioning needs elevation + a VM.
- Windows 7/8: GUI is not shipped there (ADR-0003) — use the CLI.

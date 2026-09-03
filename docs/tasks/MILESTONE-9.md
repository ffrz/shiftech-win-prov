# Milestone 9 — GUI 3-tab checklist picker

**Goal:** the GUI presents a profile as three tabs — **Drivers / Applications / Config** —
each item a checkbox. Load a profile as the default, let the technician tick/untick, then
Start. The DriverPack experience.

Prereq: Milestone 8 accepted. The backend is all there:
- `Profile` with `drivers` / `applications[] (enabled)` / `config[] (enabled)`
- `LocalInstallerProvider::available()` — local apps on the medium
- `config::catalog()` — the tweak list
- `ProvisioningEngine` already runs whatever a `Profile` says

---

## Task 1 — Editable in-memory profile

- [ ] `src/gui/ProfileModel.h/.cpp` (or plain structs): holds the currently-loaded
      `Profile` plus, for the app/config tabs, the **union** of what the profile lists and
      what is available on the medium / in the catalog, each with a checked state.
      - Applications tab rows = profile apps ∪ `LocalInstallerProvider::available()` ∪
        (optionally) a curated winget shortlist. Checked = in the profile with
        `enabled: true`.
      - Config tab rows = every `config::catalog()` entry. Checked = in the profile with
        `enabled: true`. Rows needing args show an inline field (timezone, computer name,
        startup-item match).
      - Drivers tab = provider order (editable), `installUnsigned` toggle, and a live
        device list (from `DeviceEnumerator`) with a per-device "resolve driver for this"
        checkbox feeding `drivers.exclude` (unchecked → excluded).
- [ ] "Build `ProvisioningOptions` + an effective `Profile`" from the ticked state, written
      to a temp profile file the engine loads (or pass the `Profile` straight to a new
      `ProvisioningEngine::run(const Profile&, options)` overload — small, allowed core
      addition if it stays a thin wrapper).

## Task 2 — MainWindow layout

- [ ] Replace the single "Run" group with a `QTabWidget`:
      - **Drivers** tab (Task 1)
      - **Applications** tab — a `QTableView` / list of checkboxes: name, source
        (winget/local), "installed?" (lazy check), required flag.
      - **Config** tab — checkboxes from the catalog, `[admin]` badge, inline arg fields.
- [ ] Keep the System panel, progress bars, Start/Cancel/Save-report, log + report panes.
- [ ] Profile dropdown still there; picking one re-seeds all three tabs.
- [ ] "Save as profile…" — write the current ticked state back to a `profiles/<name>.json`.

## Task 3 — Wiring

- [ ] Start → build the effective profile from the tabs → `EngineController::start`.
- [ ] The Applications tab's "installed?" column: query providers on a worker thread,
      never block the UI.
- [ ] Config rows with missing required args disable their checkbox until filled.

## Task 4 — Tests / QA

- [ ] Extend `test_mainwindow_smoke`: construct the window, assert the three tabs exist and
      that seeding a fixture profile checks the right rows.
- [ ] Update [../GUI_QA.md](../GUI_QA.md) with the tab workflow.

## Exit criteria

- [ ] Launching the GUI, picking `standard`, shows Drivers/Apps/Config tabs with the
      profile's selections pre-checked.
- [ ] Un/re-checking items and clicking Start runs exactly the selected set (verify via the
      dry-run log).
- [ ] "Save as profile…" round-trips through `ProfileLoader`.
- [ ] `src/core` diff is empty or a single signed-off thin `run(Profile, options)` overload.
- [ ] Smoke test + manual QA green. Change summary. STOP for review.

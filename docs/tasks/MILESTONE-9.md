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

## Exit criteria — DONE (2026-09-04)

- [x] GUI has a `QTabWidget` with **Drivers / Applications / Config** tabs
      (`src/gui/ChecklistTabs`). Picking a profile from the dropdown re-seeds all three.
- [x] **Drivers tab:** "Run driver stage" toggle, editable provider-order, "Install
      unsigned" toggle, live device table (from `DeviceEnumerator`) — untick a device →
      it goes to `drivers.exclude`.
- [x] **Applications tab:** table = profile apps ∪ `LocalInstallerProvider::available()`,
      checkbox + name + source (winget/local) + Required checkbox.
- [x] **Config tab:** every `config::catalog()` tweak, checkbox + title + `admin` badge +
      inline args field for tweaks that need one.
- [x] Start builds an effective `Profile` from the ticked state and runs it via
      `ProvisioningOptions::profileObject` (verified: dry-run log shows exactly the ticked
      apps/tweaks).
- [x] **Save as profile…** writes `Profile::toJson()` to `profiles/<name>.json`
      (name forced to the file stem); round-trips through `ProfileLoader`.
- [x] `src/core` additions: `ProvisioningOptions::profileObject` (optional in-memory
      profile) + `Profile::toJson()` — both thin, no pipeline logic moved out of core.
- [x] `test_mainwindow_smoke` extended: 3 tabs present, `ChecklistTabs::seed` →
      `effectiveProfile()` round-trip checks the right rows. 22 suites green.
- [x] GUI launched and screenshotted; manual QA script in [../GUI_QA.md](../GUI_QA.md).

### Deviations / notes
- "Pause" is still folded into Cancel (from M7).
- The Applications tab does not (yet) show a live "installed?" column — deferred; it would
  need per-row provider queries on a worker thread. The dry-run log already shows
  `already installed` vs `would install`.
- No curated winget shortlist in the Apps tab — you get the profile's apps + local apps;
  add a winget app by editing the profile JSON (or a future "+ add winget id" button).

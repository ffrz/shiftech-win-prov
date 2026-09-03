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

The GUI is a **picker, not an editor** (owner feedback during the milestone). It never
changes a profile's settings — a technician picks a profile and ticks which items to run.

- [x] GUI has a `QTabWidget` with **Drivers / Applications / Config** tabs
      (`src/gui/ChecklistTabs`). Picking a profile from the dropdown re-seeds all three.
      A hint line points power users at the profile files for anything they can't do here.
- [x] All three tables are **strictly read-only** (`NoEditTriggers` + non-editable item
      flags); only the include-checkboxes and the two driver toggles are interactive.
- [x] **Drivers tab:** "Run driver stage" toggle, **read-only** provider-order line (set
      it in the profile file), "Install unsigned" toggle, live device table — untick a
      device → its instance id goes to `drivers.exclude` for this run.
- [x] **Applications tab:** `Install? | Application | Source` (winget / local drive).
      `required` is carried from the profile (row tooltip), not editable in the GUI.
      Rows = profile apps ∪ `LocalInstallerProvider::available()`.
- [x] **Config tab:** `Apply? | Tweak` — only the tweaks the profile lists. Admin +
      arg-needed info is in the row tooltip. Args are set in the profile file, never here.
- [x] Start builds an effective `Profile` (profile settings unchanged, only `enabled`
      flags + `exclude` reflect the ticks) and runs it via
      `ProvisioningOptions::profileObject`. Verified: dry-run log shows exactly the ticked set.
- [x] No "Save as profile" — new profiles are made by editing the `.json` files.
- [x] Progress area uses `QGridLayout`: right-aligned labels, uniform-width bars.
- [x] `src/core` addition for the GUI: `ProvisioningOptions::profileObject` (optional
      in-memory profile) — one thin field. (`Profile::toJson()` exists as a library helper
      but the GUI no longer calls it.)
- [x] `test_mainwindow_smoke`: 3 tabs present; `ChecklistTabs::seed` → `effectiveProfile()`
      preserves the profile's config entries + `required`, only toggling `enabled`.
      22 suites green.
- [x] Manual QA script in [../GUI_QA.md](../GUI_QA.md).

### Deviations / notes
- "Pause" is still folded into Cancel (from M7).
- No live "installed?" column in the Apps tab — the dry-run log already shows
  `already installed` vs `would install`.
- The Config tab only lists the profile's tweaks (not the whole catalog) — `config list`
  on the CLI shows the full catalog; a profile author picks from it.
- No curated winget shortlist in the Apps tab — you get the profile's apps + local apps;
  add a winget app by editing the profile JSON (or a future "+ add winget id" button).

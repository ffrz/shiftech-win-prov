# Milestone 7 — Qt GUI

**Goal:** a simple Qt Widgets dashboard that is a **pure front-end** over the engine's
event stream. Adding it must require **no change** to `src/core/`.

Prereq: Milestone 6 accepted. Read [../ARCHITECTURE.md](../ARCHITECTURE.md) (GUI target),
the "GUI" section of `../../initial-prompt.md`, and [../BUILD.md](../BUILD.md) ADR-0003
(Windows 7 / Qt 6 risk).

---

## Task 1 — GUI target

- [ ] `src/gui/CMakeLists.txt`: built only when `SHIFTECH_BUILD_GUI=ON`.
      `qt_add_executable(shiftech_gui WIN32 ...)`, link `shiftech_core Qt6::Core Qt6::Widgets`.
- [ ] `main.cpp`: `QApplication`, show `MainWindow`. App manifest requesting
      `requireAdministrator` **only** if a provisioning run is started (or prompt/relaunch
      elevated on demand — do not force elevation just to view `scan`).

## Task 2 — Engine ↔ GUI bridge

- [ ] `src/gui/EngineController.h/.cpp` (`QObject`): owns a `ProvisioningEngine`, runs
      `run()` on a `QThread` / `QtConcurrent`, converts the engine's `std::function` event
      sink into Qt signals: `systemInfoReady(SystemInfo)`, `stageChanged(Stage)`,
      `progress(QString stage, int pct)`, `event(ProvisioningEvent)`, `finished(Report)`.
- [ ] Pause/Cancel: cooperative cancel token checked between items in the engine (add the
      token to `ProvisioningOptions` in core — this is the ONE allowed small core addition;
      if it turns out larger, stop and get sign-off). Pause = stop starting new items.

## Task 3 — Dashboard UI

- [ ] `src/gui/MainWindow` per the spec sketch:
      - System panel: Windows edition/build, CPU, RAM, arch, elevation.
      - Drivers progress bar + count; Applications progress bar + count.
      - "Current task" label (latest event message).
      - `[ Start ]` / `[ Pause ]` / `[ Cancel ]` buttons (Start disabled without a profile).
      - Profile picker (reads `profiles/`).
      - `[ Dry run ]` checkbox.
- [ ] `src/gui/LogView`: scrolling list of `ProvisioningEvent`s, color by severity, filter
      by category.
- [ ] `src/gui/ReportView`: the final `Report` rendered; `Save report…` (JSON) button.
- [ ] No business logic in any widget — widgets only render signals and send
      start/pause/cancel.

## Task 4 — Windows 7 / Qt 6 decision (ADR-0003)

- [ ] Test the GUI build on a Windows 7 x64 VM. Record the result in ADR-0003 and pick:
      Qt 5.15 GUI build for legacy / static Qt 6 / CLI-only on Win7. Update
      [../DECISIONS.md](../DECISIONS.md).

## Task 5 — Tests / manual QA

- [ ] A GUI smoke test (QtTest + `QTest::qWait`) that constructs `MainWindow`, feeds it
      synthetic engine signals, and asserts the progress bars / labels update.
- [ ] Manual QA script in `docs/`: run a `--dry-run` provision from the GUI, verify live
      progress, log view, report, save report.
- [ ] `scripts\test.bat` green; GUI built with `-DSHIFTECH_BUILD_GUI=ON`.

---

## Exit criteria — DONE (2026-09-03)

- [x] `shiftech_gui.exe` builds (`-DSHIFTECH_BUILD_GUI=ON`), launches, and renders the
      dashboard: System panel (edition/build/arch/elevation/winget/pnputil), profile
      picker from `profiles/`, Dry run + Skip toggles, Drivers/Applications progress bars,
      Current task label, Start/Cancel/Save-report buttons, colour-coded log pane, report
      pane. Verified with a screenshot.
- [x] `EngineController` runs `ProvisioningEngine` on a `QThread` and re-emits the
      `std::function` event sink as Qt signals; `MainWindow` only renders them and sends
      start/cancel. No pipeline logic in any widget.
- [x] **`src/core` diff for M7 = the cancel token only** (24 lines:
      `ProvisioningOptions::cancelRequested` + `cancelled()`/`finishEarly()` +
      3 check-points). This was the one sanctioned core change.
- [x] Pause/Cancel: cooperative — `EngineController` flips an `std::atomic<bool>`; the
      engine checks it between items, finalizes a partial report, and returns. (No separate
      "pause" state in V1 — Cancel covers the need; a pause toggle can be added later
      without core changes.)
- [x] `Save report…` writes the run's `report` object as JSON.
- [x] ADR-0003 resolved: **CLI-only on Windows 7/8, Qt 6 GUI for Win10/11.** (Option c —
      no Win7 VM was available to test a static/Qt5 GUI; the CLI already covers 100% of
      functionality and is the portable-USB workflow.)
- [x] GUI smoke test `test_mainwindow_smoke` passes (19 suites total); manual QA script in
      [../GUI_QA.md](../GUI_QA.md).

### Deviations / notes
- GUI widgets are built into a `shiftech_gui_lib` static lib so the smoke test can link
  `MainWindow` without the `main()`.
- No app manifest / self-elevation added — the GUI runs unelevated for viewing/dry-run;
  a real run needs the user to launch it elevated (documented). Silent self-elevation was
  explicitly out of scope for V1 (AGENTS.md §3.7).
- "Pause" from the spec sketch is folded into Cancel for V1.

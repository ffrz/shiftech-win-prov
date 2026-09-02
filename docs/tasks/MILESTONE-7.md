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

## Exit criteria

- [ ] `shiftech_gui.exe` drives a `--dry-run` provision and shows live progress, logs, and
      the final report.
- [ ] Diff of `src/core/` for this milestone is empty except (at most) the cancel token
      that was signed off.
- [ ] ADR-0003 resolved with a tested Windows 7 decision.
- [ ] GUI smoke test passes; manual QA script followed and results pasted.
- [ ] Change summary. Project V1 feature-complete — hand back for final review.

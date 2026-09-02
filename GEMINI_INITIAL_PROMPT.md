# Initial prompt for Gemini 3.1 Pro

> Paste everything in the fenced block below as the first message to the agent, from the
> repository root `d:\shiftech\software-projects\shiftech-win-prov`. It is deliberately
> scoped to **Milestone 1 only**. After M1 is reviewed, feed it
> `docs/tasks/MILESTONE-2.md`, and so on.

---

```
You are a senior Windows systems engineer and modern C++/Qt developer. You are working in
an existing repository at the current working directory. The repository is a prepared
scaffold — architecture, docs, agent rules, and toolchain wiring already exist. There is
NO application code yet. Your job is to implement it, milestone by milestone.

## Step 0 — Orient yourself (do this before writing any code)

Read, in this order:
1. AGENTS.md              — the authoritative working contract. Follow every hard rule.
2. README.md              — what the project is.
3. docs/ARCHITECTURE.md   — module boundaries, Device model, event/state model, pipeline.
4. docs/BUILD.md          — the exact toolchain on this machine and how to build/test.
5. docs/WINDOWS_APIS.md   — which Windows APIs to use for hardware detection.
6. docs/ROADMAP.md        — all milestones and their exit criteria.
7. docs/TESTING.md        — test framework and safety rules.
8. docs/tasks/MILESTONE-1.md — your task list for THIS milestone. This is your checklist.

Then briefly confirm back to me:
- the build command you will use,
- the Qt version and compiler you will use,
- the 3–4 Windows APIs you will call for device enumeration,
- anything in the architecture proposal you would change before starting (or "none").

Do not start coding until you have posted that confirmation.

## Scope of this session

Implement **Milestone 1 only** (see docs/ROADMAP.md and docs/tasks/MILESTONE-1.md):
- root CMakeLists.txt + `shiftech_core` (static lib), `provisioner` (console exe), and
  `shiftech_tests` (QtTest) targets — all building;
- `SystemInspector` (basic: OS version/build, edition, architecture, elevation,
  winget present?, pnputil present?);
- `Device` model + `DeviceEnumerator` using SetupAPI/CfgMgr32 (NOT pnputil text parsing);
- `provisioner.exe scan` — prints a system header, then all devices, with devices that
  need a driver grouped and clearly flagged at the top; supports `--json`;
- unit tests for Hardware ID parsing and device classification (must pass offline).

Do NOT implement anything from Milestone 2+ (no DriverProvider, no downloader, no winget
install, no GUI). Do NOT write a real DriverPack provider — it is gated (docs/DECISIONS.md
ADR-0004).

## Hard rules (full list in AGENTS.md §3 — these are the ones most relevant now)

- `src/core/` is UI-free: Qt Core is allowed (QString/QProcess/QFile/QJson), Qt Widgets/GUI
  is NOT. The CLI is a thin front-end over the core library.
- Prefer Windows APIs over parsing command output for enumeration.
- Public core API uses plain types (std::string, std::vector, POD structs/enums) as shown
  in docs/ARCHITECTURE.md; Qt types are fine internally and at IO boundaries.
- Every external operation (QProcess, file IO) has a timeout and error handling.
- `scan` must run WITHOUT elevation (note reduced fields if any); it must detect and
  report whether it is elevated. Do not self-elevate.
- One class per file, `#pragma once`, namespace `shiftech::core::<module>`.
- Build with MinGW via `scripts\build.bat` — there is no MSVC on this machine.

## Working method I want you to follow

1. Post the Step 0 confirmation. Wait for my "go".
2. Work through docs/tasks/MILESTONE-1.md task by task. After each task, state what you did
   and the build/test result.
3. Keep commits small and scoped: `core(hardware): add Device model`, etc. No
   Co-Authored-By trailer (this repo uses none).
4. When every exit criterion in docs/tasks/MILESTONE-1.md is met:
   - run `scripts\build.bat` clean,
   - run `scripts\run.bat scan` on this machine and paste the real output,
   - run `scripts\test.bat` and paste the result,
   - give me a change summary (files added, key decisions, any deviations from the docs,
     anything you had to stub).
5. Then STOP and wait for my review. Do not start Milestone 2.

## If you get blocked

- If a doc is ambiguous or contradicts what you find on the machine, stop and ask — don't
  guess and don't silently deviate.
- If you think the architecture proposal is wrong, say so in Step 0 with a concrete
  alternative; don't just implement something different.
- If a Windows API you expected isn't available on the build, note it and propose the
  fallback from docs/WINDOWS_APIS.md.

Begin with Step 0.
```

# AGENTS.md — working instructions for AI agents

This file is the contract for any agent (Claude Code, Codex, etc.) working in this
repository. Read it fully before writing code. `CLAUDE.md` defers to this file.

---

## 1. What this project is

A standalone native Windows provisioning tool for technicians. See [README.md](README.md)
and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). Language: **C++20**. UI: **Qt 6**.
Build: **CMake + Ninja**. Target OS: Windows 7 x86, Windows 7/8/10/11 x64. **No ARM.**

## 2. Environment (already installed on this machine)

| Tool | Path |
|------|------|
| Qt 6.6.2 (MinGW x64) | `D:\bin\Qt\6.6.2\mingw_64` |
| Qt 6.11.1 (MinGW x64) | `D:\bin\Qt\6.11.1\mingw_64` |
| MinGW GCC 11.2.0 | `D:\bin\Qt\Tools\mingw1120_64` |
| MinGW GCC 13.1.0 | `D:\bin\Qt\Tools\mingw1310_64` |
| CMake 3.30.5 | `D:\bin\Qt\Tools\CMake_64\bin\cmake.exe` |
| Ninja | `D:\bin\Qt\Tools\Ninja\ninja.exe` |
| Git | `C:\Program Files\Git` |
| winget | on PATH (`WindowsApps\winget.exe`) |

**Default kit: Qt 6.6.2 + MinGW 11.2.0.** Rationale in [docs/BUILD.md](docs/BUILD.md).
No MSVC / Visual Studio is installed — do not assume `cl.exe`. Do not `apt`/`brew`.
See [docs/BUILD.md](docs/BUILD.md) for the exact configure/build commands and the
`scripts/` wrappers.

## 3. Hard rules

1. **Inspect before implementing.** Read the relevant `docs/` file for the module you touch.
2. **Core stays UI-free.** Nothing in `src/core/` may `#include <QtWidgets>` or depend on a GUI. Qt Core (QString, QProcess, QFile, signals) is allowed; Qt Widgets/GUI is not.
3. **No GUI-click automation.** No AutoIt, no SendKeys, no screen scraping of installers.
4. **Prefer Windows APIs over parsing command output** when the API is more reliable
   (device enumeration → SetupAPI/CfgMgr32, not `pnputil /enum-devices` text parsing).
   `pnputil` *is* the right tool for driver *install* — use it there.
5. **One failure never aborts the pipeline.** Catch, log, record a per-item status, continue.
6. **Every external operation has a timeout and error handling.** `QProcess`, network, file IO.
7. **Elevation:** operations needing admin must detect the lack of it and fail with a clear
   message. Do not silently continue. Do not self-elevate silently in V1.
8. **Security:** never disable Windows Defender, driver signature enforcement, or do
   registry hacks to bypass security. Unverifiable driver package → warning + skip.
9. **No auto-reboot in V1.** Set a `rebootRequired` flag; design state so resume is possible later.
10. **Do not assume DriverPack's website/API structure.** Complete the investigation in
    [docs/DRIVER_PROVIDER.md](docs/DRIVER_PROVIDER.md) first. Until then, only the
    **mock provider** may be implemented, with integration points documented.
11. **Minimal dependencies.** Adding any third-party library requires an ADR in
    [docs/DECISIONS.md](docs/DECISIONS.md) and explicit sign-off.
12. **No secrets in logs.** No credentials, tokens, or PII written to `logs/`.
13. **Build and test after every milestone** before moving on. Report a summary of changes
    and test results.
14. **Follow the roadmap.** Do not jump ahead of the current milestone in
    [docs/ROADMAP.md](docs/ROADMAP.md).

## 4. Coding conventions

- C++20. Prefer standard library; use Qt Core types at module boundaries where they
  simplify IO/process/string handling.
- Public core API uses plain types (`std::string`, `std::vector`, POD structs / enums)
  as shown in the initial spec, so non-Qt consumers stay possible. Internal
  implementation may use Qt freely.
- One class per file, `PascalCase.h` / `PascalCase.cpp`, header guards via `#pragma once`.
- Namespaces: `shiftech::core::<module>` (e.g. `shiftech::core::hardware`).
- Errors: return a result type / status enum for expected failures; exceptions only for
  truly exceptional cases. Never let an exception escape the pipeline loop.
- Format: 4-space indent, no tabs, ~100 col soft limit. A `.clang-format` is provided.
- Every module exposes a testable interface; put pure logic (ID parsing, matching,
  classification, profile parsing, state transitions) behind functions that don't touch
  the OS so they can be unit-tested on any platform.

## 5. Testing

- Framework: **QtTest** (bundled with Qt, no extra dependency).
- Unit tests must not touch real devices, real network, or install anything.
- Integration tests that call `pnputil` / `winget` are gated behind a CMake option
  (`SHIFTECH_INTEGRATION_TESTS=OFF` by default) and must be run only on a VM / test machine.
- See [docs/TESTING.md](docs/TESTING.md).

## 6. Git

- This repo is initialized. Work on a feature branch, not on the default branch.
- Conventional-ish commit subjects: `core(hardware): add DeviceEnumerator`.
- Do not commit co authored messages, use commit skill.
- Do not commit `build/`, `cache/` contents, or `logs/` contents (`.gitignore` covers these).

## 7. Documentation map

| Doc | Read it before you… |
|-----|---------------------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | touch any module boundary, the pipeline, or the event/state model |
| [docs/BUILD.md](docs/BUILD.md) | configure/build; toolchain paths & the Win7 Qt 6 caveat |
| [docs/WINDOWS_APIS.md](docs/WINDOWS_APIS.md) | write `hardware/`, `drivers/`, or `system/` code |
| [docs/DRIVER_PROVIDER.md](docs/DRIVER_PROVIDER.md) | write anything in `drivers/` — portable-cache design, provider chain, DriverPack gate (ADR-0007) |
| [docs/CLI.md](docs/CLI.md) | add or change a `provisioner.exe` command |
| [docs/PROFILES.md](docs/PROFILES.md) | touch profile loading or ship a profile |
| [docs/ROADMAP.md](docs/ROADMAP.md) | start any milestone (exit criteria live here) |
| [docs/tasks/](docs/tasks/) | `MILESTONE-<n>.md` — the ordered checklist for the milestone you are on |
| [docs/TESTING.md](docs/TESTING.md) | write tests; safety rules for `pnputil`/`winget` |
| [docs/DECISIONS.md](docs/DECISIONS.md) | make a design choice or add a dependency (file an ADR) |

## 8. Current state & next task

**Done and reviewed:** Milestone 1 (skeleton, `SystemInspector`, `DeviceEnumerator`,
`provisioner scan`) and Milestone 2 (`DriverProvider`, `MockDriverProvider`,
`provisioner drivers scan`). Clean `-Werror` build, 7 offline test suites green.

**Present but not formally reviewed:** Milestone 5 groundwork — `ProfileLoader` (JSON),
`WinGetProvider`, `provisioner apps install --profile <name> [--dry-run]`.

**Next: Milestone 3** — portable driver cache + `DriverDownloader` + `LocalCacheProvider`
+ provider chain. Owner decisions are in [docs/DECISIONS.md](docs/DECISIONS.md) ADR-0004 /
0006 / 0007. Work [docs/tasks/MILESTONE-3.md](docs/tasks/MILESTONE-3.md) top to bottom,
report at each task, STOP at the exit criteria for review. Do not start Milestone 4.

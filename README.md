# Shiftech Win Provisioner

A native Windows tool that helps technicians provision laptops/PCs (Windows 7/8/10/11)
automatically after a fresh Windows install: hardware detection, driver resolution /
download / install / verification, and standard application installation driven by
selectable profiles — with structured logging and a final report.

**Status:** pre-implementation. This repository currently contains only the scaffold,
architecture docs, and agent working instructions. No application code has been written yet.

---

## What it does (target behaviour)

1. Detect Windows hardware/devices.
2. Identify devices with missing or problematic drivers.
3. Resolve drivers by Hardware ID via a pluggable `DriverProvider`.
4. Download drivers (with cache, retry, checksum verification).
5. Install drivers via `pnputil` (native Windows mechanism).
6. Re-verify device status after installation.
7. Skip devices whose driver cannot be found — never abort the whole run.
8. Install standard applications via WinGet, driven by a chosen profile.
9. Produce structured logs and a human + JSON report.

## Principles

- **No GUI-click automation** (no AutoIt-style simulation). Native Windows mechanisms only.
- **Core is UI-free.** The engine is a library usable from CLI, GUI, automation, or a service.
- **CLI first.** Every capability is reachable and testable from the command line before any GUI work.
- **One failure never stops the pipeline.** Failures are logged and the run continues.
- **Minimal dependencies.** C++20 + Qt 6 + Windows SDK. Nothing else unless justified.
- **Security-sensitive drivers.** Trusted source → download → verify → extract → validate → install. No Defender/signature-enforcement tampering.
- **No auto-reboot in V1.** Report "Reboot required" and design state for future resume.

## Documentation

| Doc | Purpose |
|-----|---------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | System design, module boundaries, data model, event model |
| [docs/BUILD.md](docs/BUILD.md) | Toolchain locations on this machine, how to configure & build |
| [docs/WINDOWS_APIS.md](docs/WINDOWS_APIS.md) | Which Windows APIs each module uses and why |
| [docs/DRIVER_PROVIDER.md](docs/DRIVER_PROVIDER.md) | DriverProvider contract + DriverPack investigation checklist (do this before implementing) |
| [docs/CLI.md](docs/CLI.md) | Command surface and expected output |
| [docs/PROFILES.md](docs/PROFILES.md) | Application profile file format |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Milestones 1–7 and their exit criteria |
| [docs/TESTING.md](docs/TESTING.md) | Unit/integration test strategy and safety rules |
| [docs/DECISIONS.md](docs/DECISIONS.md) | Architecture decision log (ADR-style) |

## For AI agents

Read **[AGENTS.md](AGENTS.md)** before doing anything — it is the single source of truth
(overview, toolchain, hard rules, conventions, testing, git, docs map, first task).
[CLAUDE.md](CLAUDE.md) is a thin shortcut to it with a few Claude-Code-specific notes.

## Repository layout

```
src/
  core/            UI-free engine library (shiftech_core)
    hardware/      DeviceEnumerator, Device model
    drivers/       DriverProvider, DriverDownloader, DriverInstaller, verification
    applications/  ApplicationProvider, WinGetProvider
    provisioning/  ProvisioningEngine, pipeline, state machine, events
    profiles/      profile loading/validation
    logging/       structured logger
    system/        system checks (OS version, arch, privilege, connectivity, tool availability)
  cli/             provisioner.exe — thin front-end over core
  gui/             Qt Widgets front-end (later milestone)
profiles/          shipped application profiles (standard/office/technician/developer)
cache/             runtime driver cache (gitignored)
logs/              runtime logs (gitignored)
tests/             unit + integration tests
docs/              documentation
cmake/             CMake helper modules
```

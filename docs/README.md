# docs/

| File | Contents |
|------|----------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | components, module responsibilities, data model, state machine, pipeline, build targets |
| [BUILD.md](BUILD.md) | detected toolchain paths, CMake options, configure/build/test commands, deployment, Win7 Qt 6 caveat |
| [WINDOWS_APIS.md](WINDOWS_APIS.md) | per-module Windows API choices (SetupAPI/CfgMgr32, pnputil, winget, RtlGetVersion, elevation) |
| [DRIVER_PROVIDER.md](DRIVER_PROVIDER.md) | `DriverProvider` interface, mock provider spec, **DriverPack investigation checklist (gate)**, downloader/cache design |
| [CLI.md](CLI.md) | `provisioner.exe` command surface, options, output contract, exit codes |
| [PROFILES.md](PROFILES.md) | application profile format (YAML reference / JSON loaded), fields, behaviour |
| [ROADMAP.md](ROADMAP.md) | Milestones 1–7 with scope and exit criteria |
| [TESTING.md](TESTING.md) | QtTest strategy, safety rules, unit/integration coverage, layout |
| [DECISIONS.md](DECISIONS.md) | ADR log (ADR-0001..0005; ADR-0004 blocks the real driver provider) |

Working rules for contributors/agents are in the repo root: [../AGENTS.md](../AGENTS.md),
[../CONTRIBUTING.md](../CONTRIBUTING.md).

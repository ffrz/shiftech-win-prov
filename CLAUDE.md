# CLAUDE.md

**→ Read [AGENTS.md](AGENTS.md). It is the single source of truth for working in this repo**
(project overview, environment/toolchain, hard rules, coding conventions, testing, git,
the documentation map, and the current first task). Everything an agent needs is there or
linked from there.

This file only adds Claude-Code-specific notes.

## Claude-specific

- When the user asks to commit ("commit", "commit semua", "commit dulu", …), use the
  **`commit` skill**. Per [AGENTS.md](AGENTS.md) §6, commits here carry **no**
  `Co-Authored-By` trailer.
- Work on a feature branch, never the default branch.
- For questions about the codebase / architecture, the `graphify` skill and
  [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) are the entry points.
- Stay within the current milestone in [docs/ROADMAP.md](docs/ROADMAP.md); stop and report
  at each milestone's exit criteria before continuing.
- No MSVC on this machine — build via `scripts\build.bat` (MinGW). See
  [docs/BUILD.md](docs/BUILD.md).

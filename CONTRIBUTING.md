# Contributing

Human and AI contributors follow the same rules. **[AGENTS.md](AGENTS.md)** is the
authoritative working contract — read it first.

## Workflow

1. Branch off the default branch: `feat/<short-name>` or `fix/<short-name>`.
2. Read the `docs/` file for the module you're touching (see the map in AGENTS.md §7).
3. Keep `src/core/` UI-free (Qt Core only).
4. Add/update unit tests for any pure logic you add.
5. `scripts\build.bat` and `scripts\test.bat` must pass before you push.
6. Work within the current milestone in [docs/ROADMAP.md](docs/ROADMAP.md). Hitting a
   milestone's exit criteria = stop, summarize, request review.
7. Any new third-party dependency or notable design choice → an ADR in
   [docs/DECISIONS.md](docs/DECISIONS.md).

## Commits

- Subject style: `scope(area): summary` — e.g. `core(hardware): add DeviceEnumerator`.
- No `Co-Authored-By` trailer in this repo.
- Never commit `build/`, `cache/` contents, or `logs/` contents.

## Code style

- `.clang-format` and `.editorconfig` are provided; run clang-format on changed files.
- One class per file; `#pragma once`; namespace `shiftech::core::<module>`.
- Public core API in plain C++ types; Qt types allowed at boundaries and in impl.

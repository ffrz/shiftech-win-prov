# Prompt for the next milestone (reusable)

> Paste the fenced block as the first message to the implementing agent, from the repo
> root. Set `<N>` to the milestone number you want done next. Milestones 1–2 are complete
> and reviewed; Milestone 5 groundwork exists. **Next up is Milestone 3.**

---

```
You are a senior Windows systems engineer and modern C++/Qt developer, continuing an
existing project at the current working directory. Milestones 1 and 2 are complete and
reviewed; some Milestone 5 groundwork (profiles + winget) also exists.

## Step 0 — Orient (before any code)

Read, in order:
1. AGENTS.md              — the authoritative working contract. Section 8 says where things stand.
2. docs/ARCHITECTURE.md   — module boundaries, Device / event / state model, provider chain.
3. docs/DECISIONS.md      — ADR-0004 / 0006 / 0007 are the owner's Milestone 3 decisions. READ THESE.
4. docs/DRIVER_PROVIDER.md — portable-cache design, provider chain, the DriverPack gate.
5. docs/BUILD.md          — toolchain on this machine, how to build/test.
6. docs/TESTING.md        — framework + safety rules (integration tests are VM-only, gated).
7. docs/tasks/MILESTONE-3.md — your ordered checklist for THIS milestone.

Then confirm back to me, briefly:
- the build + test commands you'll use,
- the provider chain order you'll implement and which providers are real vs stub this milestone,
- how you'll keep the driver cache portable (relative paths, no absolute paths in metadata),
- anything in MILESTONE-3.md you'd change before starting (or "none").

Do not write code until I reply "go".

## Scope

Implement **Milestone 3 only** — the portable driver cache, DriverDownloader,
LocalCacheProvider, and the provider-chain plumbing, per docs/tasks/MILESTONE-3.md.
WindowsUpdateProvider and MirrorProvider are honest STUBS this milestone.
Do the DriverPack sub-investigation and write ADR-0007; get my sign-off before writing
any DriverPackProvider. Do NOT start Milestone 4 (install/verify).

## Hard rules (full list in AGENTS.md §3)

- `src/core/` stays UI-free (Qt Core only, no Qt Widgets/GUI).
- Everything portable: all paths relative to the executable; no `C:\` or user-profile
  paths; `metadata.json` / `index.json` contain no absolute paths.
- Provider chain: try each in order, first `found` wins, a provider error/timeout is
  skipped, the run never aborts.
- Unverifiable package => warn + skip (ADR-0006). No interactive prompt.
- Every external op (network, QProcess, file IO) has a timeout + error handling.
- New third-party dependency => ADR + my sign-off first.
- Build with MinGW via scripts\build.bat (no MSVC on this machine).

## Working method

1. Post the Step 0 confirmation. Wait for "go".
2. Work MILESTONE-3.md task by task; after each, state what changed + build/test result.
3. Small scoped commits. No Co-Authored-By trailer (this repo uses the commit skill / none).
4. When every exit criterion is met: clean build, run
   `provisioner drivers resolve --download` and paste output, run the tests and paste
   results, give a change summary (files, decisions, deviations, stubs).
5. STOP and wait for my review. Do not start Milestone 4.

## If blocked

Ambiguous doc, or the machine contradicts a doc: stop and ask. Don't guess, don't silently
deviate. If you think a design in the docs is wrong, say so in Step 0 with a concrete
alternative.

Begin with Step 0.
```

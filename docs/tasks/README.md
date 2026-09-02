# docs/tasks/ — per-milestone task lists

One file per milestone. Each is a concrete, ordered checklist an implementing agent works
through so it does not lose direction between context windows. They expand
[../ROADMAP.md](../ROADMAP.md) — the roadmap has the scope and exit criteria; these files
have the step-by-step.

| File | Milestone | Feed it to the agent when… |
|------|-----------|-----------------------------|
| [MILESTONE-1.md](MILESTONE-1.md) | Skeleton + hardware detection | starting the project (see `GEMINI_INITIAL_PROMPT.md` at repo root) |
| [MILESTONE-2.md](MILESTONE-2.md) | DriverProvider abstraction + mock | M1 reviewed and accepted |
| [MILESTONE-3.md](MILESTONE-3.md) | DriverPack investigation + downloader + cache | M2 reviewed |
| [MILESTONE-4.md](MILESTONE-4.md) | Driver install + verification | M3 reviewed |
| [MILESTONE-5.md](MILESTONE-5.md) | Application provisioning (winget + profiles) | M4 reviewed |
| [MILESTONE-6.md](MILESTONE-6.md) | Provisioning pipeline + logging + report | M5 reviewed |
| [MILESTONE-7.md](MILESTONE-7.md) | Qt GUI | M6 reviewed |

## How to run a milestone

1. Give the agent the milestone file plus a one-line "you are continuing the project;
   AGENTS.md still applies; implement only this milestone".
2. The agent works the checklist top to bottom, reporting after each task.
3. At the end: clean build, run the milestone's demo command, run tests, change summary.
4. Human review against the **Exit criteria** section. Only then start the next file.

## Editing these files

If reality diverges (an API doesn't exist, a design choice changes), update the affected
task file and add an ADR in [../DECISIONS.md](../DECISIONS.md). Keep the checklists honest.

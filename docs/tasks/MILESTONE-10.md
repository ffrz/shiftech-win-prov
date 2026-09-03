# Milestone 10 — `provisioner reset` (undo a run for testbed re-use)

**Goal:** undo a provisioning run so a technician's test laptop goes back to (near) its
pre-provisioning state and can be re-tested. Reads what to undo from the run's `run.json`.

Prereq: Milestone 8 accepted. Read [../CLI.md](../CLI.md) and the "Logging" section.

---

## Status — DONE (2026-09-04)

### What it undoes
Read from `logs/<runId>/run.json` → `state`:

| Section | Undone if the run… | Mechanism |
|---------|--------------------|-----------|
| **apps** | *installed* it (`status == "installed"`) | winget: `winget uninstall --id <id> --exact --silent`; local: **not automated** — reported `not-supported` (uninstall via Add/Remove Programs) |
| **drivers** | published an `oemNN.inf` (`publishedInfs`) | `pnputil /delete-driver <oemNN.inf> /uninstall /force` |
| **config** | *applied* it (`outcome == "Applied"` / `"RequiresReboot"`) | `config::revertTweak(id)` — each tweak's `revert()` |

Apps/drivers/tweaks the run only **detected** as already-present are left alone.

### Config tweak reverts (`ConfigTweak::revert()`)
| tweak | revert |
|-------|--------|
| show-file-extensions / show-hidden-files | HKCU value → Windows default (1 / 2), restart Explorer |
| disable-fast-startup | HiberbootEnabled → 1 |
| disable-password-expiry | `net accounts /maxpwage:42` + PasswordExpires=true |
| set-power-high-performance | active scheme → Balanced |
| disable-hibernate | `powercfg /hibernate on` |
| enable-rdp | fDenyTSConnections → 1 + firewall rule off |
| clean-startup-items / disable-startup-item | re-enable the StartupApproved bytes |
| **clean-taskbar-pins** | `NotSupported` — removed pins can't be reconstructed (Windows re-adds defaults on sign-in) |
| **set-timezone** | `NotSupported` — no "default" to go back to |
| **set-computer-name** | `NotSupported` — previous name not recorded |

### CLI
```
provisioner reset [--last | --run <id>] [--dry-run] [--json]
                  [--skip-apps] [--skip-drivers] [--skip-config] [--purge-cache]
                  [--log-dir <p>] [--cache-dir <p>]
```
- `--dry-run` prints the plan, mutates nothing.
- `--purge-cache` also empties the portable `cache/drivers/` (not a Windows change).
- Elevation required for driver + machine-wide config reverts; without it those report
  `skipped`. Exit 0, or 1 if anything failed.

### Enriched `run.json` (so reset knows what to undo)
- `DriverItemResult.publishedInfs` — the `oemNN.inf` names pnputil added, mapped back per
  device via the inf→instanceId table the engine builds.
- `AppItemResult.source` + `wingetId`.

### Tests (24 suites, all offline, **no machine mutation**)
- `test_resetengine` — dry-run against a fake `run.json`: lists chrome + winrar (installed)
  but not 7zip (already-present); lists the published `oem42.inf`; lists the two applied
  tweaks but not the already-applied one; `--skip-*` honoured; missing/absent run → fatal.
- `test_configtweak` — revert guards only: unknown id → Failed, unelevated admin tweak →
  Skipped, irreversible tweak → NotSupported. **No revert is run for real** on a tweak
  that mutates the machine.

## Verified on this machine (dry-run only)
`provisioner reset --last --dry-run` on a crafted `run.json` correctly printed:
```
chrome: would uninstall
winrar: would uninstall
oem57.inf: would delete-driver
show-file-extensions: would revert
clean-taskbar-pins: would revert
```
(7zip, marked `already_installed`, was left out.)

## Not done here — VM only
The **real** reset (`winget uninstall`, `pnputil /delete-driver`, tweak reverts) has not
been run outside dry-run. Procedure for a VM:
1. `provisioner provision --profile <p>` elevated.
2. `provisioner reset --last` elevated.
3. Assert: the winget apps are gone (`winget list`), `pnputil /enum-drivers` no longer
   lists the oemNN.inf, the reverted registry values are back to default.
4. `not-supported` items (local apps, cleaned pins, timezone) are expected — undo those by
   hand.

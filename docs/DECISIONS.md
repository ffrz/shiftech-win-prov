# Architecture Decision Log

Lightweight ADRs. Add a new entry (never rewrite history) when a significant choice is
made. Status: `proposed` | `accepted` | `superseded by ADR-XXXX`.

---

## ADR-0001 — C++20 + Qt 6, CMake + Ninja, MinGW
**Status:** accepted (from the initial spec + detected toolchain)
**Context:** Native Windows tool, minimal deps, core reusable without a GUI. Only MinGW
(no MSVC) is installed on the build machine.
**Decision:** C++20. Qt 6 (Core everywhere, Widgets only in the GUI target). CMake + Ninja.
Default kit Qt 6.6.2 + MinGW 11.2.0; Qt 6.11.1 + GCC 13.1 available as an alternate.
**Consequences:** No `cl.exe` assumptions. `windeployqt` or static Qt for distribution.
Windows 7/8 support for a Qt 6 GUI is at risk — see ADR-0003.

## ADR-0002 — Profile file format: JSON now, YAML pending
**Status:** accepted (JSON-only)
**Context:** Spec shows YAML profiles. Qt has no bundled YAML parser; adding one is a
dependency.
**Decision:** `ProfileLoader` loads `.json`. We will forgo YAML entirely for V1 to keep
the executable self-contained without adding vendored YAML parsing dependencies.
**Consequences:** Profiles must be authored in JSON.

## ADR-0003 — Windows 7/8 support for a Qt 6 GUI (OPEN RISK)
**Status:** proposed
**Context:** Targets include Windows 7 (x86 + x64) and Windows 8 x64. Qt 6 officially
requires Windows 10+.
**Options:** (a) Qt 5.15 build of the GUI for legacy OS; (b) static Qt 6 build + test on
Win7 (may still fail on missing APIs); (c) ship the **CLI** (kept Qt-minimal / Qt-Core-only
or Qt-free) as the Win7/8 story and Qt 6 GUI for Win10/11 only.
**Decision:** deferred to Milestone 7. Until then: keep `shiftech_core` + `provisioner`
buildable with the smallest possible Qt surface so option (c) stays viable.
**Consequences:** Avoid Qt-only constructs in core where a std equivalent exists.

## ADR-0004 — Driver resolution: portable local cache first, then a chain of providers
**Status:** accepted (owner decision 2026-09-03); DriverPack sub-investigation still **open**
**Context:** The tool and its driver cache must be **fully portable** — copied onto a USB
flash drive and run from there for fast, autonomous provisioning of machines that may have
no/limited internet. Resolution should try the fastest source first.

**Decision:**
1. **`DriverProvider` is a chain.** `ProvisioningEngine` queries providers in priority
   order and stops at the first usable `DriverPackage` (subject to the security gate below).
   Default order:
   1. **`LocalCacheProvider`** — the portable `cache/drivers/` tree on the same medium as
      the exe. Zero network. Always tried first.
   2. **`DriverPackProvider`** — *only if* the DriverPack sub-investigation (below) yields a
      reliable + license-permitted direct-download-by-Hardware-ID mechanism. Preferred over
      Windows Update when it is fast and has the driver.
   3. **`WindowsUpdateProvider`** — COM `IUpdateSearcher` with the driver category.
      Microsoft-hosted, signed, licensed, stable. Needs internet.
   4. **`MirrorProvider`** — an internal HTTP/file share with a JSON index keyed by
      Hardware ID. Last resort; fully under our control.
   Order is configurable (`--provider-order`, or a config file on the USB medium).
2. **The cache is the portable artifact.** Everything any provider downloads is written into
   `cache/drivers/<packageId>/` (deterministic id, see [DRIVER_PROVIDER.md](DRIVER_PROVIDER.md)).
   A technician can pre-populate the flash drive by running resolution once on a
   connected machine; subsequent runs are offline. `packageId` must be path-independent
   (no absolute paths inside `metadata.json`) so the tree survives being moved between
   drive letters / machines.
3. **Paths are all relative to the executable**, never to `C:\` or a user profile, so the
   whole folder is relocatable.
4. **Unverifiable package ⇒ warn + skip** (ADR-0006).

**DriverPack sub-investigation (still required before writing `DriverPackProvider`):**
the checklist in [DRIVER_PROVIDER.md](DRIVER_PROVIDER.md) — official API vs scrape, direct
download by Hardware ID, **ToS for automated download + redistribution onto technician
media**, checksums/signatures, stability. If it fails any of these, `DriverPackProvider`
is dropped from the chain and #3/#4 carry the load; record the finding as ADR-0007.

**Consequences:**
- `LocalCacheProvider` + `DriverDownloader` + the portable cache can be built and tested
  now (Milestone 3) regardless of the DriverPack question.
- `MockDriverProvider` stays as the test/dev provider and is not in the default chain.
- The engine must treat "provider chain" as first-class: try, fall through on
  not-found/error, never abort the run.

## ADR-0006 — Unverifiable driver package ⇒ warn + skip
**Status:** accepted (owner decision 2026-09-03)
**Context:** Drivers are security-sensitive. Some packages arrive with no checksum and no
signature catalog (`CatalogFile`).
**Decision:** If a package cannot be verified (no checksum to match **and** no signed
catalog), the pipeline logs a prominent **warning**, marks the device `Skipped`, and
continues. It does **not** install unverified packages, and it does **not** prompt
interactively in V1. `pnputil` still enforces Windows driver signing for anything that
does get installed. A future `--force-unverified` flag may override this for lab use.
**Consequences:** some devices will be left without a driver on offline/unsigned sources;
that is the accepted trade-off. The report must list skipped-unverified devices distinctly.

## ADR-0007 — DriverPack sub-investigation outcome
**Status:** not started — sub-task of ADR-0004
**Context:** ADR-0004 puts `DriverPackProvider` at priority #2 *conditionally*. This ADR
records whether that condition is met.
**Decision:** _to be written_ after completing the checklist in
[DRIVER_PROVIDER.md](DRIVER_PROVIDER.md). Either "DriverPack is in the chain, mechanism =
…", or "DriverPack dropped, reason = …".
**Consequences:** until written, the default provider chain is Local → WindowsUpdate →
Mirror (DriverPack omitted).

## ADR-0005 — Archive extraction
**Status:** proposed
**Context:** Driver packages arrive as zip/cab. Need extraction without a heavy dependency.
**Decision (interim):** use Windows built-ins — `tar.exe` (bsdtar, present Win10 1803+) for
zip, `expand.exe` for cab; on older OS fall back to Shell.Application `CopyHere`. File an
ADR before vendoring a library if this proves insufficient for the Win7 target.
**Consequences:** extraction path is OS-version dependent; cover in integration tests.

---

### Template

```
## ADR-XXXX — <title>
**Status:** proposed
**Context:** <why a decision is needed>
**Decision:** <what was chosen>
**Consequences:** <trade-offs, follow-ups>
```

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

## ADR-0003 — Windows 7/8 support: CLI-only on legacy, Qt 6 GUI for Win10/11
**Status:** accepted (Milestone 7, 2026-09-03)
**Context:** Targets include Windows 7 (x86 + x64) and Windows 8 x64. Qt 6.6 officially
requires Windows 10 1809+. A dedicated Win7 VM was not available to test on during M7.
**Decision:** **Option (c).** On Windows 7/8, ship **`provisioner.exe` (CLI) only**. The
Qt 6 Widgets GUI (`app.exe`) targets Windows 10/11.
- `shiftech_core` + the CLI stay on the smallest practical Qt surface (Core + Network) so
  a future static or Qt-5.15 CLI build for Win7 remains a small change.
- The CLI already covers 100% of functionality (`scan`, `drivers scan/resolve/install`,
  `apps install`, `provision`, `report`), so legacy machines lose only the dashboard.
- The portable USB workflow is CLI-driven anyway (`provisioner provision --profile … `),
  which is the primary legacy-machine use case.
**Not chosen:** (a) a parallel Qt 5.15 GUI build doubles the UI maintenance for a shrinking
OS base; (b) a static Qt 6 build still risks missing Win7 APIs (`d3d`, `dcomp`,
user-mode font APIs) and needs a source build of Qt.
**Revisit if:** a customer specifically needs the GUI on Win7/8 — then do (a) as a
separate `shiftech_gui_legacy` target, reusing `EngineController`/`MainWindow` largely
unchanged (they are plain Widgets).
**Consequences:** the installer/packaging for legacy media bundles only the CLI + its Qt
Core/Network DLLs (smaller). Keep avoiding Qt-only constructs in core where a std
equivalent exists.

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
   2. **`WindowsUpdateProvider`** — COM `IUpdateSearcher` with the driver category.
      Microsoft-hosted, signed, licensed, stable. Needs internet (or an offline scan
      package). *(DriverPack was evaluated for this slot and rejected — see ADR-0007.)*
   3. **`MirrorProvider`** — an internal HTTP/file share with a JSON index keyed by
      Hardware ID. For hardware Windows Update does not carry; fully under our control.
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

**DriverPack sub-investigation:** completed — see **ADR-0007**. Outcome: **dropped**
(license forbids redistribution, no API). Chain is `LocalCache → WindowsUpdate → Mirror`.

**Consequences:**
- `LocalCacheProvider` + `DriverDownloader` + the portable cache built and tested in
  Milestone 3.
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

## ADR-0007 — DriverPack sub-investigation outcome: DROPPED
**Status:** accepted (investigation 2026-09-03)
**Context:** ADR-0004 puts `DriverPackProvider` at priority #2 *conditionally*, subject to a
license + mechanism check.
**Findings:**
- **No official API.** driverpack.io exposes a by-hardware-ID browse page
  (`/en/hwids`) but no documented programmatic endpoint; the page is Cloudflare-gated
  (403 to non-browser clients). Any integration would be HTML scraping.
- **License forbids our use.** The DriverPack License Agreement
  (<https://driverpack.io/en/info/terms-of-use>): *"The Software may be used solely for
  personal, informational, noncommercial purposes; The Software may not be
  redistributed."* and *"the official version of the Solution is distributed only on the
  website https://driverpack.io"*. Bundling drivers pulled from DriverPack onto a
  technician USB drive for provisioning customer machines is commercial redistribution —
  **not permitted.**
- **Integrity:** no public per-file checksum/signature manifest independent of the client.
**Decision:** **`DriverPackProvider` is dropped.** It will not be built. The provider chain
is **`LocalCache → WindowsUpdate → Mirror`**.
- **WindowsUpdateProvider** (new priority #2): COM `IUpdateSearcher` with criteria
  `IsInstalled=0 and Type='Driver'`, `ServerSelection = ssWindowsUpdate` (online) or an
  offline scan package (`wsusscn2.cab`) when configured. Microsoft-hosted, WHQL-signed,
  licensed for OEM/enterprise provisioning, works Win7→Win11. This is the primary *online*
  source.
- **MirrorProvider** (priority #3): an internal HTTP/file-share driver repo with a JSON
  index keyed by Hardware ID, fully under our control — for drivers Windows Update does
  not carry (niche OEM hardware).
**Consequences:**
- Remove the `driverpack` option from `DriverProviderFactory`; the CLI should reject it
  with "unsupported (see ADR-0007)".
- The portable `LocalCacheProvider` + pre-population workflow becomes even more important,
  since WindowsUpdate needs connectivity and Mirror needs infrastructure.
- If a future curated/licensed bulk driver source appears, add it as another chain entry
  via a new ADR — do not revisit DriverPack.

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

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
**Status:** proposed
**Context:** Spec shows YAML profiles. Qt has no bundled YAML parser; adding one is a
dependency.
**Decision (interim):** `ProfileLoader` loads `.json`. Ship `.yaml` as human-readable
references with `.json` twins kept in sync. Revisit adding a single-header YAML parser
(or a tiny subset parser) before Milestone 5 ships.
**Consequences:** Two files per profile until resolved. Decide and supersede this ADR.

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

## ADR-0004 — DriverPack integration mechanism
**Status:** not started — **blocks `DriverPackProvider` (Milestone 3)**
**Context:** Must not assume DriverPack's site/API. See the checklist in
[DRIVER_PROVIDER.md](DRIVER_PROVIDER.md).
**Decision:** _to be written after the investigation._ Must cover: API vs scrape, direct
download by Hardware ID, licensing/ToS for automated download + redistribution, integrity
(checksums/signatures), stability, and alternatives (Windows Update driver catalog,
Microsoft Update Catalog, vendor catalogs, internal mirror).
**Consequences:** until accepted, only `MockDriverProvider` exists.

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

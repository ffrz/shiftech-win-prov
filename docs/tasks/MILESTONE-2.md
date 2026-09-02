# Milestone 2 — DriverProvider abstraction + Mock provider

**Goal:** a pluggable `DriverProvider` interface and a deterministic offline
`MockDriverProvider`, surfaced through `provisioner.exe drivers scan`. **No real DriverPack
code** — that is gated by ADR-0004 ([../DECISIONS.md](../DECISIONS.md)).

Prereq: Milestone 1 accepted. Read [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md) and
[../ARCHITECTURE.md](../ARCHITECTURE.md) §3 (`drivers/`).

---

## Task 1 — Interface + value types

- [ ] `src/core/drivers/DriverProvider.h`: `TargetSystem`, `PackageType`, `DriverPackage`,
      `DriverSearchResult`, and the abstract `DriverProvider` exactly as in
      [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md). Plain types, virtual dtor,
      `std::string name() const = 0`.
- [ ] `src/core/drivers/TargetSystem.cpp`: helper `TargetSystem currentTarget(const SystemInfo&)`
      mapping `SystemInfo` → `OsFamily`/`build`/`Arch`.

## Task 2 — Driver matching logic (pure, unit-tested)

- [ ] `src/core/drivers/DriverMatch.h/.cpp` — no OS/network:
      - `int rankCandidate(const Device&, const DriverPackage&, const TargetSystem&)`:
        exact Hardware ID hit > Compatible ID hit; correct arch required; OS in
        `supportedOs` required; newer `version` ranks higher (implement a `compareVersions`).
      - `std::optional<DriverPackage> pickBest(const Device&, const DriverSearchResult&, const TargetSystem&)`.
      - `compareVersions("1.2.3.4", "1.10.0.0")` → correct numeric-segment comparison.

## Task 3 — MockDriverProvider

- [ ] `src/core/drivers/MockDriverProvider.h/.cpp` — constructed with a path to a JSON
      index; `search()` looks up each of the device's hardware + compatible IDs and returns
      matching `DriverPackage`s (best first). Offline, deterministic, no exceptions on a
      missing file (return `found=false`, reason).
- [ ] `tests/fixtures/driver_index.json`: `{ "PCI\\VEN_10EC&DEV_8168": [ { "driverName": ...,
      "version": ..., "provider": ..., "supportedOs": ["win10","win11"], "arch": "x64",
      "downloadUrl": "file:///.../tests/fixtures/packages/realtek_nic.zip",
      "packageType": "InfZip", "checksum": "", "checksumAlgo": "" } ], ... }` — cover 2–3
      real-looking IDs incl. one that also matches via a Compatible ID.
- [ ] `tests/fixtures/packages/` — tiny placeholder zip(s) (a folder with one dummy `.inf`).

## Task 4 — Wire a provider into the CLI

- [ ] A `DriverProviderFactory` / simple switch: `--provider mock` (default for now) →
      `MockDriverProvider` with the fixture index (or a `--driver-index <path>` override).
      Leave a clearly-marked TODO slot for `driverpack` (throws "not implemented — ADR-0004").
- [ ] `src/cli/commands/DriversScanCommand.*` — `provisioner.exe drivers scan`:
      - enumerate devices needing a driver,
      - for each, call the provider, run `pickBest`,
      - print: device, matched package (name/version/provider) or `NOT FOUND (<reason>)`.
      - `--json`: `{ "target": {...}, "results": [ { "device": {...}, "match": {...}|null,
        "candidates": [...], "reason": "" } ] }`.

## Task 5 — Tests

- [ ] `tests/unit/test_drivermatch.cpp`: ranking (HWID > CompatID), arch filter, OS filter,
      version comparison, `pickBest` with no candidates.
- [ ] `tests/unit/test_mockprovider.cpp`: known ID → expected package; unknown ID →
      `found=false`; missing index file → graceful `found=false` with reason.
- [ ] `scripts\test.bat` green.

---

## Exit criteria

- [ ] `provisioner.exe drivers scan` prints a per-device resolution result on this machine
      (most will be NOT FOUND against the tiny fixture index — that's expected).
- [ ] `--json` valid.
- [ ] Matching + version-compare + mock-provider unit tests pass offline.
- [ ] No real DriverPack code anywhere; `driverpack` provider path throws a clear
      "not implemented (ADR-0004)".
- [ ] Change summary + pasted `drivers scan` / test output. STOP for review.

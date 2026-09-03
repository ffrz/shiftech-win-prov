# Milestone 3 — Portable driver cache + Downloader + Provider chain

**Goal:** a robust, **portable** `DriverDownloader` + `cache/drivers/` tree, a
`LocalCacheProvider` that reads it offline, and the provider-chain plumbing in the engine.
Plus the DriverPack sub-investigation (ADR-0007).

The owner decision is recorded in **[../DECISIONS.md](../DECISIONS.md) ADR-0004 / ADR-0006** —
read it before starting. Key points:
- Tool + cache run **from a USB flash drive**. All paths relative to the executable.
- Resolution is a **chain**: LocalCache → (DriverPack?) → WindowsUpdate → Mirror. First
  usable package wins.
- Unverifiable package ⇒ **warn + skip** (ADR-0006).

Prereq: Milestones 1–2 accepted. Read [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md) fully.

---

## Task 1 — Portable cache + `DriverDownloader`

- [ ] `src/core/drivers/DriverCache.h/.cpp`:
      - `std::string packageId(const DriverPackage&)` =
        `sha256(provider|driverName|version|archStr|downloadUrl)` truncated to 16 hex chars
        (`QCryptographicHash`). **Deterministic and path-independent.**
      - `std::filesystem::path cacheRoot()` = `<exeDir>/cache/drivers` (override via ctor
        arg / `--cache-dir`). Never an absolute `C:\` or profile path.
      - `bool has(const DriverPackage&)`, `path dir(const DriverPackage&)`,
        `writeMetadata(...)`, `readMetadata(...)`.
      - `metadata.json` contains the `DriverPackage` fields + checksum + fetch timestamp
        and **no absolute paths** (so the tree survives being moved between drives).
- [ ] `src/core/drivers/DriverDownloader.h/.cpp` using `QNetworkAccessManager`:
      - `struct DownloadResult { bool ok; std::string localPath; std::string error; bool fromCache; }`
      - progress callback `std::function<void(qint64 received, qint64 total)>`
      - retry 3×, backoff 1s/4s/9s, per-attempt timeout (default 120s, configurable)
      - resume via HTTP `Range` when server sends `Accept-Ranges: bytes`
      - download to `package.<ext>.part`, verify, atomic rename into the cache dir
      - `file://` URLs supported (fixtures act as a local "server")
      - if `has(package)` and (checksum matches, or no checksum and size > 0) → return
        `fromCache = true`, no network.
- [ ] Checksum verification when `package.checksum` is set (algo from `checksumAlgo`).

## Task 2 — `LocalCacheProvider`

- [ ] `src/core/drivers/LocalCacheProvider.h/.cpp` implementing `DriverProvider`:
      - reads an **index** of what's in the portable cache. Recommended: a
        `cache/drivers/index.json` mapping Hardware/Compatible ID → `[packageId]`, written
        by `DriverDownloader` whenever it stores a package, plus a scan-and-rebuild path.
      - `search()` returns `DriverPackage`s for cached packages that match the device's
        IDs, `matchedVia` set correctly (HardwareId vs CompatibleId), `downloadUrl` pointing
        at the local `file://` path in the cache.
      - Zero network. Never throws.
- [ ] `name()` = `"localcache"`.

## Task 3 — Provider chain in the resolution path

- [ ] `src/core/drivers/ProviderChain.h/.cpp`: holds an ordered `vector<unique_ptr<DriverProvider>>`;
      `DriverSearchResult resolve(const Device&, const TargetSystem&)` tries each provider,
      returns the first with `found == true`, aggregates `notFoundReason`s otherwise.
- [ ] `DriverProviderFactory`: build the chain from a spec —
      default `"localcache,windowsupdate,mirror"` (DriverPack added only per ADR-0007).
      `--provider-order <csv>` overrides. `mock` selectable explicitly for testing.
- [ ] `WindowsUpdateProvider` / `MirrorProvider`: **stubs** in this milestone — real impl is
      Milestone 3.5 / 4 follow-up. They must compile, return `found=false` with a clear
      "not implemented yet" reason, and be wired into the factory so the chain is exercised.
- [ ] `drivers scan` uses the chain (not a single provider) unless `--provider` forces one.

## Task 4 — DriverPack sub-investigation → ADR-0007

Work the checklist in [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md). Capture evidence
(real request/response, quoted ToS clause). Decide **one**:
- DriverPack is in the chain — document the exact reliable download-by-Hardware-ID
  mechanism, its position vs WindowsUpdate, checksum story; **or**
- DriverPack is dropped — record why (no API / ToS forbids redistribution to media /
  scraping too fragile). Chain stays LocalCache → WindowsUpdate → Mirror.

Write **ADR-0007** in [../DECISIONS.md](../DECISIONS.md). Get owner sign-off before
implementing any `DriverPackProvider`.

## Task 5 — CLI

- [ ] `provisioner.exe drivers resolve [--download] [--cache-dir <path>] [--provider-order <csv>]`:
      for every device needing a driver, run the chain; with `--download`, fetch the winning
      package into the portable cache and update `index.json`. Print `cache hit` vs
      `downloaded` vs `NOT FOUND` per device. One failure never aborts the batch.
- [ ] `drivers scan` keeps working (resolution only, no download).

## Task 6 — Tests (all offline)

- [ ] `tests/unit/test_cacheid.cpp`: `packageId` deterministic for same inputs; changes when
      version/url/arch changes; contains no path separators.
- [ ] `tests/unit/test_downloader.cpp`: against a **local `QHttpServer`/`QTcpServer` fixture**
      — success, 404, mid-stream disconnect + retry, `Range` resume, checksum mismatch →
      failure, cache hit skips network, `file://` source.
- [ ] `tests/unit/test_localcacheprovider.cpp`: given a fixture cache tree + `index.json`,
      resolves a known ID, misses an unknown ID, `matchedVia` correct.
- [ ] `tests/unit/test_providerchain.cpp`: chain falls through provider 1 (not found) to
      provider 2 (found); returns first hit; all-miss aggregates reasons.
- [ ] `scripts\test.bat` green. **No test hits the internet.**

---

## Exit criteria

- [ ] ADR-0007 written, evidence-backed, owner-signed-off.
- [ ] `DriverDownloader` + `DriverCache` unit tests pass against a local fixture server.
- [ ] Cache is portable: move `cache/drivers/` to a different path, `LocalCacheProvider`
      still resolves from it (test proves this).
- [ ] `drivers resolve --download` populates the cache; a second run reports `cache hit`.
- [ ] Provider chain falls through correctly; one provider error never aborts the run.
- [ ] `WindowsUpdateProvider` / `MirrorProvider` compile as honest stubs.
- [ ] Change summary + ADR-0007 + test output. STOP for review.

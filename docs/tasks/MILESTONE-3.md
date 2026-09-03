# Milestone 3 — Portable driver cache + Downloader + Provider chain

**Goal:** a robust, **portable** `DriverDownloader` + `cache/drivers/` tree, a
`LocalCacheProvider` that reads it offline, and the provider-chain plumbing in the engine.
DriverPack was investigated and **rejected** (ADR-0007).

The owner decisions are in **[../DECISIONS.md](../DECISIONS.md) ADR-0004 / 0006 / 0007** —
read them before starting. Key points:
- Tool + cache run **from a USB flash drive**. All paths relative to the executable.
- Resolution is a **chain**: `LocalCache → WindowsUpdate → Mirror`. First usable package wins.
  **Do not build `DriverPackProvider`** — license forbids it.
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
      default `"localcache,windowsupdate,mirror"`.
      `--provider-order <csv>` overrides. `mock` selectable explicitly for testing.
      `driverpack` is rejected with "unsupported (ADR-0007)".
- [ ] `WindowsUpdateProvider` / `MirrorProvider`: **stubs** in this milestone — real impl is
      Milestone 3.5 / 4 follow-up. They must compile, return `found=false` with a clear
      "not implemented yet" reason, and be wired into the factory so the chain is exercised.
- [ ] `drivers scan` uses the chain (not a single provider) unless `--provider` forces one.

## Task 4 — DriverPack sub-investigation → ADR-0007 ✅ DONE

Completed 2026-09-03. **DriverPack dropped** — license forbids redistribution, no API.
See [../DECISIONS.md](../DECISIONS.md) ADR-0007. Nothing further to do here; do not build
`DriverPackProvider`.

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

## Exit criteria — DONE (2026-09-03)

- [x] ADR-0007 written and accepted: **DriverPack dropped** (license forbids
      redistribution, no API). Chain = LocalCache → WindowsUpdate → Mirror.
- [x] `DriverDownloader` + `DriverCache` unit tests pass against a local `QTcpServer`
      fixture — success, 404/no-listener, mid-stream drop + retry, `Range` resume,
      checksum mismatch/match, cache-hit skips network, `file://` source.
- [x] Cache is portable: `test_localcacheprovider::survivesCacheDirMove` renames the whole
      cache tree and still resolves.
- [x] `drivers resolve --download` populates the portable cache (verified on this machine
      via a mock index → `portable_cache/<id>/nic.zip` + `metadata.json` + `index.json`);
      a follow-up `--provider-order localcache` run resolves offline with no mock.
- [x] Provider chain falls through and aggregates reasons; a throwing provider is caught
      (`test_providerchain`).
- [x] `WindowsUpdateProvider` / `MirrorProvider` compile as honest stubs (clear
      "not implemented yet" reason).
- [x] `driverpack` token rejected by the factory with an ADR-0007 message.

### Deviations / notes
- `packageId` includes the source `downloadUrl` per the design, so the same driver from
  two different source URLs caches twice. Accepted for V1.
- `metadata.json` keeps the original `downloadUrl` as **provenance**; `LocalCacheProvider`
  never uses it to locate the payload (always `<packageId>/<payloadFileName>`), so a stale
  absolute path there does not break relocation. Documented in DRIVER_PROVIDER.md.
- Real `WindowsUpdateProvider` (COM `IUpdateSearcher`) and `MirrorProvider` are a
  **Milestone 3.5** follow-up — see [MILESTONE-3.5.md](MILESTONE-3.5.md).
- `MockDriverProvider` now reports JSON parse errors instead of silently returning
  "not found".

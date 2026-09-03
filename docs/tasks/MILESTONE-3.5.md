# Milestone 3.5 — Real WindowsUpdate & Mirror providers

**Goal:** replace the two chain stubs with working implementations.

Prereq: Milestone 3 accepted. These are additive — the chain, cache, downloader and
`LocalCacheProvider` already work. Read [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md)
("WindowsUpdateProvider") and [../DECISIONS.md](../DECISIONS.md) ADR-0007.

This milestone can be done **after** M4/M5/M6 if the local cache + mock cover near-term
needs — it is not on the critical path to an end-to-end dry run. Sequence it whenever a
real online driver source is actually required.

---

## Task 1 — `WindowsUpdateProvider` (COM `IUpdateSearcher`)

- [ ] Link/late-bind the Windows Update Agent COM API (`wuapi.h`; `CLSID_UpdateSession`,
      `IUpdateSearcher`, `IUpdateSearchResult`, `IUpdate`, `IUpdate5` for driver props).
- [ ] `CoInitializeEx` per call (or once, documented); wrap everything so a COM failure
      returns `found=false` with a reason — never throws out of `search()`.
- [ ] Online mode: `IUpdateSearcher::put_ServerSelection(ssWindowsUpdate)`.
- [ ] Offline mode: if a `wsusscn2.cab` path was passed (`--wsus-scan`, or
      `cache/wsusscn2.cab` if present), register it via
      `IUpdateServiceManager::AddScanPackageService` and search that service.
- [ ] Criteria string: `"IsInstalled=0 and Type='Driver'"`.
- [ ] For each returned `IUpdate`:
      - read `IUpdate5::get_DriverHardwareID` / `get_DriverClass` / `get_DriverVerDate` /
        `get_DriverVersion` (or the `WindowsDriverUpdateEntry` collection);
      - match against the device's hardware + compatible IDs; set `matchedVia`;
      - build a `DriverPackage`: `driverName` = update title, `version` from
        `DriverVerDate`/`DriverVersion`, `provider` = `get_DriverProvider`,
        `downloadUrl` from `IUpdate::get_DownloadContents` →
        `IUpdateDownloadContent::get_DownloadUrl`, `checksum` when the API exposes one,
        `packageType` = `InfCab`/`InfZip` per the URL.
      - WHQL-signed by definition — mark verified.
- [ ] Everything time-boxed (a WUA online search can hang); a hung search → `found=false`.

## Task 2 — `MirrorProvider`

- [ ] Config: `--mirror-url <baseUrl>` (http(s):// or file://) pointing at a root that
      contains `index.json` = `{ "<hwOrCompatId>": [ { DriverPackage-shaped object with a
      relative or absolute `path` }, ... ] }`.
- [ ] `search()` fetches `<baseUrl>/index.json` (cached briefly in memory), looks up the
      device's IDs, returns `DriverPackage`s with `downloadUrl` = `<baseUrl>/<path>`,
      `matchedVia` set. Network/parse failure → `found=false` with reason.
- [ ] A `checksum`/`checksumAlgo` in the index entry is passed through so
      `DriverDownloader` verifies it.
- [ ] Document the mirror `index.json` schema in [../DRIVER_PROVIDER.md](../DRIVER_PROVIDER.md).

## Task 3 — Tests

- [ ] `WindowsUpdateProvider`: gated integration test (`SHIFTECH_INTEGRATION_TESTS=ON`,
      VM/online) — search returns something on a machine with a known missing driver;
      offline test with a checked-in tiny `wsusscn2`-style package if feasible, else
      document the manual procedure.
- [ ] `MirrorProvider`: unit test against a local `QTcpServer` serving a fixture
      `index.json` + payload — hit, miss, malformed index, checksum passthrough.
- [ ] `scripts\test.bat` green (unit); integration documented.

## Exit criteria

- [ ] `provisioner drivers resolve` on a connected machine with a real missing driver
      returns a WindowsUpdate match and `--download` fetches it into the portable cache.
- [ ] `MirrorProvider` resolves against a local fixture mirror.
- [ ] Chain order `localcache,windowsupdate,mirror` demonstrably tries each.
- [ ] No stub reasons left in the default chain output.
- [ ] Change summary + test output. STOP for review.

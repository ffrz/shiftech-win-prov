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

## Exit criteria — DONE (2026-09-03)

- [x] `WindowsUpdateProvider` is real: COM `IUpdateSearcher` +
      `"IsInstalled=0 and Type='Driver'"`, driver properties read via `IDispatch`
      late-binding (MinGW's `wuapi.h` lacks `IWindowsDriverUpdate5`). Verified live on this
      machine — `drivers resolve --provider-order windowsupdate` connects to Windows
      Update, gets a driver-update result set, matches by `DriverHardwareID` (no match for
      this machine's 2 edge devices, which is correct).
- [x] `MirrorProvider` is real: fetches `<baseUrl>/index.json`, matches Hardware/Compatible
      IDs, passes checksums through. `test_mirrorprovider` — hit, miss, malformed index,
      unconfigured, `file://` base (5 cases, offline via `QTcpServer`).
- [x] Default chain output has **no stub reasons** — `windowsupdate` gives a real
      "no … matched" / "has no driver updates", `mirror` gives "no mirror configured"
      (a config state, not a stub).
- [x] Chain tries each in order (`test_providerchain` + live run).
- [x] 19 test suites green.

### Deviations / not done
- **Offline `wsusscn2.cab` scanning: not implemented.** Needs `IUpdateServiceManager`,
  absent from MinGW's headers. `--wsus-scan` is accepted but falls through to an online
  search. Documented in DRIVER_PROVIDER.md; add by hand-declaring the interface if a real
  air-gapped need appears.
- WU integration test is the live manual run above (a gated automated test would need a
  machine with a known-missing WHQL driver).
- `drivers scan` still takes only `--provider`/`--driver-index`; `drivers resolve` has the
  full option set. Documented rather than refactored.

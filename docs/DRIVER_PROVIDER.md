# DriverProvider contract, portable cache & DriverPack investigation

## Design constraints (ADR-0004)

- **Portable.** The whole app folder — exe + `cache/drivers/` — is copied to a USB flash
  drive and run from there. Every path is **relative to the executable**; nothing under
  `C:\` or a user profile. `metadata.json` / `index.json` hold **no absolute paths** so the
  tree survives moving between drive letters and machines.
- **Fastest source first.** Resolution is a **provider chain**:
  `LocalCache → (DriverPack, only if ADR-0007 approves) → WindowsUpdate → Mirror`.
  The first provider that returns `found == true` wins (subject to the security gate).
- **Never abort.** A provider that errors or finds nothing is skipped; the chain moves on;
  a device with no driver anywhere is logged and skipped.
- **Unverifiable ⇒ warn + skip** (ADR-0006) — no interactive prompt in V1.

## The interface (as implemented)

```cpp
namespace shiftech::core::drivers {

struct TargetSystem {
    enum class OsFamily { Win7, Win8, Win10, Win11 };
    enum class Arch { x86, x64 };
    OsFamily os = OsFamily::Win10;
    int build = 0;
    Arch arch = Arch::x64;
};

enum class PackageType { InfZip, InfCab, InfFolder, Unknown };
enum class MatchVia { HardwareId, CompatibleId, Unspecified };

struct DriverPackage {
    std::string driverName;
    std::string version;
    std::string provider;
    std::vector<std::string> supportedOs;   // "win7","win8","win10","win11"
    TargetSystem::Arch arch = TargetSystem::Arch::x64;
    std::string downloadUrl;                 // http(s):// or file://
    PackageType packageType = PackageType::Unknown;
    std::string checksum;                    // empty if none
    std::string checksumAlgo;                // "sha256" etc.
    MatchVia matchedVia = MatchVia::Unspecified;  // set by the provider
    std::string matchedId;                   // the device ID that matched
};

struct DriverSearchResult {
    bool found = false;
    std::string notFoundReason;
    std::vector<DriverPackage> candidates;   // provider order; pickBest ranks them
};

class DriverProvider {
public:
    virtual ~DriverProvider() = default;
    virtual DriverSearchResult search(const hardware::Device& device,
                                      const TargetSystem& target) = 0;
    virtual std::string name() const = 0;    // "localcache","windowsupdate","mirror","mock"
};

} // namespace
```

`pickBest()` (in `DriverMatch.h`) hard-filters on arch + OS, then ranks
**HardwareId hits above CompatibleId hits**, with version as the tie-break.

## Provider chain

| Priority | Provider | Network | Status |
|----------|----------|---------|--------|
| 1 | `LocalCacheProvider` | none | Milestone 3 |
| 2 | `WindowsUpdateProvider` | yes | **real (M3.5)** — COM `IUpdateSearcher`, `IsInstalled=0 and Type='Driver'`, matched by `DriverHardwareID` via IDispatch late-binding |
| 3 | `MirrorProvider` | yes | **real (M3.5)** — fetches `<baseUrl>/index.json`, matches by Hardware/Compatible ID |

`DriverPackProvider` was evaluated for slot 2 and **rejected** — see
[DECISIONS.md](DECISIONS.md) ADR-0007 (license forbids redistribution; no API). Do not
build it.

`mock` (`MockDriverProvider`) is **not** in the default chain — it's selected explicitly
for tests/dev via `--provider mock`.

## MockDriverProvider (implement first — Milestone 2)

- Backed by a JSON fixture: `tests/fixtures/driver_index.json` mapping
  `hardwareId → [DriverPackage]`.
- Deterministic, offline, no network. Used by unit tests and for end-to-end pipeline dev
  before a real provider exists.
- `downloadUrl` in fixtures points at small local files under `tests/fixtures/packages/`
  so `DriverDownloader` and `DriverInstaller` (dry-run) can be exercised.

## DriverPack — rejected (ADR-0007)

Investigation done 2026-09-03. **DriverPack is not used.** The License Agreement limits use
to "personal, informational, noncommercial purposes" and states "The Software may not be
redistributed" — bundling its drivers onto technician provisioning media is not permitted.
There is also no official API (only a Cloudflare-gated browse page). `DriverPackProvider`
will not be built; `WindowsUpdateProvider` takes slot 2.

## WindowsUpdateProvider (chain slot 2) — implemented

- COM `IUpdateSession` → `IUpdateSearcher`, criteria `"IsInstalled=0 and Type='Driver'"`,
  `ServerSelection = ssWindowsUpdate`, `Online = TRUE`.
- **MinGW note:** its `wuapi.h` only declares the base `IUpdate`/`IUpdateSearcher`/
  `IUpdateSession`/`ISearchResult` — not `IWindowsDriverUpdate5` / `IUpdateServiceManager`.
  So the driver-specific properties (`DriverHardwareID`, `DriverProvider`, `DriverVerDate`)
  and `DownloadContents[0].DownloadUrl` are read via **`IDispatch` late-binding**
  (`GetIDsOfNames` + `Invoke`), which is header-version independent. The WUA GUIDs are
  pulled in with `#include <initguid.h>` before `<wuapi.h>` (no `wuguid` import lib exists
  for MinGW).
- Each matching `IUpdate` → a `DriverPackage`: `driverName` = Title,
  `version` = `DriverVerDate` (sortable date string), `provider` = `DriverProvider`,
  `downloadUrl` = first `DownloadContents` URL, `packageType` = `.cab`→`InfCab` else
  `InfZip`. WHQL-signed by definition.
- Match by `DriverHardwareID` (case-insensitive, exact) vs the device's hardware then
  compatible IDs; `matchedVia` set accordingly.
- A COM/search failure → `found = false` with a reason; never throws.
- **Offline (`wsusscn2.cab`) scanning is NOT implemented** — it needs
  `IUpdateServiceManager`, absent from MinGW's headers. `--wsus-scan` / a
  `cache/wsusscn2.cab` are accepted by the CLI but currently fall through to an online
  search. A follow-up can add offline support by hand-declaring that interface if an
  air-gapped need appears.

## MirrorProvider (chain slot 3) — implemented

- `--mirror-url <baseUrl>` (http(s):// or file://) → fetches `<baseUrl>/index.json` once
  per process (in-memory cache), with a timeout.
- **`index.json` schema:**
  ```json
  {
    "PCI\\VEN_10EC&DEV_8168": [
      { "driverName": "Realtek NIC", "version": "2.0.0.0", "provider": "Realtek",
        "supportedOs": ["win10","win11"], "arch": "x64",
        "path": "realtek/rt_nic.zip",
        "packageType": "InfZip",
        "checksum": "<sha256 hex>", "checksumAlgo": "sha256" }
    ]
  }
  ```
  Keys are Hardware or Compatible IDs. `path` is relative to the mirror root (or an
  absolute URL). `downloadUrl` in the returned package = `<baseUrl>/<path>`; any
  `checksum` is passed through so `DriverDownloader` verifies it.
- Network / parse failure → `found = false` with a reason.

Note: `drivers scan` is the quick "what would resolve" view and takes only
`--provider` / `--driver-index`. Use **`drivers resolve`** for the full option set
(`--provider-order`, `--mirror-url`, `--wsus-scan`, `--cache-dir`, `--download`).

## Portable cache & DriverDownloader

- `packageId = sha256(provider + "|" + driverName + "|" + version + "|" + archStr + "|" + downloadUrl)`
  truncated to 16 hex chars. **Deterministic, path-independent, no separators.**
- Layout, all **relative to the exe**:
  ```
  cache/drivers/index.json              # {hardwareOrCompatId: [packageId, ...]}
  cache/drivers/<packageId>/package.<ext>
  cache/drivers/<packageId>/metadata.json   # DriverPackage fields + checksum + fetchedAt
  ```
  `metadata.json` must contain **no absolute paths**.
- `LocalCacheProvider` reads `index.json` (rebuildable by scanning the dir) and returns
  cached packages whose IDs match the device, **rewriting `downloadUrl` to a `file://`
  path inside the cache** and setting `matchedVia`. The `downloadUrl` stored in
  `metadata.json` is provenance only (where the package was originally fetched from) — it
  is never used to locate the payload, so a stale absolute path there does not break
  relocation. The payload is always found via `<packageId>/<payloadFileName>` relative to
  the cache root.
- `DriverDownloader`: before downloading, if the package dir exists and (checksum matches
  OR no checksum and size > 0) → reuse (`fromCache = true`). Otherwise download to
  `package.<ext>.part`, verify, atomic-rename, then update `index.json`.
- Retry 3×, backoff 1s/4s/9s, per-attempt timeout 120s (configurable). Resume via HTTP
  `Range` when the server advertises `Accept-Ranges: bytes`. `file://` sources supported.
- **Pre-population workflow:** a technician runs `provisioner drivers resolve --download`
  on a connected reference machine; the portable `cache/drivers/` fills up; the flash drive
  is then used offline on target machines (LocalCacheProvider serves everything).

## Security gate before install

```
trusted source? -> download -> checksum verify -> extract -> INF sanity check -> pnputil install
```

Any failed step ⇒ record a warning, mark the device `Skipped`, continue the pipeline.
INF sanity check: file parses, has a `[Version]` section, `Class`/`ClassGuid` present,
no obviously hostile directives. Never run a bundled `.exe`.

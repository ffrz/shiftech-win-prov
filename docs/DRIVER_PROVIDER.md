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
| 2 | `DriverPackProvider` | yes | **only if ADR-0007 approves** |
| 3 | `WindowsUpdateProvider` | yes | stub in M3, real later (COM `IUpdateSearcher`, driver category) |
| 4 | `MirrorProvider` | yes | stub in M3, real later (internal HTTP/share + JSON index) |

`mock` (`MockDriverProvider`) is **not** in the default chain — it's selected explicitly
for tests/dev via `--provider mock`.

## MockDriverProvider (implement first — Milestone 2)

- Backed by a JSON fixture: `tests/fixtures/driver_index.json` mapping
  `hardwareId → [DriverPackage]`.
- Deterministic, offline, no network. Used by unit tests and for end-to-end pipeline dev
  before a real provider exists.
- `downloadUrl` in fixtures points at small local files under `tests/fixtures/packages/`
  so `DriverDownloader` and `DriverInstaller` (dry-run) can be exercised.

## DriverPackProvider (BLOCKED — see ADR-0007)

**Do not write `DriverPackProvider` code, and do not hardcode any DriverPack URL/HTML
structure, until every box below is filled in and ADR-0007 is signed off.** If the
investigation fails, DriverPack is simply dropped from the chain (positions 1/3/4 remain).

### Investigation checklist

- [ ] Is there an **official public API** from DriverPack (driverpack.io / DriverPack
      Solution)? Document the base URL, auth, rate limits, response schema.
- [ ] If no API: what exactly is served? (a) per-Hardware-ID driver packs, (b) one giant
      offline pack, (c) an online "solution" client only. Record findings.
- [ ] Is **direct, stable download by Hardware ID** possible? Capture a real request/response
      for one known ID (e.g. a Realtek NIC).
- [ ] **Licensing / Terms of Service**: are automated download and redistribution to
      technician machines permitted? Quote the relevant clause. If unclear → stop, escalate.
- [ ] **Integrity**: does the source provide checksums / signatures? How is package
      authenticity established?
- [ ] **Stability**: how likely is the mechanism to break? Is scraping HTML required? If a
      scraper is unavoidable, is there a more reliable alternative (e.g. Windows Update
      driver catalog, vendor catalogs, a curated internal mirror)?
- [ ] **Alternatives evaluated**: Windows Update (`UpdateSearcher` / `IUpdateServiceManager`
      with the driver category), Microsoft Update Catalog, per-vendor driver catalogs,
      an internal driver share/mirror. Record pros/cons vs DriverPack.

### Output of the investigation

Write the findings into [DECISIONS.md](DECISIONS.md) as **ADR-0007** (sub-task of ADR-0004):
either "DriverPack is in the chain, mechanism = …" or "DriverPack dropped, reason = …".

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
  cached packages whose IDs match the device, with `downloadUrl` = a `file://` path inside
  the cache and `matchedVia` set.
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

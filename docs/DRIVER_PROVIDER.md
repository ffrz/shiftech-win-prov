# DriverProvider contract & DriverPack investigation

## The interface

```cpp
namespace shiftech::core::drivers {

struct TargetSystem {
    enum class OsFamily { Win7, Win8, Win10, Win11 };
    OsFamily os;
    int build = 0;
    enum class Arch { x86, x64 };
    Arch arch;
};

enum class PackageType { InfZip, InfCab, InfFolder, Unknown };

struct DriverPackage {
    std::string driverName;
    std::string version;
    std::string provider;
    std::vector<std::string> supportedOs;
    TargetSystem::Arch arch;
    std::string downloadUrl;
    PackageType packageType = PackageType::Unknown;
    std::string checksum;        // empty if none
    std::string checksumAlgo;    // "sha256" etc.
};

struct DriverSearchResult {
    bool found = false;
    std::string notFoundReason;      // when !found
    std::vector<DriverPackage> candidates;  // best first
};

class DriverProvider {
public:
    virtual ~DriverProvider() = default;
    virtual DriverSearchResult search(const Device& device,
                                      const TargetSystem& target) = 0;
    virtual std::string name() const = 0;   // "mock", "driverpack", ...
};

} // namespace
```

Given a Hardware ID like `PCI\VEN_10EC&DEV_8168`, a provider returns zero or more candidate
packages. The pipeline picks the best candidate, downloads, installs, verifies.

## MockDriverProvider (implement first — Milestone 2)

- Backed by a JSON fixture: `tests/fixtures/driver_index.json` mapping
  `hardwareId → [DriverPackage]`.
- Deterministic, offline, no network. Used by unit tests and for end-to-end pipeline dev
  before a real provider exists.
- `downloadUrl` in fixtures points at small local files under `tests/fixtures/packages/`
  so `DriverDownloader` and `DriverInstaller` (dry-run) can be exercised.

## DriverPackProvider (Milestone 3 — BLOCKED until this checklist is done)

**Do not write `DriverPackProvider` code, and do not hardcode any DriverPack URL/HTML
structure, until every box below is filled in and reviewed.**

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

Write the findings into [DECISIONS.md](DECISIONS.md) as ADR-0004, then either:
- implement `DriverPackProvider` per the documented, reliable mechanism, **or**
- if no reliable/permitted mechanism exists: keep only `MockDriverProvider` + a documented
  integration point, and record a `WindowsUpdateProvider` / `MirrorProvider` as the
  recommended real path.

## DriverDownloader / cache

- `packageId = sha256(provider + "|" + driverName + "|" + version + "|" + arch + "|" + downloadUrl)[:16]`
- Layout: `cache/drivers/<packageId>/package.<ext>` + `metadata.json`.
- Before downloading: if `metadata.json` exists and (checksum matches OR no checksum
  available and file size > 0), reuse. Otherwise (re)download.
- Retry: 3 attempts, exponential backoff (1s, 4s, 9s), per-attempt timeout 120s (configurable).
- Resume via HTTP `Range` when the server advertises `Accept-Ranges: bytes`.
- Download to `package.<ext>.part`, verify, then atomic rename.

## Security gate before install

```
trusted source? -> download -> checksum verify -> extract -> INF sanity check -> pnputil install
```

Any failed step ⇒ record a warning, mark the device `Skipped`, continue the pipeline.
INF sanity check: file parses, has a `[Version]` section, `Class`/`ClassGuid` present,
no obviously hostile directives. Never run a bundled `.exe`.

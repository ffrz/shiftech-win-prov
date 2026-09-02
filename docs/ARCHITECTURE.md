# Architecture

Status: **proposal** for Milestone 1. Refine as modules are implemented; record changes in
[DECISIONS.md](DECISIONS.md).

## 1. Goals & constraints

- Standalone tool that runs on a technician's laptop. No server, no microservices.
- Core engine is a **UI-free library** (`shiftech_core`) reusable by CLI, GUI, automation,
  or a future Windows service.
- Minimal dependencies: C++20 standard library, Qt 6 (Core; Widgets only in the GUI target),
  Windows SDK. YAML/JSON handling: prefer Qt's JSON; for YAML profiles see §7.
- Targets: Windows 7 x86, Windows 7/8/10/11 x64. No ARM.
- Resilient: one driver/app failure never stops the run.

## 2. Component overview

```
                +---------------------+       +---------------------+
                |     src/cli         |       |     src/gui         |
                | provisioner.exe     |       | Qt Widgets frontend |
                | (arg parse, output) |       | (events -> widgets) |
                +----------+----------+       +----------+----------+
                           |                             |
                           |     both depend only on     |
                           v         the core API        v
                +--------------------------------------------------+
                |                shiftech_core (lib)               |
                |                                                  |
                |  provisioning/  ProvisioningEngine               |
                |                 Pipeline + StateMachine          |
                |                 ProvisioningEvent (signals)      |
                |                                                  |
                |  hardware/      DeviceEnumerator -> Device       |
                |  drivers/       DriverProvider (iface)           |
                |                 MockDriverProvider               |
                |                 DriverPackProvider (later)       |
                |                 DriverDownloader                 |
                |                 DriverInstaller (pnputil)        |
                |                 DriverVerifier                   |
                |  applications/  ApplicationProvider (iface)      |
                |                 WinGetProvider                   |
                |  profiles/      ProfileLoader -> Profile         |
                |  logging/       StructuredLogger (JSON)          |
                |  system/        SystemInspector -> SystemInfo    |
                +------------------------+-------------------------+
                                         |
                        Windows APIs / external tools
             SetupAPI, CfgMgr32, Version, sysinfo, WinHTTP/Qt Network,
                          pnputil.exe, winget.exe
```

## 3. Module responsibilities

### system/ — `SystemInspector`
Produces `SystemInfo`: Windows version + build, edition, architecture (x86/x64),
firmware mode (UEFI/BIOS), running elevated?, internet connectivity, `winget` present?,
`pnputil` present?. Pure query, no mutation. Used by the pipeline's System Check stage
and by CLI `scan` header output. Missing requirements ⇒ clear, actionable error strings.

### hardware/ — `DeviceEnumerator`, `Device`
Enumerate all present devices via **SetupAPI + CfgMgr32** (see
[WINDOWS_APIS.md](WINDOWS_APIS.md)). Fill the `Device` model. Classify each device's
"needs driver" state from its config-manager status/problem code. No command-output parsing.

```cpp
namespace shiftech::core::hardware {

enum class DeviceStatus {
    Ok,              // working, driver present
    NoDriver,        // no driver bound
    Problem,         // has a CM problem code
    Unknown,         // unknown device
    Disabled,
};

struct Device {
    std::string name;               // friendly name or "Unknown Device"
    std::string className;           // device class (e.g. "Net", "Display")
    std::string classGuid;
    std::string manufacturer;
    std::vector<std::string> hardwareIds;
    std::vector<std::string> compatibleIds;
    std::string instanceId;

    DeviceStatus status = DeviceStatus::Unknown;
    int problemCode = 0;             // CM_PROB_* ; 0 == none

    std::string driverVersion;
    std::string driverProvider;
    std::string driverDate;          // ISO-8601 if available

    bool needsDriver() const;        // NoDriver || Problem(28/1/...) || Unknown
};

class DeviceEnumerator {
public:
    std::vector<Device> enumerate();          // all present devices
    std::vector<Device> enumerateNeedingDriver();
};

} // namespace
```

### drivers/
- **`DriverProvider`** (interface): `DriverSearchResult search(const Device&, const TargetSystem&)`.
  `TargetSystem` = OS family + build + architecture. Result carries: driver name, version,
  provider, supported OS list, architecture, download URL, package type, checksum (optional),
  and a `found` flag / `notFoundReason`.
- **`MockDriverProvider`**: fixture-backed, for tests and Milestone 2. Deterministic.
- **`DriverPackProvider`**: real implementation — **blocked** until
  [DRIVER_PROVIDER.md](DRIVER_PROVIDER.md) investigation is complete.
- **`DriverDownloader`**: download to a temp file → verify checksum → move into
  `cache/drivers/<package-id>/`. Progress callback, retry with backoff, timeout, resume
  if the server supports ranges, skip if already cached. `<package-id>` is a deterministic
  hash of `{provider, driver name, version, architecture, url}`.
- **`DriverInstaller`**: extract package → find all `*.inf` → `pnputil /add-driver <inf>
  /install` per INF → capture stdout/stderr/exit code, per-INF timeout, aggregate result.
  Never blind-runs an unknown `.exe`.
- **`DriverVerifier`**: re-run `DeviceEnumerator`, diff before/after per device, produce
  `DriverInstallStatus` (`AlreadyInstalled`, `Installed`, `Failed`, `NotFound`, `Skipped`,
  `RequiresReboot`).

### applications/ — `ApplicationProvider`, `WinGetProvider`
`isInstalled(id)`, `install(id, {silent, timeout})`. `WinGetProvider` shells `winget` with
`--silent --accept-package-agreements --accept-source-agreements --disable-interactivity`.
Captures output + exit code, retries when appropriate, continues past failures.

### profiles/ — `ProfileLoader`, `Profile`
Load `profiles/<name>.yaml` (or `.json`). Validate: unique ids, known keys, `required` bool.
`Profile { name, description, vector<AppEntry{ id, required }> }`. Pure parsing — unit-tested
with fixture files, no OS calls.

### provisioning/ — `ProvisioningEngine`, pipeline, state machine, events
Orchestrates the pipeline (§5). Emits `ProvisioningEvent` for every meaningful step.
Holds a serializable `ProvisioningState` (current stage + per-item results) so a future
version can resume after reboot. Business logic lives here, **not** in CLI/GUI classes.

```cpp
struct ProvisioningEvent {
    std::string timestamp;   // ISO-8601
    std::string category;    // "system" | "hardware" | "driver" | "application" | "report"
    std::string severity;    // "info" | "success" | "warning" | "error"
    std::string message;
    int progress = -1;       // 0..100 for the current stage, -1 = n/a
};
```

CLI subscribes and prints; GUI subscribes and updates widgets. In the Qt build this is a
`QObject` with a `void event(const ProvisioningEvent&)` signal; a non-Qt consumer can use a
`std::function` sink.

### logging/ — `StructuredLogger`
Writes one JSON file per run: `logs/<YYYY-MM-DD_HHMMSS>.json` containing system info, every
stage's events, per-device and per-app results, durations, and the final report object.
Never writes credentials/PII. Also mirrors events to the console at a chosen level.

## 4. State machine

```
Init -> SystemCheck -> HardwareScan -> DriverAnalysis -> DriverResolution
     -> DriverDownload -> DriverInstall -> DriverVerify
     -> AppDetection -> AppInstall -> FinalVerify -> Report -> Done
                                                            \-> Failed (only on fatal env error)
```

Only **fatal environment errors** (not elevated, no `pnputil`, unsupported OS) move to
`Failed`. Per-item driver/app failures stay within their stage and are recorded.
Each transition is persisted to `ProvisioningState` for future resume.

## 5. Pipeline stages

| Stage | Module | Fatal-on-fail? |
|-------|--------|----------------|
| System Check | `SystemInspector` | yes (unmet hard requirement) |
| Hardware Scan | `DeviceEnumerator` | yes (cannot enumerate at all) |
| Driver Analysis | hardware classification | no |
| Driver Resolution | `DriverProvider` | no (per device) |
| Driver Download | `DriverDownloader` | no (per package) |
| Driver Installation | `DriverInstaller` | no (per package) |
| Driver Verification | `DriverVerifier` | no |
| Application Detection | `ApplicationProvider` | no |
| Application Installation | `ApplicationProvider` | no (per app) |
| Final Verification | verifier + report build | no |
| Report | `StructuredLogger` + report builder | no |

## 6. Data & directory model

```
cache/drivers/<package-id>/package.zip
cache/drivers/<package-id>/metadata.json   # provider result + checksum + fetch time
logs/<YYYY-MM-DD_HHMMSS>.json
profiles/<name>.yaml
```

## 7. Dependency notes

- **JSON**: `QJsonDocument` (Qt Core). No extra dependency.
- **YAML profiles**: decision pending — options: (a) ship profiles as `.json` and skip YAML,
  (b) vendor a single-header YAML parser, (c) minimal hand-rolled subset parser. Until an ADR
  is filed, `ProfileLoader` should accept **`.json`** and the shipped `profiles/*.yaml` are
  provided as human-readable references with `.json` equivalents. See
  [DECISIONS.md](DECISIONS.md) ADR-0002.
- **HTTP**: `QNetworkAccessManager` (Qt Network) for `DriverDownloader`. Alternative: WinHTTP.
- **Archive extraction**: prefer the Windows built-in (`tar.exe` on Win10+, or Shell
  `CopyHere` for zip); if broader support is needed, file an ADR before vendoring a lib.

## 8. Build targets

| Target | Type | Depends on | Qt modules |
|--------|------|-----------|------------|
| `shiftech_core` | static lib | Windows SDK | Core, Network |
| `provisioner` | console exe | `shiftech_core` | Core |
| `shiftech_gui` | GUI exe | `shiftech_core` | Core, Widgets |
| `shiftech_tests` | console exe (ctest) | `shiftech_core` | Core, Test |

GUI and tests are opt-in via CMake options (`SHIFTECH_BUILD_GUI`, `SHIFTECH_BUILD_TESTS`).

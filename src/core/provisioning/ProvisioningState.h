#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class QJsonObject;

namespace shiftech::core::provisioning {

enum class Stage {
    Init,
    SystemCheck,
    HardwareScan,
    DriverAnalysis,
    DriverResolution,
    DriverDownload,
    DriverInstall,
    DriverVerify,
    AppDetection,
    AppInstall,
    FinalVerify,
    Report,
    Done,
    Failed,
};

const char* toString(Stage s);
bool isLegalTransition(Stage from, Stage to);

struct DriverItemResult {
    std::string instanceId;
    std::string deviceName;
    std::string status;      // DriverInstallStatus string
    std::string detail;
};

struct AppItemResult {
    std::string id;
    bool required = false;
    std::string status;      // installed | already_installed | failed | skipped | would_install
    int exitCode = 0;
};

// Serializable snapshot of a provisioning run. Persisted after every stage transition so a
// future version can resume; V1 only reads it back for `report`.
struct ProvisioningState {
    std::string runId;                 // YYYY-MM-DD_HHMMSS
    Stage stage = Stage::Init;
    bool dryRun = false;
    bool rebootRequired = false;
    std::string fatalError;            // set only when stage == Failed

    // system
    std::string osName;
    int osBuild = 0;
    std::string arch;
    bool elevated = false;

    // hardware
    int devicesDetected = 0;
    int devicesNeedingDriver = 0;

    // driver / app per-item
    std::vector<DriverItemResult> drivers;
    std::vector<AppItemResult> apps;

    // timings (epoch ms)
    int64_t startedAtMs = 0;
    int64_t finishedAtMs = 0;

    // transitions log
    std::vector<std::string> stageHistory;

    // Applies a transition, throwing std::logic_error on an illegal one.
    void transitionTo(Stage next);

    QJsonObject toJson() const;
    static ProvisioningState fromJson(const QJsonObject& o);
};

} // namespace shiftech::core::provisioning

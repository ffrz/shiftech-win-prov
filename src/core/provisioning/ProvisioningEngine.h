#pragma once

#include "ProvisioningEvent.h"
#include "ProvisioningState.h"
#include "ReportBuilder.h"
#include <QString>
#include <atomic>
#include <string>

namespace shiftech::core::provisioning {

struct ProvisioningOptions {
    std::string profile;              // app profile name/path ("" => skip apps)
    bool dryRun = false;
    bool skipDrivers = false;
    bool skipApps = false;
    std::string providerOrder;        // driver provider chain spec ("" => default)
    QString cacheDir;                 // "" => portable default
    QString profilesDir;              // "" => exe-relative default
    QString logDir;                   // "" => <exeDir>/logs
    QString mockDriverIndex;          // for --provider-order mock
    std::string mirrorUrl;

    // Cooperative cancellation. When set and observed true between items, the engine
    // stops starting new work, finalizes the report with what completed, and returns.
    // Owned by the caller; may be null.
    const std::atomic<bool>* cancelRequested = nullptr;
};

struct ProvisioningResult {
    ProvisioningState state;
    Report report;
    QString runDir;                   // where the log was written
};

// Orchestrates the full pipeline. All business logic lives here; CLI/GUI only subscribe to
// events and render the result. One per-item failure never aborts the run — only a fatal
// environment error moves to Stage::Failed.
class ProvisioningEngine {
public:
    explicit ProvisioningEngine(EventSink sink = {});

    ProvisioningResult run(const ProvisioningOptions& opts);

private:
    EventSink m_sink;
    void emitEvent(const std::string& category, Severity sev, const std::string& msg,
                   int progress = -1);
};

} // namespace shiftech::core::provisioning

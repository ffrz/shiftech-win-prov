#pragma once

#include "ProvisioningEvent.h"
#include <QString>
#include <string>
#include <vector>

namespace shiftech::core::provisioning {

struct ResetOptions {
    QString runIdOrPath;      // "" => latest run in logDir
    QString logDir;           // "" => <exeDir>/logs
    QString cacheDir;         // "" => portable default (only for --purge-cache)
    bool dryRun = false;
    bool skipApps = false;
    bool skipDrivers = false;
    bool skipConfig = false;
    bool purgeCache = false;  // also empty cache/drivers/
};

struct ResetItem {
    std::string category;     // "app" | "driver" | "config" | "cache"
    std::string name;
    std::string outcome;      // "reverted" | "removed" | "not-supported" | "skipped" | "failed" | "would-…"
    std::string detail;
};

struct ResetResult {
    std::string runId;
    bool dryRun = false;
    std::vector<ResetItem> items;
    int reverted = 0, removed = 0, notSupported = 0, skipped = 0, failed = 0;
    std::string fatalError;   // set only on a run-not-found / bad-log error
};

// Undo a provisioning run, reading what to undo from its run.json:
//   - apps the run *installed* (status "installed") -> winget uninstall / local uninstaller
//   - drivers the run installed -> pnputil /delete-driver <oemNN.inf> /uninstall
//   - config tweaks the run *applied* -> tweak.revert()
// Items the run only detected as already-present / already-installed are left alone.
// Elevation is required for driver + machine-wide config reverts; without it those are
// reported "skipped".
class ResetEngine {
public:
    explicit ResetEngine(EventSink sink = {});
    ResetResult run(const ResetOptions& opts);

private:
    EventSink m_sink;
    void emitEvent(const std::string& cat, Severity sev, const std::string& msg);
};

} // namespace shiftech::core::provisioning

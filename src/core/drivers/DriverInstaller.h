#pragma once

#include <string>
#include <vector>

namespace shiftech::core::drivers {

struct InfInstallOutcome {
    std::string infPath;
    int exitCode = -1;
    bool ok = false;
    bool rebootRequired = false;
    std::string publishedName;   // oemNN.inf, when pnputil reports it
    std::string log;             // captured pnputil output (trimmed)
};

struct InstallReport {
    bool ranElevated = false;
    bool anyInstalled = false;
    bool anyFailed = false;
    bool rebootRequired = false;
    std::vector<InfInstallOutcome> perInf;
    std::string fatalError;       // set when nothing could run (e.g. not elevated)
};

struct InstallerOptions {
    int perInfTimeoutMs = 180000;
    bool dryRun = false;          // plan only; do not invoke pnputil
};

// Installs one or more INF files via `pnputil /add-driver "<inf>" /install`.
// - Requires elevation; without it, returns fatalError and installs nothing.
// - One INF failure is recorded and the batch continues.
// - Never executes a bundled .exe.
class DriverInstaller {
public:
    explicit DriverInstaller(InstallerOptions opts = {});

    InstallReport installInfs(const std::vector<std::string>& infPaths);

private:
    InstallerOptions m_opts;
};

} // namespace shiftech::core::drivers

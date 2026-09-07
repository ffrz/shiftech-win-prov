#pragma once

#include "ApplicationProvider.h"
#include <QStringList>

namespace shiftech::core::applications {

struct WinGetBootstrapResult {
    bool ok = false;
    bool alreadyPresent = false;
    std::string detail;
};

class WinGetProvider : public ApplicationProvider {
public:
    WinGetProvider();

    std::string name() const override { return "winget"; }
    bool isInstalled(const std::string& id) override;
    InstallResult install(const std::string& id, const InstallOptions& options = {}) override;

    bool isAvailable() const { return m_available; }

    // Try to make winget available on a machine that lacks it (fresh Windows 10):
    // installs the bundled App Installer package(s) from `bundleDir` (every *.msix,
    // *.msixbundle, *.appx, *.appxbundle in that folder) via Add-AppxPackage, then
    // re-checks. `bundleDir` empty => <exeDir>/tools/winget (with repo-relative fallbacks).
    // Re-checks availability and updates isAvailable().
    WinGetBootstrapResult bootstrap(const QString& bundleDir = QString());

private:
    bool m_available = false;
    bool checkAvailability();

    struct ProcessResult {
        int exitCode = -1;
        std::string stdOut;
        std::string stdErr;
        bool timedOut = false;
    };

    ProcessResult runWinGet(const QStringList& args, int timeoutMs = 600000);
};

} // namespace shiftech::core::applications

#pragma once

#include "ApplicationProvider.h"
#include <QStringList>

namespace shiftech::core::applications {

class WinGetProvider : public ApplicationProvider {
public:
    WinGetProvider();

    std::string name() const override { return "winget"; }
    bool isInstalled(const std::string& id) override;
    InstallResult install(const std::string& id, const InstallOptions& options = {}) override;

    bool isAvailable() const { return m_available; }

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

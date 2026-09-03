#include "WinGetOutput.h"
#include <algorithm>
#include <array>
#include <cctype>

namespace shiftech::core::applications::winget {

namespace {
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}

bool listOutputSaysInstalled(int exitCode, const std::string& stdOut) {
    const std::string low = toLower(stdOut);
    if (low.find("no installed package found") != std::string::npos) {
        return false;
    }
    if (low.find("no package found matching") != std::string::npos) {
        return false;
    }
    // A non-zero exit with no "not found" text is treated as "not installed"
    // (query failure), not as installed.
    if (exitCode != 0) {
        return false;
    }
    // Exit 0 and no "not found" marker => winget printed a result table.
    return true;
}

bool isTransientInstallFailure(int exitCode) {
    // winget error codes (APPINSTALLER_CLI_ERROR_*). Values are the signed
    // interpretation of the 0x8A15xxxx HRESULTs as returned via process exit code.
    // Transient: source/server/network problems.
    static const std::array<int, 6> transient = {
        static_cast<int>(0x8A150044), // INTERNAL_ERROR
        static_cast<int>(0x8A15004A), // SOURCE_DATA_MISSING (retry can refresh)
        static_cast<int>(0x8A150101), // INSTALL_DOWNLOAD_ERROR
        static_cast<int>(0x8A150102), // INSTALL_ABORTED (often transient)
        static_cast<int>(0x8A15010C), // INSTALL_SYSTEM_NOT_SUPPORTED? keep conservative
        static_cast<int>(0x8A150043), // SOURCE_NOT_REMOTE / server
    };
    for (int c : transient) {
        if (exitCode == c) return true;
    }
    return false;
}

} // namespace shiftech::core::applications::winget

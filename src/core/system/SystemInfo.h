#pragma once

#include <string>

namespace shiftech::core::system {

enum class Arch { X86, X64, Unsupported };

struct SystemInfo {
    std::string productName;
    std::string editionId;
    std::string displayVersion;
    int buildNumber = 0;
    Arch arch = Arch::Unsupported;
    bool elevated = false;
    bool wingetAvailable = false;
    bool pnputilAvailable = false;

    // TODO(Milestone 6): UEFI/BIOS mode
    // TODO(Milestone 6): internet connectivity
};

} // namespace shiftech::core::system

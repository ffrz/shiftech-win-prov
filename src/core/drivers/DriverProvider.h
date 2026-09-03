#pragma once

#include <string>
#include <vector>
#include "../hardware/Device.h"
#include "../system/SystemInfo.h"

namespace shiftech::core::drivers {

struct TargetSystem {
    enum class OsFamily { Win7, Win8, Win10, Win11 };
    OsFamily os;
    int build = 0;
    enum class Arch { x86, x64 };
    Arch arch;
};

TargetSystem currentTarget(const system::SystemInfo& info);

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
    virtual DriverSearchResult search(const hardware::Device& device,
                                      const TargetSystem& target) = 0;
    virtual std::string name() const = 0;   // "mock", "driverpack", ...
};

} // namespace shiftech::core::drivers

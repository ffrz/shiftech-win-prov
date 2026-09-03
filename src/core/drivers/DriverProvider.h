#pragma once

#include <string>
#include <vector>
#include "../hardware/Device.h"
#include "../system/SystemInfo.h"

namespace shiftech::core::drivers {

struct TargetSystem {
    enum class OsFamily { Win7, Win8, Win10, Win11 };
    enum class Arch { x86, x64 };

    OsFamily os = OsFamily::Win10;
    int build = 0;
    Arch arch = Arch::x64;
};

TargetSystem currentTarget(const system::SystemInfo& info);

enum class PackageType { InfZip, InfCab, InfFolder, Unknown };

// How a candidate package was matched against the device's IDs.
// HardwareId is a stronger (more specific) match than CompatibleId.
enum class MatchVia { HardwareId, CompatibleId, Unspecified };

struct DriverPackage {
    std::string driverName;
    std::string version;
    std::string provider;
    std::vector<std::string> supportedOs;
    TargetSystem::Arch arch = TargetSystem::Arch::x64;
    std::string downloadUrl;
    PackageType packageType = PackageType::Unknown;
    std::string checksum;        // empty if none
    std::string checksumAlgo;    // "sha256" etc.

    // Populated by the provider so pickBest can rank HardwareId hits above
    // CompatibleId hits. Providers that cannot tell leave it Unspecified.
    MatchVia matchedVia = MatchVia::Unspecified;
    std::string matchedId;       // the device ID string that produced the match
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

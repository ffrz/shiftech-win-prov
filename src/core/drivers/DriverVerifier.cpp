#include "DriverVerifier.h"

#include "../hardware/DeviceEnumerator.h"

#include <algorithm>
#include <set>

namespace shiftech::core::drivers {

using hardware::Device;
using hardware::DeviceStatus;

const char* toString(DriverInstallStatus s) {
    switch (s) {
        case DriverInstallStatus::AlreadyInstalled: return "AlreadyInstalled";
        case DriverInstallStatus::Installed: return "Installed";
        case DriverInstallStatus::Failed: return "Failed";
        case DriverInstallStatus::NotFound: return "NotFound";
        case DriverInstallStatus::Skipped: return "Skipped";
        case DriverInstallStatus::RequiresReboot: return "RequiresReboot";
    }
    return "Failed";
}

DriverInstallStatus classifyTransition(const Device& before,
                                       const std::optional<Device>& after,
                                       bool installerReboot) {
    if (!before.needsDriver()) {
        return DriverInstallStatus::AlreadyInstalled;
    }

    if (!after.has_value()) {
        // Device no longer present. Common when a bus/function device re-enumerates
        // under a new instance id after its driver binds; treat a reboot hint as the
        // reason, otherwise Failed (caller may re-scan later).
        return installerReboot ? DriverInstallStatus::RequiresReboot
                               : DriverInstallStatus::Failed;
    }

    const Device& a = *after;
    const bool nowOk = !a.needsDriver() && a.status == DeviceStatus::Ok;

    if (nowOk) {
        return installerReboot ? DriverInstallStatus::RequiresReboot
                               : DriverInstallStatus::Installed;
    }

    // Still needs a driver.
    if (installerReboot) return DriverInstallStatus::RequiresReboot;
    return DriverInstallStatus::Failed;
}

std::vector<VerifyResult> verifyAfterInstall(
    const std::vector<Device>& before, bool installerReboot,
    const std::vector<std::string>& notFoundInstanceIds,
    const std::vector<std::string>& skippedInstanceIds) {

    hardware::DeviceEnumerator enumerator;
    const std::vector<Device> after = enumerator.enumerate();

    auto findAfter = [&](const std::string& instanceId) -> std::optional<Device> {
        for (const auto& d : after) {
            if (d.instanceId == instanceId) return d;
        }
        return std::nullopt;
    };

    const std::set<std::string> notFound(notFoundInstanceIds.begin(), notFoundInstanceIds.end());
    const std::set<std::string> skipped(skippedInstanceIds.begin(), skippedInstanceIds.end());

    std::vector<VerifyResult> results;
    results.reserve(before.size());

    for (const auto& b : before) {
        VerifyResult r;
        r.instanceId = b.instanceId;
        r.deviceName = b.name;

        if (skipped.count(b.instanceId)) {
            r.status = DriverInstallStatus::Skipped;
            r.detail = "deliberately skipped";
        } else if (notFound.count(b.instanceId)) {
            r.status = DriverInstallStatus::NotFound;
            r.detail = "no driver found by any provider";
        } else {
            r.status = classifyTransition(b, findAfter(b.instanceId), installerReboot);
        }
        results.push_back(r);
    }
    return results;
}

} // namespace shiftech::core::drivers

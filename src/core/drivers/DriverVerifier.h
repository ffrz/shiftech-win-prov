#pragma once

#include "../hardware/Device.h"
#include <optional>
#include <string>
#include <vector>

namespace shiftech::core::drivers {

// Exactly the states from the initial spec.
enum class DriverInstallStatus {
    AlreadyInstalled,
    Installed,
    Failed,
    NotFound,
    Skipped,
    RequiresReboot,
};

const char* toString(DriverInstallStatus s);

struct VerifyResult {
    std::string instanceId;
    std::string deviceName;
    DriverInstallStatus status = DriverInstallStatus::Failed;
    std::string detail;
};

// Pure: given a device's before/after snapshot (matched by instance id) and whether the
// installer reported reboot-required, decide the outcome.
//   after == nullopt            -> device gone from the tree
//   before needsDriver, after OK -> Installed (or RequiresReboot if installerReboot)
//   before OK                    -> AlreadyInstalled
//   still needs a driver         -> Failed
DriverInstallStatus classifyTransition(const hardware::Device& before,
                                       const std::optional<hardware::Device>& after,
                                       bool installerReboot);

// Re-enumerate the live device tree and produce a VerifyResult per "before" device.
// `notInstalled` = instance ids we never attempted (resolution NOT FOUND) -> NotFound.
// `skipped`      = instance ids deliberately skipped (unverifiable pkg etc) -> Skipped.
std::vector<VerifyResult> verifyAfterInstall(
    const std::vector<hardware::Device>& before,
    bool installerReboot,
    const std::vector<std::string>& notFoundInstanceIds = {},
    const std::vector<std::string>& skippedInstanceIds = {});

} // namespace shiftech::core::drivers

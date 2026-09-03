#pragma once

#include <string>
#include <vector>

namespace shiftech::core::hardware {

enum class DeviceStatus {
    Ok,              // working, driver present
    NoDriver,        // no driver bound
    Problem,         // has a CM problem code
    Unknown,         // unknown device
    Disabled,
};

struct Device {
    std::string name;               // friendly name or "Unknown Device"
    std::string className;           // device class (e.g. "Net", "Display")
    std::string classGuid;
    std::string manufacturer;
    std::vector<std::string> hardwareIds;
    std::vector<std::string> compatibleIds;
    std::string instanceId;

    DeviceStatus status = DeviceStatus::Unknown;
    int problemCode = 0;             // CM_PROB_* ; 0 == none

    std::string driverVersion;
    std::string driverProvider;
    std::string driverDate;          // ISO-8601 if available

    bool needsDriver() const;        // NoDriver || Problem(28/1/...) || Unknown
};

} // namespace shiftech::core::hardware

#pragma once

#include "ProvisioningState.h"
#include <string>

class QJsonObject;

namespace shiftech::core::provisioning {

enum class RunStatus { Success, SuccessWithWarnings, Failed };
const char* toString(RunStatus s);

struct Report {
    // Hardware
    int devicesDetected = 0;
    int devicesNeedingDriver = 0;
    // Drivers
    int driversAlreadyInstalled = 0;
    int driversInstalled = 0;
    int driversNotFound = 0;
    int driversFailed = 0;
    int driversSkipped = 0;
    int driversRequireReboot = 0;
    // Applications
    int appsInstalled = 0;
    int appsAlreadyInstalled = 0;
    int appsFailed = 0;
    int appsFailedRequired = 0;
    int appsSkipped = 0;
    // Config tweaks
    int configApplied = 0;
    int configAlreadyApplied = 0;
    int configFailed = 0;
    int configSkipped = 0;
    // Overall
    bool rebootRequired = false;
    int64_t durationMs = 0;
    RunStatus status = RunStatus::Success;
    std::string fatalError;

    std::string toText() const;
    QJsonObject toJson() const;
};

Report buildReport(const ProvisioningState& state);

} // namespace shiftech::core::provisioning

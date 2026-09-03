#include "ReportBuilder.h"

#include <QJsonObject>
#include <sstream>

namespace shiftech::core::provisioning {

const char* toString(RunStatus s) {
    switch (s) {
        case RunStatus::Success: return "SUCCESS";
        case RunStatus::SuccessWithWarnings: return "SUCCESS WITH WARNINGS";
        case RunStatus::Failed: return "FAILED";
    }
    return "FAILED";
}

Report buildReport(const ProvisioningState& st) {
    Report r;
    r.devicesDetected = st.devicesDetected;
    r.devicesNeedingDriver = st.devicesNeedingDriver;
    r.fatalError = st.fatalError;
    r.rebootRequired = st.rebootRequired;

    for (const auto& d : st.drivers) {
        if (d.status == "AlreadyInstalled") ++r.driversAlreadyInstalled;
        else if (d.status == "Installed") ++r.driversInstalled;
        else if (d.status == "NotFound") ++r.driversNotFound;
        else if (d.status == "Failed") ++r.driversFailed;
        else if (d.status == "Skipped") ++r.driversSkipped;
        else if (d.status == "RequiresReboot") { ++r.driversRequireReboot; r.rebootRequired = true; }
    }
    for (const auto& a : st.apps) {
        if (a.status == "installed") ++r.appsInstalled;
        else if (a.status == "already_installed") ++r.appsAlreadyInstalled;
        else if (a.status == "failed") { ++r.appsFailed; if (a.required) ++r.appsFailedRequired; }
        else if (a.status == "skipped" || a.status == "skipped_no_winget") ++r.appsSkipped;
    }

    if (st.finishedAtMs > st.startedAtMs) r.durationMs = st.finishedAtMs - st.startedAtMs;

    if (st.stage == Stage::Failed || !st.fatalError.empty()) {
        r.status = RunStatus::Failed;
    } else if (r.driversNotFound || r.driversFailed || r.driversSkipped ||
               r.appsFailed || r.appsSkipped || r.rebootRequired) {
        r.status = RunStatus::SuccessWithWarnings;
    } else {
        r.status = RunStatus::Success;
    }
    return r;
}

std::string Report::toText() const {
    std::ostringstream o;
    const int64_t secs = durationMs / 1000;

    o << "Provisioning Complete\n\n";
    if (!fatalError.empty()) {
        o << "FATAL: " << fatalError << "\n\n";
    }
    o << "Hardware\n--------\n";
    o << "Devices detected: " << devicesDetected << "\n";
    o << "Devices requiring driver: " << devicesNeedingDriver << "\n\n";

    o << "Drivers\n-------\n";
    o << "Already installed: " << driversAlreadyInstalled << "\n";
    o << "Installed: " << driversInstalled << "\n";
    o << "Not found: " << driversNotFound << "\n";
    o << "Failed: " << driversFailed << "\n";
    o << "Skipped: " << driversSkipped << "\n";
    if (driversRequireReboot) o << "Require reboot: " << driversRequireReboot << "\n";
    o << "\n";

    o << "Applications\n------------\n";
    o << "Installed: " << appsInstalled << "\n";
    o << "Already installed: " << appsAlreadyInstalled << "\n";
    o << "Failed: " << appsFailed;
    if (appsFailedRequired) o << " (" << appsFailedRequired << " required)";
    o << "\n";
    o << "Skipped: " << appsSkipped << "\n\n";

    o << "Reboot required: " << (rebootRequired ? "YES" : "NO") << "\n\n";
    o << "Duration: " << (secs / 60) << "m " << (secs % 60) << "s\n\n";
    o << "Status: " << toString(status) << "\n";
    return o.str();
}

QJsonObject Report::toJson() const {
    QJsonObject h;
    h["devicesDetected"] = devicesDetected;
    h["devicesNeedingDriver"] = devicesNeedingDriver;

    QJsonObject d;
    d["alreadyInstalled"] = driversAlreadyInstalled;
    d["installed"] = driversInstalled;
    d["notFound"] = driversNotFound;
    d["failed"] = driversFailed;
    d["skipped"] = driversSkipped;
    d["requireReboot"] = driversRequireReboot;

    QJsonObject a;
    a["installed"] = appsInstalled;
    a["alreadyInstalled"] = appsAlreadyInstalled;
    a["failed"] = appsFailed;
    a["failedRequired"] = appsFailedRequired;
    a["skipped"] = appsSkipped;

    QJsonObject o;
    o["hardware"] = h;
    o["drivers"] = d;
    o["applications"] = a;
    o["rebootRequired"] = rebootRequired;
    o["durationMs"] = static_cast<double>(durationMs);
    o["status"] = toString(status);
    o["fatalError"] = QString::fromStdString(fatalError);
    return o;
}

} // namespace shiftech::core::provisioning

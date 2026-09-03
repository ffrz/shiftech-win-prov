#include "ProvisioningState.h"

#include <QJsonArray>
#include <QJsonObject>
#include <array>
#include <stdexcept>

namespace shiftech::core::provisioning {

namespace {
constexpr std::array<const char*, 14> kNames = {
    "Init",         "SystemCheck",  "HardwareScan", "DriverAnalysis", "DriverResolution",
    "DriverDownload","DriverInstall","DriverVerify", "AppDetection",   "AppInstall",
    "FinalVerify",  "Report",       "Done",         "Failed"};
}

const char* toString(Stage s) {
    const auto i = static_cast<size_t>(s);
    return i < kNames.size() ? kNames[i] : "Init";
}

static Stage stageFromString(const QString& s) {
    for (size_t i = 0; i < kNames.size(); ++i) {
        if (s == kNames[i]) return static_cast<Stage>(i);
    }
    return Stage::Init;
}

bool isLegalTransition(Stage from, Stage to) {
    // Any stage may fail.
    if (to == Stage::Failed) return true;
    // The happy path is strictly forward through the ordered enum, one step at a time,
    // BUT stages may be skipped forward (e.g. --skip-drivers jumps Resolution..Verify).
    // Never go backwards; never leave a terminal state.
    if (from == Stage::Done || from == Stage::Failed) return false;
    return static_cast<int>(to) > static_cast<int>(from);
}

void ProvisioningState::transitionTo(Stage next) {
    if (!isLegalTransition(stage, next)) {
        throw std::logic_error(std::string("illegal provisioning transition ") +
                               toString(stage) + " -> " + toString(next));
    }
    stage = next;
    stageHistory.emplace_back(toString(next));
}

QJsonObject ProvisioningState::toJson() const {
    QJsonObject o;
    o["runId"] = QString::fromStdString(runId);
    o["stage"] = toString(stage);
    o["dryRun"] = dryRun;
    o["rebootRequired"] = rebootRequired;
    o["fatalError"] = QString::fromStdString(fatalError);

    QJsonObject sys;
    sys["osName"] = QString::fromStdString(osName);
    sys["osBuild"] = osBuild;
    sys["arch"] = QString::fromStdString(arch);
    sys["elevated"] = elevated;
    o["system"] = sys;

    QJsonObject hw;
    hw["devicesDetected"] = devicesDetected;
    hw["devicesNeedingDriver"] = devicesNeedingDriver;
    o["hardware"] = hw;

    QJsonArray drv;
    for (const auto& d : drivers) {
        QJsonObject e;
        e["instanceId"] = QString::fromStdString(d.instanceId);
        e["deviceName"] = QString::fromStdString(d.deviceName);
        e["status"] = QString::fromStdString(d.status);
        e["detail"] = QString::fromStdString(d.detail);
        drv.append(e);
    }
    o["drivers"] = drv;

    QJsonArray ap;
    for (const auto& a : apps) {
        QJsonObject e;
        e["id"] = QString::fromStdString(a.id);
        e["required"] = a.required;
        e["status"] = QString::fromStdString(a.status);
        e["exitCode"] = a.exitCode;
        ap.append(e);
    }
    o["apps"] = ap;

    QJsonArray cfg;
    for (const auto& c : configTweaks) {
        QJsonObject e;
        e["id"] = QString::fromStdString(c.id);
        e["outcome"] = QString::fromStdString(c.outcome);
        e["detail"] = QString::fromStdString(c.detail);
        cfg.append(e);
    }
    o["configTweaks"] = cfg;

    o["startedAtMs"] = static_cast<double>(startedAtMs);
    o["finishedAtMs"] = static_cast<double>(finishedAtMs);

    QJsonArray hist;
    for (const auto& h : stageHistory) hist.append(QString::fromStdString(h));
    o["stageHistory"] = hist;

    return o;
}

ProvisioningState ProvisioningState::fromJson(const QJsonObject& o) {
    ProvisioningState s;
    s.runId = o["runId"].toString().toStdString();
    s.stage = stageFromString(o["stage"].toString());
    s.dryRun = o["dryRun"].toBool();
    s.rebootRequired = o["rebootRequired"].toBool();
    s.fatalError = o["fatalError"].toString().toStdString();

    const QJsonObject sys = o["system"].toObject();
    s.osName = sys["osName"].toString().toStdString();
    s.osBuild = sys["osBuild"].toInt();
    s.arch = sys["arch"].toString().toStdString();
    s.elevated = sys["elevated"].toBool();

    const QJsonObject hw = o["hardware"].toObject();
    s.devicesDetected = hw["devicesDetected"].toInt();
    s.devicesNeedingDriver = hw["devicesNeedingDriver"].toInt();

    for (const auto& v : o["drivers"].toArray()) {
        const QJsonObject e = v.toObject();
        s.drivers.push_back({e["instanceId"].toString().toStdString(),
                             e["deviceName"].toString().toStdString(),
                             e["status"].toString().toStdString(),
                             e["detail"].toString().toStdString()});
    }
    for (const auto& v : o["apps"].toArray()) {
        const QJsonObject e = v.toObject();
        AppItemResult a;
        a.id = e["id"].toString().toStdString();
        a.required = e["required"].toBool();
        a.status = e["status"].toString().toStdString();
        a.exitCode = e["exitCode"].toInt();
        s.apps.push_back(a);
    }
    for (const auto& v : o["configTweaks"].toArray()) {
        const QJsonObject e = v.toObject();
        s.configTweaks.push_back({e["id"].toString().toStdString(),
                                  e["outcome"].toString().toStdString(),
                                  e["detail"].toString().toStdString()});
    }

    s.startedAtMs = static_cast<int64_t>(o["startedAtMs"].toDouble());
    s.finishedAtMs = static_cast<int64_t>(o["finishedAtMs"].toDouble());
    for (const auto& v : o["stageHistory"].toArray()) {
        s.stageHistory.push_back(v.toString().toStdString());
    }
    return s;
}

} // namespace shiftech::core::provisioning

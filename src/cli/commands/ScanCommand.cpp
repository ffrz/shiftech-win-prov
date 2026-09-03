#include "ScanCommand.h"
#include "../../core/system/SystemInspector.h"
#include "../../core/hardware/DeviceEnumerator.h"
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

using namespace shiftech::core::system;
using namespace shiftech::core::hardware;

namespace shiftech::cli::commands {

namespace {

QString archToString(Arch arch) {
    switch (arch) {
        case Arch::X86: return "x86";
        case Arch::X64: return "x64";
        default: return "Unsupported";
    }
}

QString statusToString(DeviceStatus status) {
    switch (status) {
        case DeviceStatus::Ok: return "OK";
        case DeviceStatus::NoDriver: return "NO DRIVER";
        case DeviceStatus::Problem: return "PROBLEM";
        case DeviceStatus::Unknown: return "UNKNOWN";
        case DeviceStatus::Disabled: return "DISABLED";
        default: return "";
    }
}

QJsonObject deviceToJson(const Device& d) {
    QJsonObject obj;
    obj["name"] = QString::fromStdString(d.name);
    obj["className"] = QString::fromStdString(d.className);
    obj["classGuid"] = QString::fromStdString(d.classGuid);
    obj["manufacturer"] = QString::fromStdString(d.manufacturer);
    
    QJsonArray hwIds;
    for (const auto& hwId : d.hardwareIds) hwIds.append(QString::fromStdString(hwId));
    obj["hardwareIds"] = hwIds;

    QJsonArray compatIds;
    for (const auto& compId : d.compatibleIds) compatIds.append(QString::fromStdString(compId));
    obj["compatibleIds"] = compatIds;

    obj["instanceId"] = QString::fromStdString(d.instanceId);
    obj["status"] = statusToString(d.status);
    obj["problemCode"] = d.problemCode;
    obj["driverVersion"] = QString::fromStdString(d.driverVersion);
    obj["driverProvider"] = QString::fromStdString(d.driverProvider);
    obj["driverDate"] = QString::fromStdString(d.driverDate);
    return obj;
}

} // namespace

int ScanCommand::run(bool jsonOutput) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    SystemInfo sys = SystemInspector::inspect();
    DeviceEnumerator enumerator;
    std::vector<Device> allDevices = enumerator.enumerate();
    std::vector<Device> needingDriver;

    for (const auto& d : allDevices) {
        if (d.needsDriver()) needingDriver.push_back(d);
    }

    if (jsonOutput) {
        QJsonObject sysObj;
        sysObj["productName"] = QString::fromStdString(sys.productName);
        sysObj["editionId"] = QString::fromStdString(sys.editionId);
        sysObj["displayVersion"] = QString::fromStdString(sys.displayVersion);
        sysObj["buildNumber"] = sys.buildNumber;
        sysObj["arch"] = archToString(sys.arch);
        sysObj["elevated"] = sys.elevated;
        sysObj["wingetAvailable"] = sys.wingetAvailable;
        sysObj["pnputilAvailable"] = sys.pnputilAvailable;

        QJsonArray allDevArray;
        for (const auto& d : allDevices) allDevArray.append(deviceToJson(d));

        QJsonArray needingArray;
        for (const auto& d : needingDriver) needingArray.append(deviceToJson(d));

        QJsonObject root;
        root["system"] = sysObj;
        root["devices"] = allDevArray;
        root["needingDriver"] = needingArray;

        QJsonDocument doc(root);
        out << doc.toJson(QJsonDocument::Indented);
        out.flush();
    } else {
        out << "Shiftech Win Provisioner - scan\n\n";

        out << "System\n";
        out << "  " << QString::fromStdString(sys.productName) 
            << " (build " << sys.buildNumber << ")  "
            << archToString(sys.arch) << "   "
            << "Elevated: " << (sys.elevated ? "yes" : "no") << "\n";
        out << "  winget: " << (sys.wingetAvailable ? "yes" : "no")
            << "   pnputil: " << (sys.pnputilAvailable ? "yes" : "no") << "\n\n";

        if (!sys.elevated) {
            out << "Note: Some driver fields may be limited (not elevated).\n\n";
        }

        out << "Devices needing a driver (" << needingDriver.size() << ")\n";
        for (const auto& d : needingDriver) {
            QString firstId = d.hardwareIds.empty() ? "" : QString::fromStdString(d.hardwareIds.front());
            out << "  [" << statusToString(d.status) << "]  " 
                << firstId << "        " 
                << QString::fromStdString(d.name) << "        ";
            if (d.problemCode != 0) {
                out << "(problem " << d.problemCode << ")";
            }
            out << "\n";
        }
        out << "\n";

        out << "All devices (" << allDevices.size() << ")\n";
        for (const auto& d : allDevices) {
            out << "  [" << statusToString(d.status) << "]  " 
                << QString::fromStdString(d.className) << "  "
                << QString::fromStdString(d.name) << "  "
                << QString::fromStdString(d.driverVersion) << "  "
                << QString::fromStdString(d.driverProvider) << "  "
                << QString::fromStdString(d.driverDate) << "\n";
        }
        out.flush();
    }

    if (allDevices.empty()) {
        return 2; // fatal error
    }

    if (!needingDriver.empty()) {
        return 1;
    }

    return 0;
}

} // namespace shiftech::cli::commands

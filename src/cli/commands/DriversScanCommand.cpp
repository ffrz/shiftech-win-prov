#include "DriversScanCommand.h"
#include "../../core/system/SystemInspector.h"
#include "../../core/hardware/DeviceEnumerator.h"
#include "../../core/drivers/DriverProvider.h"
#include "../../core/drivers/DriverProviderFactory.h"
#include "../../core/drivers/ProviderChain.h"
#include "../../core/drivers/DriverMatch.h"
#include <QTextStream>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

using namespace shiftech::core::system;
using namespace shiftech::core::hardware;
using namespace shiftech::core::drivers;

namespace shiftech::cli::commands {

namespace {

QJsonObject packageToJson(const DriverPackage& pkg) {
    QJsonObject obj;
    obj["driverName"] = QString::fromStdString(pkg.driverName);
    obj["version"] = QString::fromStdString(pkg.version);
    obj["provider"] = QString::fromStdString(pkg.provider);
    QJsonArray osArr;
    for (const auto& os : pkg.supportedOs) osArr.append(QString::fromStdString(os));
    obj["supportedOs"] = osArr;
    obj["arch"] = pkg.arch == TargetSystem::Arch::x64 ? "x64" : "x86";
    obj["downloadUrl"] = QString::fromStdString(pkg.downloadUrl);
    const char* via = pkg.matchedVia == MatchVia::HardwareId ? "hardwareId"
                    : pkg.matchedVia == MatchVia::CompatibleId ? "compatibleId"
                    : "unspecified";
    obj["matchedVia"] = via;
    obj["matchedId"] = QString::fromStdString(pkg.matchedId);
    return obj;
}

QJsonObject deviceToJsonMinimal(const Device& d) {
    QJsonObject obj;
    obj["name"] = QString::fromStdString(d.name);
    obj["className"] = QString::fromStdString(d.className);
    QJsonArray hwIds;
    for (const auto& hwId : d.hardwareIds) hwIds.append(QString::fromStdString(hwId));
    obj["hardwareIds"] = hwIds;
    QJsonArray compatIds;
    for (const auto& compId : d.compatibleIds) compatIds.append(QString::fromStdString(compId));
    obj["compatibleIds"] = compatIds;
    return obj;
}

} // namespace

int DriversScanCommand::run(bool jsonOutput, const QString& providerName, const QString& indexFile) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    // Build the resolution path. Empty providerName => default provider chain.
    // "--provider mock" forces a single mock provider (needs a driver index).
    FactoryOptions fo;
    QString order;
    if (providerName == "mock") {
        QString idx = indexFile;
        if (idx.isEmpty()) {
            const QString exeDir = QCoreApplication::applicationDirPath();
            for (const QString& c : {exeDir + "/driver_index.json",
                                     exeDir + "/../tests/fixtures/driver_index.json",
                                     exeDir + "/../../tests/fixtures/driver_index.json"}) {
                if (QFileInfo::exists(c)) { idx = c; break; }
            }
            if (idx.isEmpty()) {
                out << "Error: no driver index found. Pass --driver-index <path>.\n";
                return 2;
            }
        }
        fo.mockIndexPath = idx;
        order = "mock";
    } else if (!providerName.isEmpty()) {
        order = providerName; // single named provider, e.g. "localcache"
        fo.mockIndexPath = indexFile;
    }

    std::string err;
    auto chainOpt = buildProviderChain(order.toStdString(), fo, err);
    if (!chainOpt) {
        out << "Error: " << QString::fromStdString(err) << "\n";
        return 2;
    }
    ProviderChain chain = std::move(*chainOpt);

    SystemInfo sys = SystemInspector::inspect();
    TargetSystem target = currentTarget(sys);

    DeviceEnumerator enumerator;
    std::vector<Device> needingDriver = enumerator.enumerateNeedingDriver();

    QJsonArray resultsArray;

    if (!jsonOutput) {
        out << "Shiftech Win Provisioner - drivers scan\n\n";
        out << "Chain: ";
        const auto names = chain.names();
        for (size_t i = 0; i < names.size(); ++i) {
            out << QString::fromStdString(names[i]);
            if (i + 1 < names.size()) out << " -> ";
        }
        out << "\nDevices needing a driver (" << needingDriver.size() << ")\n\n";
    }

    for (const auto& d : needingDriver) {
        DriverSearchResult result = chain.resolve(d, target);
        auto best = pickBest(d, result, target);

        if (jsonOutput) {
            QJsonObject resObj;
            resObj["device"] = deviceToJsonMinimal(d);
            
            QJsonArray cands;
            for (const auto& c : result.candidates) cands.append(packageToJson(c));
            resObj["candidates"] = cands;

            if (best) {
                resObj["match"] = packageToJson(*best);
                resObj["reason"] = "";
            } else {
                resObj["match"] = QJsonValue::Null;
                resObj["reason"] = QString::fromStdString(result.notFoundReason);
            }
            resultsArray.append(resObj);
        } else {
            out << "Device: " << QString::fromStdString(d.name) << "\n";
            QString firstId = d.hardwareIds.empty() ? "" : QString::fromStdString(d.hardwareIds.front());
            out << "  ID: " << firstId << "\n";
            
            if (best) {
                out << "  Match: " << QString::fromStdString(best->driverName) 
                    << " (v" << QString::fromStdString(best->version) << ") by "
                    << QString::fromStdString(best->provider) << "\n";
            } else {
                out << "  Match: NOT FOUND";
                if (!result.notFoundReason.empty()) {
                    out << " (" << QString::fromStdString(result.notFoundReason) << ")";
                }
                out << "\n";
            }
            out << "\n";
        }
    }

    if (jsonOutput) {
        QJsonObject root;
        QJsonObject targetObj;
        targetObj["osFamily"] = static_cast<int>(target.os);
        targetObj["build"] = target.build;
        targetObj["arch"] = target.arch == TargetSystem::Arch::x64 ? "x64" : "x86";
        
        root["target"] = targetObj;
        root["results"] = resultsArray;

        QJsonDocument doc(root);
        out << doc.toJson(QJsonDocument::Indented);
    }
    
    out.flush();
    return 0;
}

} // namespace shiftech::cli::commands

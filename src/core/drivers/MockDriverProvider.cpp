#include "MockDriverProvider.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <cctype>

namespace shiftech::core::drivers {

namespace {

PackageType parsePackageType(const QString& typeStr) {
    QString t = typeStr.toLower();
    if (t == "infzip") return PackageType::InfZip;
    if (t == "infcab") return PackageType::InfCab;
    if (t == "inffolder") return PackageType::InfFolder;
    return PackageType::Unknown;
}

TargetSystem::Arch parseArch(const QString& archStr) {
    QString a = archStr.toLower();
    if (a == "x86") return TargetSystem::Arch::x86;
    return TargetSystem::Arch::x64; // Default
}

std::vector<DriverPackage> parsePackages(const QJsonArray& arr) {
    std::vector<DriverPackage> pkgs;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();
        DriverPackage pkg;
        pkg.driverName = obj["driverName"].toString().toStdString();
        pkg.version = obj["version"].toString().toStdString();
        pkg.provider = obj["provider"].toString().toStdString();
        
        QJsonArray osArr = obj["supportedOs"].toArray();
        for (int j = 0; j < osArr.size(); ++j) {
            pkg.supportedOs.push_back(osArr[j].toString().toStdString());
        }
        
        pkg.arch = parseArch(obj["arch"].toString());
        pkg.downloadUrl = obj["downloadUrl"].toString().toStdString();
        pkg.packageType = parsePackageType(obj["packageType"].toString());
        pkg.checksum = obj["checksum"].toString().toStdString();
        pkg.checksumAlgo = obj["checksumAlgo"].toString().toStdString();
        
        pkgs.push_back(pkg);
    }
    return pkgs;
}

} // namespace

MockDriverProvider::MockDriverProvider(const std::string& indexFilePath)
    : indexPath(indexFilePath) {
}

DriverSearchResult MockDriverProvider::search(const hardware::Device& device, const TargetSystem& target) {
    (void)target;
    DriverSearchResult result;
    
    QFile file(QString::fromStdString(indexPath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.found = false;
        result.notFoundReason = "Mock index file not found";
        return result;
    }

    QByteArray val = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(val);
    if (!doc.isObject()) {
        result.found = false;
        result.notFoundReason = "Invalid JSON in mock index";
        return result;
    }

    QJsonObject root = doc.object();

    // Check HardwareIDs first (more specific match)
    for (const auto& hwid : device.hardwareIds) {
        QString key = QString::fromStdString(hwid);
        if (root.contains(key)) {
            std::vector<DriverPackage> pkgs = parsePackages(root[key].toArray());
            for (auto& p : pkgs) {
                p.matchedVia = MatchVia::HardwareId;
                p.matchedId = hwid;
            }
            result.candidates.insert(result.candidates.end(), pkgs.begin(), pkgs.end());
        }
    }

    // Then CompatibleIDs (weaker match)
    for (const auto& cid : device.compatibleIds) {
        QString key = QString::fromStdString(cid);
        if (root.contains(key)) {
            std::vector<DriverPackage> pkgs = parsePackages(root[key].toArray());
            for (auto& p : pkgs) {
                p.matchedVia = MatchVia::CompatibleId;
                p.matchedId = cid;
            }
            result.candidates.insert(result.candidates.end(), pkgs.begin(), pkgs.end());
        }
    }
    
    if (!result.candidates.empty()) {
        result.found = true;
    } else {
        result.found = false;
        result.notFoundReason = "No matching Hardware ID or Compatible ID found in mock index";
    }

    return result;
}

} // namespace shiftech::core::drivers

#include "DriverCache.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace shiftech::core::drivers {

namespace {

QString archStr(TargetSystem::Arch a) {
    return a == TargetSystem::Arch::x64 ? QStringLiteral("x64") : QStringLiteral("x86");
}

TargetSystem::Arch archFromStr(const QString& s) {
    return s.compare("x86", Qt::CaseInsensitive) == 0 ? TargetSystem::Arch::x86
                                                      : TargetSystem::Arch::x64;
}

const char* packageTypeStr(PackageType t) {
    switch (t) {
        case PackageType::InfZip: return "InfZip";
        case PackageType::InfCab: return "InfCab";
        case PackageType::InfFolder: return "InfFolder";
        case PackageType::Unknown: return "Unknown";
    }
    return "Unknown";
}

PackageType packageTypeFromStr(const QString& s) {
    if (s == "InfZip") return PackageType::InfZip;
    if (s == "InfCab") return PackageType::InfCab;
    if (s == "InfFolder") return PackageType::InfFolder;
    return PackageType::Unknown;
}

QJsonObject packageToJson(const DriverPackage& p) {
    QJsonObject o;
    o["driverName"] = QString::fromStdString(p.driverName);
    o["version"] = QString::fromStdString(p.version);
    o["provider"] = QString::fromStdString(p.provider);
    QJsonArray os;
    for (const auto& s : p.supportedOs) os.append(QString::fromStdString(s));
    o["supportedOs"] = os;
    o["arch"] = archStr(p.arch);
    o["downloadUrl"] = QString::fromStdString(p.downloadUrl);
    o["packageType"] = packageTypeStr(p.packageType);
    o["checksum"] = QString::fromStdString(p.checksum);
    o["checksumAlgo"] = QString::fromStdString(p.checksumAlgo);
    return o;
}

DriverPackage packageFromJson(const QJsonObject& o) {
    DriverPackage p;
    p.driverName = o["driverName"].toString().toStdString();
    p.version = o["version"].toString().toStdString();
    p.provider = o["provider"].toString().toStdString();
    for (const auto& v : o["supportedOs"].toArray()) p.supportedOs.push_back(v.toString().toStdString());
    p.arch = archFromStr(o["arch"].toString());
    p.downloadUrl = o["downloadUrl"].toString().toStdString();
    p.packageType = packageTypeFromStr(o["packageType"].toString());
    p.checksum = o["checksum"].toString().toStdString();
    p.checksumAlgo = o["checksumAlgo"].toString().toStdString();
    return p;
}

} // namespace

DriverCache::DriverCache(const QString& rootDir) {
    if (!rootDir.isEmpty()) {
        m_root = QDir(rootDir).absolutePath();
    } else {
        m_root = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("cache/drivers");
    }
}

std::string DriverCache::packageId(const DriverPackage& pkg) {
    const QString key = QString::fromStdString(pkg.provider) + "|" +
                        QString::fromStdString(pkg.driverName) + "|" +
                        QString::fromStdString(pkg.version) + "|" +
                        archStr(pkg.arch) + "|" +
                        QString::fromStdString(pkg.downloadUrl);
    const QByteArray h = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256);
    return h.toHex().left(16).toStdString();
}

QString DriverCache::packageDir(const std::string& id) const {
    return QDir(m_root).absoluteFilePath(QString::fromStdString(id));
}

QString DriverCache::packageDir(const DriverPackage& pkg) const {
    return packageDir(packageId(pkg));
}

QString DriverCache::indexPath() const {
    return QDir(m_root).absoluteFilePath("index.json");
}

std::optional<CachedPackage> DriverCache::read(const std::string& id) const {
    QFile f(QDir(packageDir(id)).absoluteFilePath("metadata.json"));
    if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return std::nullopt;
    const QJsonObject o = doc.object();

    CachedPackage e;
    e.package = packageFromJson(o["package"].toObject());
    for (const auto& v : o["deviceIds"].toArray()) e.deviceIds.push_back(v.toString().toStdString());
    e.payloadFileName = o["payloadFileName"].toString().toStdString();
    e.sha256 = o["sha256"].toString().toStdString();
    e.fetchedAt = o["fetchedAt"].toString().toStdString();
    return e;
}

bool DriverCache::isUsable(const DriverPackage& pkg) const {
    const auto entry = read(packageId(pkg));
    if (!entry) return false;
    if (entry->payloadFileName.empty()) return false;

    const QString payload =
        QDir(packageDir(pkg)).absoluteFilePath(QString::fromStdString(entry->payloadFileName));
    const QFileInfo fi(payload);
    if (!fi.exists() || fi.size() <= 0) return false;

    if (!pkg.checksum.empty()) {
        const QString want = QString::fromStdString(pkg.checksum).toLower();
        return sha256OfFile(payload).toLower() == want;
    }
    // No checksum available: a non-empty payload is accepted (ADR-0006 handles the
    // security consequences at install time).
    return true;
}

QString DriverCache::cachedPayloadUrl(const DriverPackage& pkg) const {
    if (!isUsable(pkg)) return {};
    const auto entry = read(packageId(pkg));
    const QString payload =
        QDir(packageDir(pkg)).absoluteFilePath(QString::fromStdString(entry->payloadFileName));
    return QUrl::fromLocalFile(payload).toString();
}

bool DriverCache::store(const CachedPackage& entry) const {
    const std::string id = packageId(entry.package);
    QDir().mkpath(packageDir(id));

    QJsonObject o;
    o["package"] = packageToJson(entry.package);
    QJsonArray ids;
    for (const auto& s : entry.deviceIds) ids.append(QString::fromStdString(s));
    o["deviceIds"] = ids;
    o["payloadFileName"] = QString::fromStdString(entry.payloadFileName);
    o["sha256"] = QString::fromStdString(entry.sha256);
    o["fetchedAt"] = entry.fetchedAt.empty()
                         ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
                         : QString::fromStdString(entry.fetchedAt);

    QFile f(QDir(packageDir(id)).absoluteFilePath("metadata.json"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    f.close();

    addToIndexInternal(id, entry.deviceIds);
    return true;
}

std::vector<std::string> DriverCache::lookup(const std::string& deviceId) const {
    QFile f(indexPath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const auto it = root.find(QString::fromStdString(deviceId));
    if (it == root.end()) return {};
    std::vector<std::string> out;
    for (const auto& v : it->toArray()) out.push_back(v.toString().toStdString());
    return out;
}

void DriverCache::addToIndex(const std::string& packageId,
                             const std::vector<std::string>& deviceIds) {
    addToIndexInternal(packageId, deviceIds);
}

void DriverCache::addToIndexInternal(const std::string& packageId,
                                     const std::vector<std::string>& deviceIds) const {
    QDir().mkpath(m_root);
    QJsonObject root;
    {
        QFile f(indexPath());
        if (f.open(QIODevice::ReadOnly)) root = QJsonDocument::fromJson(f.readAll()).object();
    }
    for (const auto& did : deviceIds) {
        const QString key = QString::fromStdString(did);
        QJsonArray arr = root.value(key).toArray();
        const QString pid = QString::fromStdString(packageId);
        bool present = false;
        for (const auto& v : arr) {
            if (v.toString() == pid) { present = true; break; }
        }
        if (!present) arr.append(pid);
        root[key] = arr;
    }
    QFile f(indexPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

int DriverCache::rebuildIndex() {
    QJsonObject root;
    int count = 0;
    const QDir dir(m_root);
    for (const QFileInfo& sub : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto entry = read(sub.fileName().toStdString());
        if (!entry) continue;
        ++count;
        const QString pid = sub.fileName();
        for (const auto& did : entry->deviceIds) {
            QJsonArray arr = root.value(QString::fromStdString(did)).toArray();
            arr.append(pid);
            root[QString::fromStdString(did)] = arr;
        }
    }
    QFile f(indexPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
    return count;
}

QString DriverCache::sha256OfFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return {};
    return QString::fromLatin1(h.result().toHex());
}

} // namespace shiftech::core::drivers

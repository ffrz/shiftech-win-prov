#include "LocalInstallerProvider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>

#include <algorithm>

namespace shiftech::core::applications {

namespace {

QString resolveAppsRoot(const QString& given) {
    if (!given.isEmpty()) return QDir(given).absolutePath();
    const QString exe = QCoreApplication::applicationDirPath();
    for (const QString& d : {exe + "/apps", exe + "/../apps", exe + "/../../apps"}) {
        if (QDir(d).exists()) return QDir(d).absolutePath();
    }
    return QDir(exe).absoluteFilePath("apps");
}

bool registryKeyExists(const QString& path) {
    // path like "HKLM\\SOFTWARE\\WinRAR"
    int slash = path.indexOf('\\');
    if (slash < 0) return false;
    const QString hive = path.left(slash);
    const QString sub = path.mid(slash + 1);
    QString full;
    if (hive.compare("HKLM", Qt::CaseInsensitive) == 0)
        full = "HKEY_LOCAL_MACHINE\\" + sub;
    else if (hive.compare("HKCU", Qt::CaseInsensitive) == 0)
        full = "HKEY_CURRENT_USER\\" + sub;
    else
        return false;
    QSettings s(full, QSettings::NativeFormat);
    return !s.childKeys().isEmpty() || !s.childGroups().isEmpty() || s.contains("Default");
}

bool arpHasDisplayName(const QString& substr) {
    const char* roots[] = {
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"};
    for (const char* r : roots) {
        QSettings s(r, QSettings::NativeFormat);
        for (const QString& g : s.childGroups()) {
            s.beginGroup(g);
            const QString dn = s.value("DisplayName").toString();
            s.endGroup();
            if (!dn.isEmpty() && dn.contains(substr, Qt::CaseInsensitive)) return true;
        }
    }
    return false;
}

} // namespace

std::optional<LocalAppManifest> loadLocalAppManifest(const QString& appFolder,
                                                     std::string& error) {
    const QString id = QFileInfo(appFolder).fileName();
    QFile f(QDir(appFolder).filePath("app.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        error = "no app.json in " + appFolder.toStdString();
        return std::nullopt;
    }
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        error = id.toStdString() + "/app.json: parse error";
        return std::nullopt;
    }
    const QJsonObject o = doc.object();

    LocalAppManifest m;
    m.id = id.toStdString();
    m.name = o.value("name").toString(id).toStdString();
    m.installerFile = o.value("installer").toString().toStdString();
    if (m.installerFile.empty()) {
        error = id.toStdString() + ": manifest has no 'installer'";
        return std::nullopt;
    }
    const QString ext = QFileInfo(QString::fromStdString(m.installerFile)).suffix().toLower();
    if (ext != "exe" && ext != "msi") {
        error = id.toStdString() + ": installer must be .exe or .msi";
        return std::nullopt;
    }
    if (!QFileInfo::exists(QDir(appFolder).filePath(QString::fromStdString(m.installerFile)))) {
        error = id.toStdString() + ": installer file not found: " + m.installerFile;
        return std::nullopt;
    }
    for (const auto& v : o.value("silentArgs").toArray())
        m.silentArgs.push_back(v.toString().toStdString());

    const QJsonObject d = o.value("detect").toObject();
    m.detectType = d.value("type").toString("arp").toStdString();
    for (const auto& v : d.value("keys").toArray())
        m.detectKeys.push_back(v.toString().toStdString());
    m.detectName = d.value("name").toString().toStdString();

    const QJsonArray ec = o.value("expectedExitCodes").toArray();
    if (ec.isEmpty()) m.expectedExitCodes = {0, 1641, 3010};
    else for (const auto& v : ec) m.expectedExitCodes.push_back(v.toInt());

    return m;
}

LocalInstallerProvider::LocalInstallerProvider(QString appsRoot)
    : m_root(resolveAppsRoot(appsRoot)) {}

QString LocalInstallerProvider::folderFor(const std::string& id) const {
    return QDir(m_root).filePath(QString::fromStdString(id));
}

std::vector<LocalAppManifest> LocalInstallerProvider::available() const {
    std::vector<LocalAppManifest> out;
    QDir dir(m_root);
    if (!dir.exists()) return out;
    for (const QFileInfo& fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        std::string err;
        if (auto m = loadLocalAppManifest(fi.absoluteFilePath(), err)) out.push_back(*m);
    }
    return out;
}

bool LocalInstallerProvider::isInstalled(const std::string& id) {
    std::string err;
    auto m = loadLocalAppManifest(folderFor(id), err);
    if (!m) return false;

    if (m->detectType == "registry") {
        for (const auto& k : m->detectKeys)
            if (registryKeyExists(QString::fromStdString(k))) return true;
        return false;
    }
    if (m->detectType == "file") {
        for (const auto& k : m->detectKeys)
            if (QFileInfo::exists(QString::fromStdString(k))) return true;
        return false;
    }
    // "arp"
    const QString needle = m->detectName.empty() ? QString::fromStdString(m->name)
                                                 : QString::fromStdString(m->detectName);
    return arpHasDisplayName(needle);
}

InstallResult LocalInstallerProvider::install(const std::string& id, const InstallOptions&) {
    InstallResult r;
    std::string err;
    auto m = loadLocalAppManifest(folderFor(id), err);
    if (!m) {
        r.ok = false;
        r.log = err;
        return r;
    }
    if (isInstalled(id)) {
        r.ok = true;
        r.alreadyInstalled = true;
        r.log = "already installed";
        return r;
    }

    const QString folder = folderFor(id);
    const QString installerPath =
        QDir(folder).filePath(QString::fromStdString(m->installerFile));
    const bool isMsi =
        QFileInfo(installerPath).suffix().compare("msi", Qt::CaseInsensitive) == 0;

    QProcess p;
    p.setWorkingDirectory(folder);
    p.setProcessChannelMode(QProcess::MergedChannels);
    if (isMsi) {
        p.setProgram("msiexec");
        QStringList args{"/i", QDir::toNativeSeparators(installerPath), "/qn", "/norestart"};
        for (const auto& a : m->silentArgs) args << QString::fromStdString(a);
        p.setArguments(args);
    } else {
        p.setProgram(installerPath);
        QStringList args;
        for (const auto& a : m->silentArgs) args << QString::fromStdString(a);
        p.setArguments(args);
    }

    p.start();
    if (!p.waitForStarted(15000)) {
        r.ok = false;
        r.log = "failed to start installer";
        return r;
    }
    if (!p.waitForFinished(900000)) { // 15 min
        p.kill();
        p.waitForFinished(3000);
        r.ok = false;
        r.log = "installer timed out";
        return r;
    }

    r.exitCode = p.exitCode();
    r.log = QString::fromLocal8Bit(p.readAll()).trimmed().toStdString();
    r.ok = std::find(m->expectedExitCodes.begin(), m->expectedExitCodes.end(),
                     r.exitCode) != m->expectedExitCodes.end();
    if (r.ok && (r.exitCode == 3010 || r.exitCode == 1641)) {
        r.log += "  (reboot required)";
    }
    return r;
}

} // namespace shiftech::core::applications

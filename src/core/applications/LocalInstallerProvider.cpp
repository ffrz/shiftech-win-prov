#include "LocalInstallerProvider.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

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

// Console tool -> {exitCode, combinedOutput, timedOut}
struct ProcOut {
    int code = -1;
    QString out;
    bool timedOut = false;
};
ProcOut runTool(const QString& program, const QStringList& args, const QString& workDir,
                int timeoutMs) {
    ProcOut r;
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    if (!workDir.isEmpty()) p.setWorkingDirectory(workDir);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();
    if (!p.waitForStarted(15000)) { r.out = "failed to start " + program; return r; }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(3000);
        r.timedOut = true;
        r.out = program + " timed out";
        return r;
    }
    r.code = p.exitCode();
    r.out = QString::fromLocal8Bit(p.readAll()).trimmed();
    return r;
}

bool registryKeyExists(const QString& path) {
    // Accept "HKLM\SOFTWARE\Foo" or "HKLM/SOFTWARE/Foo"; QSettings NativeFormat wants
    // the full "HKEY_..\..\.." form with BACKSLASHES.
    QString p = path;
    p.replace('/', '\\');
    const int bs = p.indexOf('\\');
    if (bs < 0) return false;
    const QString hive = p.left(bs);
    const QString sub = p.mid(bs + 1);
    QString full;
    if (hive.compare("HKLM", Qt::CaseInsensitive) == 0) full = "HKEY_LOCAL_MACHINE\\" + sub;
    else if (hive.compare("HKCU", Qt::CaseInsensitive) == 0) full = "HKEY_CURRENT_USER\\" + sub;
    else return false;
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

bool folderHasContent(const QString& path) {
    QDir d(path);
    if (!d.exists()) return false;
    return !d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}

// Find a 7-Zip CLI for .7z extraction. Order:
//   1. next to the provisioner exe:  7za.exe  (single-file, no dll) or 7z.exe (+7z.dll)
//   2. tools\7za.exe / tools\7z.exe
//   3. the app's own folder
//   4. a system 7-Zip install
//   5. "7z" on PATH
QString find7z(const QString& appFolder) {
    const QString exeDir = QCoreApplication::applicationDirPath();
    for (const QString& c : {exeDir + "/7za.exe", exeDir + "/7z.exe",
                             exeDir + "/tools/7za.exe", exeDir + "/tools/7z.exe",
                             appFolder + "/7za.exe", appFolder + "/7z.exe"}) {
        if (QFileInfo::exists(c)) return QDir::toNativeSeparators(c);
    }
    for (const QString& c : {QString("C:/Program Files/7-Zip/7z.exe"),
                             QString("C:/Program Files (x86)/7-Zip/7z.exe")}) {
        if (QFileInfo::exists(c)) return QDir::toNativeSeparators(c);
    }
    return "7z";
}

} // namespace

QString expandPath(const QString& raw) {
    QString s = raw;
    auto sub = [&](const char* token, const QString& value) {
        s.replace(QLatin1String(token), value, Qt::CaseInsensitive);
    };
    sub("%USERPROFILE%", QDir::homePath());
    sub("%DESKTOP%", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    sub("%PUBLIC%", qEnvironmentVariable("PUBLIC", "C:/Users/Public"));
    sub("%PUBLIC_DESKTOP%",
        qEnvironmentVariable("PUBLIC", "C:/Users/Public") + "/Desktop");
    sub("%PROGRAMDATA%", qEnvironmentVariable("ProgramData", "C:/ProgramData"));
    sub("%PROGRAMFILES%", qEnvironmentVariable("ProgramFiles", "C:/Program Files"));
    // generic %ENV%
    static const QRegularExpression re("%([A-Za-z_][A-Za-z0-9_]*)%");
    QRegularExpressionMatch m;
    while ((m = re.match(s)).hasMatch()) {
        const QString name = m.captured(1);
        const QString val = qEnvironmentVariable(name.toLocal8Bit().constData());
        s.replace(m.captured(0), val);
        if (val.isEmpty()) break; // avoid infinite loop on undefined var
    }
    return QDir::fromNativeSeparators(s);
}

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

    const QString kind = o.value("kind").toString("installer").toLower();
    if (kind == "portable") m.kind = LocalAppKind::Portable;
    else if (kind == "installer") m.kind = LocalAppKind::Installer;
    else {
        error = id.toStdString() + ": kind must be 'installer' or 'portable'";
        return std::nullopt;
    }

    const QJsonObject d = o.value("detect").toObject();
    m.detectType = d.value("type").toString(m.kind == LocalAppKind::Portable ? "folder"
                                                                            : "arp")
                       .toStdString();
    for (const auto& v : d.value("keys").toArray())
        m.detectKeys.push_back(v.toString().toStdString());
    m.detectName = d.value("name").toString().toStdString();

    if (m.kind == LocalAppKind::Installer) {
        m.installerFile = o.value("installer").toString().toStdString();
        if (m.installerFile.empty()) {
            error = id.toStdString() + ": manifest has no 'installer'";
            return std::nullopt;
        }
        const QString ext =
            QFileInfo(QString::fromStdString(m.installerFile)).suffix().toLower();
        if (ext != "exe" && ext != "msi") {
            error = id.toStdString() + ": installer must be .exe or .msi";
            return std::nullopt;
        }
        if (!QFileInfo::exists(
                QDir(appFolder).filePath(QString::fromStdString(m.installerFile)))) {
            error = id.toStdString() + ": installer file not found: " + m.installerFile;
            return std::nullopt;
        }
        for (const auto& v : o.value("silentArgs").toArray())
            m.silentArgs.push_back(v.toString().toStdString());
        const QJsonArray ec = o.value("expectedExitCodes").toArray();
        if (ec.isEmpty()) m.expectedExitCodes = {0, 1641, 3010};
        else for (const auto& v : ec) m.expectedExitCodes.push_back(v.toInt());
    } else {
        m.archiveFile = o.value("archive").toString().toStdString();
        if (m.archiveFile.empty()) {
            error = id.toStdString() + ": portable manifest has no 'archive'";
            return std::nullopt;
        }
        const QString ext =
            QFileInfo(QString::fromStdString(m.archiveFile)).suffix().toLower();
        if (ext != "zip" && ext != "7z") {
            error = id.toStdString() + ": archive must be .zip or .7z";
            return std::nullopt;
        }
        if (!QFileInfo::exists(
                QDir(appFolder).filePath(QString::fromStdString(m.archiveFile)))) {
            error = id.toStdString() + ": archive file not found: " + m.archiveFile;
            return std::nullopt;
        }
        m.extractTo = o.value("extractTo").toString().toStdString();
        if (m.extractTo.empty()) {
            error = id.toStdString() + ": portable manifest has no 'extractTo'";
            return std::nullopt;
        }
        m.flattenSingleRoot = o.value("flattenSingleRoot").toBool(false);
        m.shortcutExe = o.value("shortcutExe").toString().toStdString();
        m.shortcutName = o.value("shortcutName").toString(QString::fromStdString(m.name))
                             .toStdString();
    }

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

std::string LocalInstallerProvider::manifestError(const std::string& id) const {
    std::string err;
    if (loadLocalAppManifest(folderFor(id), err)) return {};
    return err;
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
            if (QFileInfo::exists(expandPath(QString::fromStdString(k)))) return true;
        return false;
    }
    if (m->detectType == "folder") {
        // default target: extractTo (for portable apps)
        if (m->detectKeys.empty() && !m->extractTo.empty())
            return folderHasContent(expandPath(QString::fromStdString(m->extractTo)));
        for (const auto& k : m->detectKeys)
            if (folderHasContent(expandPath(QString::fromStdString(k)))) return true;
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
        r.log = "already present";
        return r;
    }
    const QString folder = folderFor(id);
    return m->kind == LocalAppKind::Portable ? deployPortable(*m, folder)
                                             : runInstaller(*m, folder);
}

InstallResult LocalInstallerProvider::runInstaller(const LocalAppManifest& m,
                                                   const QString& folder) {
    InstallResult r;
    const QString installerPath =
        QDir(folder).filePath(QString::fromStdString(m.installerFile));
    const bool isMsi =
        QFileInfo(installerPath).suffix().compare("msi", Qt::CaseInsensitive) == 0;

    QStringList args;
    QString program;
    if (isMsi) {
        program = "msiexec";
        args << "/i" << QDir::toNativeSeparators(installerPath) << "/qn" << "/norestart";
        for (const auto& a : m.silentArgs) args << QString::fromStdString(a);
    } else {
        program = installerPath;
        for (const auto& a : m.silentArgs) args << QString::fromStdString(a);
    }

    ProcOut o = runTool(program, args, folder, 900000);
    r.exitCode = o.code;
    r.log = o.out.toStdString();
    if (o.timedOut) { r.ok = false; return r; }
    r.ok = std::find(m.expectedExitCodes.begin(), m.expectedExitCodes.end(), o.code) !=
           m.expectedExitCodes.end();
    if (r.ok && (o.code == 3010 || o.code == 1641)) r.log += "  (reboot required)";
    return r;
}

InstallResult LocalInstallerProvider::deployPortable(const LocalAppManifest& m,
                                                     const QString& folder) {
    InstallResult r;
    const QString archive =
        QDir(folder).filePath(QString::fromStdString(m.archiveFile));
    const QString dest = expandPath(QString::fromStdString(m.extractTo));
    QDir().mkpath(dest);

    const QString ext = QFileInfo(archive).suffix().toLower();
    ProcOut o;
    if (ext == "zip") {
        // bsdtar handles zip on Win10 1803+
        o = runTool("tar",
                    {"-xf", QDir::toNativeSeparators(archive), "-C",
                     QDir::toNativeSeparators(dest)},
                    {}, 300000);
        if (o.code != 0) {
            // fallback: PowerShell
            o = runTool("powershell",
                        {"-NoProfile", "-NonInteractive", "-Command",
                         QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                             .arg(archive, dest)},
                        {}, 300000);
        }
    } else { // 7z
        const QString sevenZip = find7z(folder);
        o = runTool(sevenZip,
                    {"x", "-y", QString("-o%1").arg(QDir::toNativeSeparators(dest)),
                     QDir::toNativeSeparators(archive)},
                    {}, 300000);
    }

    r.exitCode = o.code;
    r.log = o.out.left(400).toStdString();
    if (o.timedOut || o.code != 0) {
        r.ok = false;
        if (r.log.empty()) {
            r.log = ext == "7z"
                        ? "7z extraction failed - install 7-Zip or put 7za.exe next to "
                          "the exe / in the app folder"
                        : "extraction failed";
        }
        return r;
    }

    // If the archive was one wrapper folder, hoist its contents up into `dest`.
    if (m.flattenSingleRoot) {
        const QDir dd(dest);
        const auto entries = dd.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        if (entries.size() == 1 && QFileInfo(dd.filePath(entries.first())).isDir()) {
            const QString inner = dd.filePath(entries.first());
            QDirIterator it(inner, QDir::AllEntries | QDir::NoDotAndDotDot);
            while (it.hasNext()) {
                it.next();
                const QString to = dd.filePath(it.fileName());
                QDir().rename(it.filePath(), to);
            }
            QDir(inner).removeRecursively();
        }
    }

    // Optional Desktop shortcut.
    if (!m.shortcutExe.empty()) {
        const QString target =
            QDir(dest).filePath(QString::fromStdString(m.shortcutExe));
        const QString desktop =
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        const QString lnk =
            QDir(desktop).filePath(QString::fromStdString(m.shortcutName) + ".lnk");
        runTool("powershell",
                {"-NoProfile", "-NonInteractive", "-Command",
                 QString("$s=(New-Object -ComObject WScript.Shell).CreateShortcut('%1');"
                         "$s.TargetPath='%2';$s.WorkingDirectory='%3';$s.Save()")
                     .arg(lnk, QDir::toNativeSeparators(target),
                          QDir::toNativeSeparators(dest))},
                {}, 30000);
    }

    r.ok = true;
    r.log = "extracted to " + dest.toStdString() +
            (m.shortcutExe.empty() ? "" : "  (+ Desktop shortcut)");
    return r;
}

} // namespace shiftech::core::applications

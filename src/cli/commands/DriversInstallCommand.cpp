#include "DriversInstallCommand.h"

#include "../../core/drivers/DriverDownloader.h"
#include "../../core/drivers/DriverInstaller.h"
#include "../../core/drivers/DriverMatch.h"
#include "../../core/drivers/DriverProviderFactory.h"
#include "../../core/drivers/DriverVerifier.h"
#include "../../core/drivers/InfValidator.h"
#include "../../core/drivers/PackageExtractor.h"
#include "../../core/hardware/DeviceEnumerator.h"
#include "../../core/system/SystemInspector.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>

using namespace shiftech::core;
using namespace shiftech::core::drivers;
using namespace shiftech::core::hardware;

namespace shiftech::cli::commands {

namespace {

QString opt(const QStringList& a, const QString& n, const QString& d = {}) {
    const int i = a.indexOf(n);
    return (i >= 0 && i + 1 < a.size()) ? a[i + 1] : d;
}

std::vector<std::string> allIds(const Device& d) {
    std::vector<std::string> v = d.hardwareIds;
    v.insert(v.end(), d.compatibleIds.begin(), d.compatibleIds.end());
    return v;
}

} // namespace

int DriversInstallCommand::run(const QStringList& args) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#endif

    const bool json = args.contains("--json");
    const bool dryRun = args.contains("--dry-run");
    const QString only = opt(args, "--only");

    // Elevation gate for a real install.
    if (!dryRun && !system::SystemInspector::isElevated()) {
        out << "Error: 'drivers install' needs Administrator privileges. "
               "Re-run elevated, or use --dry-run.\n";
        return 2;
    }

    FactoryOptions fo;
    fo.cacheDir = opt(args, "--cache-dir");
    fo.mirrorUrl = opt(args, "--mirror-url").toStdString();
    fo.wsusScanPackage = opt(args, "--wsus-scan").toStdString();
    fo.mockIndexPath = opt(args, "--driver-index");
    std::string err;
    auto chainOpt = buildProviderChain(opt(args, "--provider-order").toStdString(), fo, err);
    if (!chainOpt) {
        out << "Error: " << QString::fromStdString(err) << "\n";
        return 2;
    }
    ProviderChain chain = std::move(*chainOpt);

    const system::SystemInfo sys = system::SystemInspector::inspect();
    const TargetSystem target = currentTarget(sys);

    DeviceEnumerator enumerator;
    std::vector<Device> before = enumerator.enumerateNeedingDriver();
    if (!only.isEmpty()) {
        before.erase(std::remove_if(before.begin(), before.end(),
                                    [&](const Device& d) {
                                        return QString::fromStdString(d.instanceId) != only;
                                    }),
                     before.end());
    }

    DriverCache cache(fo.cacheDir);
    DriverDownloader downloader(cache);
    DriverInstaller installer(InstallerOptions{180000, dryRun});

    std::vector<std::string> notFoundIds;
    std::vector<std::string> skippedIds;
    std::vector<std::string> infsToInstall;
    QJsonArray perDevice;

    if (!json) {
        out << "Shiftech Win Provisioner - drivers install"
            << (dryRun ? "  (DRY RUN)" : "") << "\n\n";
        out << "Devices needing a driver: " << before.size() << "\n\n";
    }

    for (const Device& d : before) {
        QJsonObject dj;
        dj["device"] = QString::fromStdString(d.name);
        dj["instanceId"] = QString::fromStdString(d.instanceId);

        DriverSearchResult sr = chain.resolve(d, target);
        auto best = pickBest(d, sr, target);
        if (!best) {
            notFoundIds.push_back(d.instanceId);
            dj["stage"] = "resolve";
            dj["result"] = "not_found";
            if (!json)
                out << "  [NOT FOUND] " << QString::fromStdString(d.name) << "\n";
            perDevice.append(dj);
            continue;
        }

        DownloadResult dr = downloader.fetch(*best, allIds(d));
        if (!dr.ok) {
            skippedIds.push_back(d.instanceId);
            dj["stage"] = "download";
            dj["result"] = "skipped";
            dj["reason"] = QString::fromStdString(dr.error);
            if (!json)
                out << "  [SKIP DL]   " << QString::fromStdString(d.name) << "  "
                    << QString::fromStdString(dr.error) << "\n";
            perDevice.append(dj);
            continue;
        }

        const QString extractDir =
            QDir(cache.packageDir(*best)).filePath("extracted");
        PackageExtractor extractor;
        ExtractResult ex = extractor.extract(dr.payloadPath, extractDir);
        if (!ex.ok) {
            skippedIds.push_back(d.instanceId);
            dj["stage"] = "extract";
            dj["result"] = "skipped";
            dj["reason"] = QString::fromStdString(ex.error);
            if (!json)
                out << "  [SKIP EXT]  " << QString::fromStdString(d.name) << "  "
                    << QString::fromStdString(ex.error) << "\n";
            perDevice.append(dj);
            continue;
        }

        const auto infs = PackageExtractor::findInfFiles(ex.extractedDir);
        int accepted = 0;
        QJsonArray infArr;
        for (const auto& infPath : infs) {
            const InfValidation v = validateInf(infPath);
            QJsonObject io;
            io["inf"] = QString::fromStdString(infPath);
            io["verdict"] = v.verdict == InfVerdict::Ok      ? "ok"
                            : v.verdict == InfVerdict::Warn   ? "warn"
                                                             : "reject";
            QJsonArray msgs;
            for (const auto& m : v.messages) msgs.append(QString::fromStdString(m));
            io["messages"] = msgs;
            infArr.append(io);

            if (v.verdict == InfVerdict::Reject) {
                if (!json)
                    out << "  [REJECT INF] " << QString::fromStdString(infPath) << "\n";
                continue;
            }
            if (v.verdict == InfVerdict::Warn && !v.hasCatalog) {
                // ADR-0006: unverifiable (unsigned) package -> warn + skip.
                if (!json)
                    out << "  [SKIP UNSIGNED] " << QString::fromStdString(infPath) << "\n";
                continue;
            }
            infsToInstall.push_back(infPath);
            ++accepted;
        }
        dj["infs"] = infArr;

        if (accepted == 0) {
            skippedIds.push_back(d.instanceId);
            dj["stage"] = "validate";
            dj["result"] = "skipped";
            dj["reason"] = "no acceptable INF (unsigned or rejected)";
        } else {
            dj["stage"] = "queued";
            dj["result"] = "ready";
            dj["match"] = QString::fromStdString(best->driverName);
            if (!json)
                out << "  [READY]     " << QString::fromStdString(d.name) << "  ->  "
                    << QString::fromStdString(best->driverName) << "  ("
                    << accepted << " INF)\n";
        }
        perDevice.append(dj);
    }

    // Install pass.
    InstallReport ir = installer.installInfs(infsToInstall);
    if (!ir.fatalError.empty()) {
        out << "Error: " << QString::fromStdString(ir.fatalError) << "\n";
        return 2;
    }

    // Verify pass (skipped for dry run: nothing changed).
    std::vector<VerifyResult> vr;
    if (!dryRun) {
        vr = verifyAfterInstall(before, ir.rebootRequired, notFoundIds, skippedIds);
    }

    // ----- report -----
    int already = 0, installed = 0, failed = 0, notfound = 0, skipped = 0, reboot = 0;
    for (const auto& r : vr) {
        switch (r.status) {
            case DriverInstallStatus::AlreadyInstalled: ++already; break;
            case DriverInstallStatus::Installed: ++installed; break;
            case DriverInstallStatus::Failed: ++failed; break;
            case DriverInstallStatus::NotFound: ++notfound; break;
            case DriverInstallStatus::Skipped: ++skipped; break;
            case DriverInstallStatus::RequiresReboot: ++reboot; break;
        }
    }
    const bool rebootRequired = ir.rebootRequired || reboot > 0;

    if (json) {
        QJsonObject root;
        root["dryRun"] = dryRun;
        root["elevated"] = ir.ranElevated;
        root["devices"] = perDevice;
        QJsonArray installArr;
        for (const auto& o : ir.perInf) {
            QJsonObject io;
            io["inf"] = QString::fromStdString(o.infPath);
            io["ok"] = o.ok;
            io["exitCode"] = o.exitCode;
            io["rebootRequired"] = o.rebootRequired;
            io["publishedName"] = QString::fromStdString(o.publishedName);
            installArr.append(io);
        }
        root["install"] = installArr;
        QJsonArray verifyArr;
        for (const auto& r : vr) {
            QJsonObject ro;
            ro["instanceId"] = QString::fromStdString(r.instanceId);
            ro["device"] = QString::fromStdString(r.deviceName);
            ro["status"] = toString(r.status);
            verifyArr.append(ro);
        }
        root["verify"] = verifyArr;
        QJsonObject sum;
        sum["alreadyInstalled"] = already;
        sum["installed"] = installed;
        sum["failed"] = failed;
        sum["notFound"] = notfound;
        sum["skipped"] = skipped;
        sum["requiresReboot"] = reboot;
        sum["rebootRequired"] = rebootRequired;
        root["summary"] = sum;
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    } else {
        out << "\n";
        if (dryRun) {
            out << "Dry run complete. " << infsToInstall.size()
                << " INF(s) would be installed via pnputil.\n";
            out << "Not found: " << notFoundIds.size()
                << "   Skipped (unsigned/invalid): " << skippedIds.size() << "\n";
        } else {
            out << "Drivers\n-------\n";
            out << "Already installed: " << already << "\n";
            out << "Installed:         " << installed << "\n";
            out << "Requires reboot:   " << reboot << "\n";
            out << "Failed:            " << failed << "\n";
            out << "Not found:         " << notfound << "\n";
            out << "Skipped:           " << skipped << "\n";
            out << "\nReboot required: " << (rebootRequired ? "YES" : "NO") << "\n";
        }
    }
    out.flush();

    if (!ir.fatalError.empty()) return 2;
    if (dryRun) {
        return (notFoundIds.empty() && skippedIds.empty()) ? 0 : 1;
    }
    if (failed > 0 || notfound > 0 || skipped > 0 || rebootRequired) return 1;
    return 0;
}

} // namespace shiftech::cli::commands

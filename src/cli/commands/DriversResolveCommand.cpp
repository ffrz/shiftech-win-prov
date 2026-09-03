#include "DriversResolveCommand.h"

#include "../../core/drivers/DriverDownloader.h"
#include "../../core/drivers/DriverMatch.h"
#include "../../core/drivers/DriverProviderFactory.h"
#include "../../core/hardware/DeviceEnumerator.h"
#include "../../core/system/SystemInspector.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace shiftech::core;
using namespace shiftech::core::drivers;
using namespace shiftech::core::hardware;

namespace shiftech::cli::commands {

namespace {

QString opt(const QStringList& args, const QString& name, const QString& def = {}) {
    const int i = args.indexOf(name);
    if (i >= 0 && i + 1 < args.size()) return args[i + 1];
    return def;
}

QJsonObject pkgJson(const DriverPackage& p) {
    QJsonObject o;
    o["driverName"] = QString::fromStdString(p.driverName);
    o["version"] = QString::fromStdString(p.version);
    o["provider"] = QString::fromStdString(p.provider);
    o["arch"] = p.arch == TargetSystem::Arch::x64 ? "x64" : "x86";
    o["downloadUrl"] = QString::fromStdString(p.downloadUrl);
    o["matchedVia"] = p.matchedVia == MatchVia::HardwareId       ? "hardwareId"
                      : p.matchedVia == MatchVia::CompatibleId   ? "compatibleId"
                                                                 : "unspecified";
    return o;
}

std::vector<std::string> deviceIds(const Device& d) {
    std::vector<std::string> ids = d.hardwareIds;
    ids.insert(ids.end(), d.compatibleIds.begin(), d.compatibleIds.end());
    return ids;
}

} // namespace

int DriversResolveCommand::run(const QStringList& args) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#endif

    const bool json = args.contains("--json");
    const bool download = args.contains("--download");

    FactoryOptions fo;
    fo.cacheDir = opt(args, "--cache-dir");
    fo.mirrorUrl = opt(args, "--mirror-url").toStdString();
    fo.wsusScanPackage = opt(args, "--wsus-scan").toStdString();
    fo.mockIndexPath = opt(args, "--driver-index");
    const std::string order = opt(args, "--provider-order").toStdString();

    std::string err;
    auto chainOpt = buildProviderChain(order, fo, err);
    if (!chainOpt) {
        out << "Error: " << QString::fromStdString(err) << "\n";
        return 2;
    }
    ProviderChain chain = std::move(*chainOpt);

    const system::SystemInfo sys = system::SystemInspector::inspect();
    const TargetSystem target = currentTarget(sys);

    DeviceEnumerator enumerator;
    const std::vector<Device> needing = enumerator.enumerateNeedingDriver();

    DriverCache cache(fo.cacheDir);
    DriverDownloader downloader(cache);

    int resolved = 0, downloaded = 0, cacheHits = 0, notFound = 0, failed = 0;
    QJsonArray results;

    if (!json) {
        out << "Shiftech Win Provisioner - drivers resolve\n\n";
        out << "Chain: ";
        const auto names = chain.names();
        for (size_t i = 0; i < names.size(); ++i) {
            out << QString::fromStdString(names[i]);
            if (i + 1 < names.size()) out << " -> ";
        }
        out << "\nDevices needing a driver (" << needing.size() << ")\n\n";
    }

    for (const Device& d : needing) {
        DriverSearchResult sr = chain.resolve(d, target);
        auto best = pickBest(d, sr, target);

        QJsonObject ro;
        ro["device"] = QString::fromStdString(d.name);
        ro["instanceId"] = QString::fromStdString(d.instanceId);

        if (!best) {
            ++notFound;
            if (json) {
                ro["status"] = "not_found";
                ro["reason"] = QString::fromStdString(sr.notFoundReason);
            } else {
                out << "  [NOT FOUND] " << QString::fromStdString(d.name) << "\n"
                    << "              " << QString::fromStdString(sr.notFoundReason) << "\n";
            }
            results.append(ro);
            continue;
        }

        ++resolved;
        ro["match"] = pkgJson(*best);

        if (!download) {
            if (json) {
                ro["status"] = "resolved";
            } else {
                out << "  [RESOLVED]  " << QString::fromStdString(d.name) << "  ->  "
                    << QString::fromStdString(best->driverName) << " v"
                    << QString::fromStdString(best->version) << " ("
                    << QString::fromStdString(best->provider) << ")\n";
            }
            results.append(ro);
            continue;
        }

        DownloadResult dr = downloader.fetch(*best, deviceIds(d));
        if (!dr.ok) {
            ++failed;
            if (json) {
                ro["status"] = "download_failed";
                ro["reason"] = QString::fromStdString(dr.error);
            } else {
                out << "  [DL FAIL]   " << QString::fromStdString(d.name) << "  "
                    << QString::fromStdString(dr.error) << "\n";
            }
        } else if (dr.fromCache) {
            ++cacheHits;
            if (json) ro["status"] = "cache_hit";
            else
                out << "  [CACHE HIT] " << QString::fromStdString(d.name) << "  ->  "
                    << QString::fromStdString(best->driverName) << "\n";
        } else {
            ++downloaded;
            if (json) ro["status"] = "downloaded";
            else
                out << "  [DOWNLOAD]  " << QString::fromStdString(d.name) << "  ->  "
                    << QString::fromStdString(best->driverName) << "\n";
        }
        results.append(ro);
    }

    if (json) {
        QJsonObject root;
        QJsonObject t;
        t["arch"] = target.arch == TargetSystem::Arch::x64 ? "x64" : "x86";
        t["build"] = target.build;
        root["target"] = t;
        QJsonArray chainArr;
        for (const auto& n : chain.names()) chainArr.append(QString::fromStdString(n));
        root["chain"] = chainArr;
        root["results"] = results;
        QJsonObject summary;
        summary["resolved"] = resolved;
        summary["downloaded"] = downloaded;
        summary["cacheHits"] = cacheHits;
        summary["notFound"] = notFound;
        summary["failed"] = failed;
        root["summary"] = summary;
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    } else {
        out << "\nSummary: resolved " << resolved;
        if (download) out << ", downloaded " << downloaded << ", cache hits " << cacheHits
                          << ", failed " << failed;
        out << ", not found " << notFound << "\n";
    }
    out.flush();

    return (failed > 0 || notFound > 0) ? 1 : 0;
}

} // namespace shiftech::cli::commands

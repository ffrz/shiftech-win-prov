#include "ProvisioningEngine.h"

#include "../applications/WinGetProvider.h"
#include "../drivers/DriverCache.h"
#include "../drivers/DriverDownloader.h"
#include "../drivers/DriverInstaller.h"
#include "../drivers/DriverMatch.h"
#include "../drivers/DriverProviderFactory.h"
#include "../drivers/DriverVerifier.h"
#include "../drivers/InfValidator.h"
#include "../drivers/PackageExtractor.h"
#include "../hardware/DeviceEnumerator.h"
#include "../logging/StructuredLogger.h"
#include "../profiles/ProfileLoader.h"
#include "../system/SystemInspector.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <algorithm>

using namespace shiftech::core;
using shiftech::core::logging::StructuredLogger;

namespace shiftech::core::provisioning {

namespace {
std::string nowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
}
int64_t nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
}

ProvisioningEngine::ProvisioningEngine(EventSink sink) : m_sink(std::move(sink)) {}

void ProvisioningEngine::emitEvent(const std::string& category, Severity sev,
                                   const std::string& msg, int progress) {
    if (!m_sink) return;
    ProvisioningEvent e;
    e.timestamp = nowIso();
    e.category = category;
    e.severity = sev;
    e.message = msg;
    e.progress = progress;
    m_sink(e);
}

ProvisioningResult ProvisioningEngine::run(const ProvisioningOptions& opts) {
    ProvisioningResult result;
    ProvisioningState& st = result.state;

    st.runId = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss").toStdString();
    st.dryRun = opts.dryRun;
    st.startedAtMs = nowMs();

    StructuredLogger logger(st.runId, opts.logDir);
    result.runDir = logger.runDir();

    auto tick = [&](Stage s) {
        st.transitionTo(s);
        logger.writeState(st);
        emitEvent("pipeline", Severity::Info, std::string("stage: ") + toString(s));
    };
    auto ev = [&](const std::string& cat, Severity sev, const std::string& m, int p = -1) {
        emitEvent(cat, sev, m, p);
        ProvisioningEvent le;
        le.timestamp = nowIso();
        le.category = cat;
        le.severity = sev;
        le.message = m;
        le.progress = p;
        logger.recordEvent(le);
    };
    auto cancelled = [&]() {
        return opts.cancelRequested && opts.cancelRequested->load();
    };
    auto finishEarly = [&](const char* why) -> ProvisioningResult {
        ev("pipeline", Severity::Warning, std::string("cancelled: ") + why);
        st.finishedAtMs = nowMs();
        if (st.stage != Stage::Report && st.stage != Stage::Done &&
            isLegalTransition(st.stage, Stage::Report)) {
            st.transitionTo(Stage::Report);
        }
        result.report = buildReport(st);
        if (isLegalTransition(st.stage, Stage::Done)) st.transitionTo(Stage::Done);
        logger.finalize(st, result.report.toJson());
        return result;
    };
    auto fail = [&](const std::string& why) -> ProvisioningResult {
        st.fatalError = why;
        st.transitionTo(Stage::Failed);
        st.finishedAtMs = nowMs();
        ev("pipeline", Severity::Error, "FATAL: " + why);
        result.report = buildReport(st);
        logger.finalize(st, result.report.toJson());
        return result;
    };

    // ---- System Check ----
    tick(Stage::SystemCheck);
    const system::SystemInfo sys = system::SystemInspector::inspect();
    st.osName = sys.productName;
    st.osBuild = sys.buildNumber;
    st.arch = sys.arch == system::Arch::X64 ? "x64"
              : sys.arch == system::Arch::X86 ? "x86" : "unsupported";
    st.elevated = sys.elevated;
    ev("system", Severity::Info,
       sys.productName + " (build " + std::to_string(sys.buildNumber) + ") " + st.arch +
           (sys.elevated ? ", elevated" : ", NOT elevated"));

    if (sys.arch == system::Arch::Unsupported) {
        return fail("unsupported architecture (ARM or unknown)");
    }
    const bool doDrivers = !opts.skipDrivers;
    const bool doApps = !opts.skipApps && !opts.profile.empty();
    if (doDrivers && !opts.dryRun && !sys.elevated) {
        return fail("driver installation requires Administrator; re-run elevated or use --dry-run");
    }

    // ---- Hardware Scan ----
    tick(Stage::HardwareScan);
    hardware::DeviceEnumerator enumerator;
    const std::vector<hardware::Device> allDevices = enumerator.enumerate();
    if (allDevices.empty()) {
        return fail("device enumeration returned nothing");
    }
    std::vector<hardware::Device> needing;
    for (const auto& d : allDevices) if (d.needsDriver()) needing.push_back(d);
    st.devicesDetected = static_cast<int>(allDevices.size());
    st.devicesNeedingDriver = static_cast<int>(needing.size());
    ev("hardware", Severity::Info,
       "detected " + std::to_string(allDevices.size()) + " devices, " +
           std::to_string(needing.size()) + " need a driver");

    // ---- Driver stages ----
    std::vector<std::string> notFoundIds, skippedIds;
    std::vector<std::string> infsToInstall;
    drivers::InstallReport installReport;

    if (doDrivers && !needing.empty()) {
        tick(Stage::DriverAnalysis);

        drivers::FactoryOptions fo;
        fo.cacheDir = opts.cacheDir;
        fo.mirrorUrl = opts.mirrorUrl;
        fo.mockIndexPath = opts.mockDriverIndex;
        std::string cerr;
        auto chainOpt = drivers::buildProviderChain(opts.providerOrder, fo, cerr);
        if (!chainOpt) return fail("driver provider chain: " + cerr);
        drivers::ProviderChain chain = std::move(*chainOpt);

        const drivers::TargetSystem target = drivers::currentTarget(sys);
        drivers::DriverCache cache(opts.cacheDir);
        drivers::DriverDownloader downloader(cache);

        tick(Stage::DriverResolution);
        struct Planned { hardware::Device dev; drivers::DriverPackage pkg; };
        std::vector<Planned> planned;
        int i = 0;
        for (const auto& d : needing) {
            if (cancelled()) return finishEarly("during driver resolution");
            const int prog = ++i * 100 / std::max<int>(1, (int)needing.size());
            auto sr = chain.resolve(d, target);
            auto best = drivers::pickBest(d, sr, target);
            if (!best) {
                notFoundIds.push_back(d.instanceId);
                st.drivers.push_back({d.instanceId, d.name, "NotFound", sr.notFoundReason});
                ev("driver", Severity::Warning, d.name + ": no driver found", prog);
            } else {
                planned.push_back({d, *best});
                ev("driver", Severity::Info,
                   d.name + " -> " + best->driverName + " v" + best->version, prog);
            }
        }

        tick(Stage::DriverDownload);
        std::vector<Planned> downloaded;
        i = 0;
        for (const auto& p : planned) {
            if (cancelled()) return finishEarly("during driver download");
            const int prog = ++i * 100 / std::max<int>(1, (int)planned.size());
            std::vector<std::string> ids = p.dev.hardwareIds;
            ids.insert(ids.end(), p.dev.compatibleIds.begin(), p.dev.compatibleIds.end());
            auto dr = downloader.fetch(p.pkg, ids);
            if (!dr.ok) {
                skippedIds.push_back(p.dev.instanceId);
                st.drivers.push_back({p.dev.instanceId, p.dev.name, "Skipped",
                                      "download: " + dr.error});
                ev("driver", Severity::Warning, p.dev.name + ": download failed: " + dr.error,
                   prog);
                continue;
            }
            // extract + validate
            const QString exDir = QDir(cache.packageDir(p.pkg)).filePath("extracted");
            drivers::PackageExtractor extractor;
            auto ex = extractor.extract(dr.payloadPath, exDir);
            if (!ex.ok) {
                skippedIds.push_back(p.dev.instanceId);
                st.drivers.push_back({p.dev.instanceId, p.dev.name, "Skipped",
                                      "extract: " + ex.error});
                ev("driver", Severity::Warning, p.dev.name + ": extract failed", prog);
                continue;
            }
            int accepted = 0;
            for (const auto& inf : drivers::PackageExtractor::findInfFiles(ex.extractedDir)) {
                const auto v = drivers::validateInf(inf);
                if (v.verdict == drivers::InfVerdict::Reject) continue;
                if (v.verdict == drivers::InfVerdict::Warn && !v.hasCatalog) {
                    ev("driver", Severity::Warning,
                       p.dev.name + ": skipping unsigned INF (ADR-0006)");
                    continue;
                }
                infsToInstall.push_back(inf);
                ++accepted;
            }
            if (accepted == 0) {
                skippedIds.push_back(p.dev.instanceId);
                st.drivers.push_back({p.dev.instanceId, p.dev.name, "Skipped",
                                      "no acceptable INF"});
            } else {
                downloaded.push_back(p);
            }
        }

        tick(Stage::DriverInstall);
        drivers::InstallerOptions io;
        io.dryRun = opts.dryRun;
        drivers::DriverInstaller installer(io);
        installReport = installer.installInfs(infsToInstall);
        if (!installReport.fatalError.empty()) {
            return fail(installReport.fatalError);
        }
        st.rebootRequired = st.rebootRequired || installReport.rebootRequired;
        ev("driver", installReport.anyFailed ? Severity::Warning : Severity::Success,
           std::string(opts.dryRun ? "dry-run: " : "") + "pnputil processed " +
               std::to_string(installReport.perInf.size()) + " INF(s)");

        tick(Stage::DriverVerify);
        if (!opts.dryRun) {
            auto vr = drivers::verifyAfterInstall(needing, installReport.rebootRequired,
                                                  notFoundIds, skippedIds);
            for (const auto& r : vr) {
                // Don't double-record devices already marked NotFound/Skipped above.
                bool exists = false;
                for (auto& existing : st.drivers)
                    if (existing.instanceId == r.instanceId) { exists = true; break; }
                if (!exists) {
                    st.drivers.push_back({r.instanceId, r.deviceName,
                                          drivers::toString(r.status), r.detail});
                }
                if (r.status == drivers::DriverInstallStatus::RequiresReboot)
                    st.rebootRequired = true;
            }
        } else {
            for (const auto& p : downloaded) {
                st.drivers.push_back({p.dev.instanceId, p.dev.name, "Installed",
                                      "dry-run: would install"});
            }
        }
    } else if (doDrivers) {
        // no devices need a driver — still walk the stages so state history is complete
        tick(Stage::DriverAnalysis);
        tick(Stage::DriverResolution);
        tick(Stage::DriverDownload);
        tick(Stage::DriverInstall);
        tick(Stage::DriverVerify);
        ev("driver", Severity::Success, "no devices require a driver");
    } else {
        ev("driver", Severity::Info, "driver stages skipped (--skip-drivers)");
        tick(Stage::DriverVerify);
    }

    // ---- Application stages ----
    if (doApps) {
        tick(Stage::AppDetection);
        const QString profilePath = [&]() -> QString {
            const QString name = QString::fromStdString(opts.profile);
            if (QFileInfo(name).isAbsolute() && name.endsWith(".json")) return name;
            const QString exeDir = QCoreApplication::applicationDirPath();
            QStringList dirs;
            if (!opts.profilesDir.isEmpty()) dirs << opts.profilesDir;
            else dirs << exeDir + "/profiles" << exeDir + "/../profiles"
                      << exeDir + "/../../profiles";
            for (const QString& d : dirs) {
                const QString c = QDir(d).filePath(name + ".json");
                if (QFileInfo::exists(c)) return c;
            }
            return {};
        }();
        if (profilePath.isEmpty()) {
            return fail("profile '" + opts.profile + "' not found");
        }
        auto loaded = profiles::ProfileLoader::load(profilePath.toStdString());
        if (std::holds_alternative<profiles::ProfileLoadError>(loaded)) {
            return fail("profile: " +
                        std::get<profiles::ProfileLoadError>(loaded).message);
        }
        const auto profile = std::get<profiles::Profile>(loaded);
        applications::WinGetProvider winget;
        const bool wingetOk = winget.isAvailable();
        ev("application", wingetOk ? Severity::Info : Severity::Warning,
           wingetOk ? ("profile '" + profile.name + "': " +
                       std::to_string(profile.applications.size()) + " apps")
                    : "winget unavailable — all apps will be skipped");

        tick(Stage::AppInstall);
        int i = 0;
        for (const auto& app : profile.applications) {
            if (cancelled()) return finishEarly("during application install");
            const int prog = ++i * 100 / std::max<int>(1, (int)profile.applications.size());
            AppItemResult ar;
            ar.id = app.id;
            ar.required = app.required;
            if (!wingetOk) {
                ar.status = "skipped_no_winget";
                ev("application", Severity::Warning, app.id + ": skipped (no winget)", prog);
            } else if (winget.isInstalled(app.id)) {
                ar.status = "already_installed";
                ev("application", Severity::Info, app.id + ": already installed", prog);
            } else if (opts.dryRun) {
                ar.status = "would_install";
                ev("application", Severity::Info, app.id + ": would install", prog);
            } else {
                const auto r = winget.install(app.id, {});
                ar.status = r.ok ? "installed" : "failed";
                ar.exitCode = r.exitCode;
                ev("application", r.ok ? Severity::Success : Severity::Warning,
                   app.id + (r.ok ? ": installed" : ": FAILED"), prog);
            }
            st.apps.push_back(ar);
        }
    } else {
        tick(Stage::AppDetection);
        tick(Stage::AppInstall);
        ev("application", Severity::Info,
           opts.profile.empty() ? "no profile given — apps skipped" : "apps skipped");
    }

    // ---- Final Verify + Report ----
    tick(Stage::FinalVerify);
    tick(Stage::Report);
    st.finishedAtMs = nowMs();
    result.report = buildReport(st);
    ev("report", Severity::Info, std::string("status: ") + toString(result.report.status));

    tick(Stage::Done);
    logger.finalize(st, result.report.toJson());
    return result;
}

} // namespace shiftech::core::provisioning

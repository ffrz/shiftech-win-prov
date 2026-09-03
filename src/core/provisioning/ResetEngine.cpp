#include "ResetEngine.h"

#include "../applications/LocalInstallerProvider.h"
#include "../config/ConfigTweak.h"
#include "../logging/StructuredLogger.h"
#include "../profiles/Profile.h"
#include "../system/SystemInspector.h"
#include "ProvisioningState.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QProcess>

using shiftech::core::logging::StructuredLogger;

namespace shiftech::core::provisioning {

namespace {

std::string nowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
}

struct Proc {
    int code = -1;
    QString out;
    bool timedOut = false;
};
Proc runProc(const QString& program, const QStringList& args, int timeoutMs) {
    Proc r;
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();
    if (!p.waitForStarted(10000)) { r.out = "failed to start " + program; return r; }
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

} // namespace

ResetEngine::ResetEngine(EventSink sink) : m_sink(std::move(sink)) {}

void ResetEngine::emitEvent(const std::string& cat, Severity sev, const std::string& msg) {
    if (!m_sink) return;
    ProvisioningEvent e;
    e.timestamp = nowIso();
    e.category = cat;
    e.severity = sev;
    e.message = msg;
    m_sink(e);
}

ResetResult ResetEngine::run(const ResetOptions& opts) {
    ResetResult res;

    QString runId = opts.runIdOrPath;
    if (runId.isEmpty()) {
        runId = StructuredLogger::latestRunId(opts.logDir);
        if (runId.isEmpty()) {
            res.fatalError = "no provisioning runs found to reset";
            return res;
        }
    }
    QJsonObject runJson;
    if (!StructuredLogger::loadRun(runId, opts.logDir, runJson)) {
        res.fatalError = "could not read run '" + runId.toStdString() + "'";
        return res;
    }
    const ProvisioningState st =
        ProvisioningState::fromJson(runJson.value("state").toObject());
    res.runId = st.runId;
    res.dryRun = opts.dryRun;

    const bool elevated = system::SystemInspector::isElevated();
    emitEvent("reset", Severity::Info,
         "resetting run " + st.runId + (opts.dryRun ? "  (DRY RUN)" : "") +
             (elevated ? "" : "  (NOT elevated - driver/machine reverts will be skipped)"));

    auto record = [&](const char* cat, const std::string& name, const std::string& outcome,
                      const std::string& detail) {
        res.items.push_back({cat, name, outcome, detail});
        if (outcome == "reverted") ++res.reverted;
        else if (outcome == "removed") ++res.removed;
        else if (outcome == "not-supported") ++res.notSupported;
        else if (outcome == "skipped") ++res.skipped;
        else if (outcome == "failed") ++res.failed;
    };

    // ---- Applications (only the ones this run installed) ----
    if (!opts.skipApps) {
        applications::LocalInstallerProvider local;
        for (const auto& a : st.apps) {
            if (a.status != "installed") continue; // leave already-present apps alone
            if (opts.dryRun) {
                record("app", a.id, "would-remove",
                       a.source == "local" ? "local uninstaller" : "winget uninstall");
                emitEvent("app", Severity::Info, a.id + ": would uninstall");
                continue;
            }
            if (a.source == "winget") {
                const QString id = QString::fromStdString(
                    a.wingetId.empty() ? a.id : a.wingetId);
                Proc p = runProc("winget",
                             {"uninstall", "--id", id, "--exact", "--silent",
                              "--accept-source-agreements", "--disable-interactivity"},
                             600000);
                const bool ok = p.code == 0;
                record("app", a.id, ok ? "removed" : "failed", p.out.left(200).toStdString());
                emitEvent("app", ok ? Severity::Success : Severity::Warning,
                     a.id + (ok ? ": uninstalled" : ": uninstall FAILED"));
            } else {
                // local: no generic uninstaller path yet - report not-supported
                record("app", a.id, "not-supported",
                       "local installers have no automatic uninstall; remove via "
                       "Add/Remove Programs");
                emitEvent("app", Severity::Warning,
                     a.id + ": local app - uninstall it manually");
            }
        }
    }

    // ---- Drivers (pnputil /delete-driver for the oemNN.inf this run published) ----
    if (!opts.skipDrivers) {
        for (const auto& d : st.drivers) {
            for (const auto& inf : d.publishedInfs) {
                if (opts.dryRun) {
                    record("driver", inf, "would-remove",
                           "pnputil /delete-driver " + inf + " /uninstall");
                    emitEvent("driver", Severity::Info, inf + ": would delete-driver");
                    continue;
                }
                if (!elevated) {
                    record("driver", inf, "skipped", "needs Administrator");
                    emitEvent("driver", Severity::Warning, inf + ": skipped (not elevated)");
                    continue;
                }
                Proc p = runProc("pnputil",
                             {"/delete-driver", QString::fromStdString(inf), "/uninstall",
                              "/force"},
                             180000);
                const bool ok = p.code == 0 || p.code == 3010;
                record("driver", inf, ok ? "removed" : "failed", p.out.left(200).toStdString());
                emitEvent("driver", ok ? Severity::Success : Severity::Warning,
                     inf + (ok ? ": deleted" : ": delete FAILED"));
            }
        }
    }

    // ---- Config tweaks (revert the ones this run applied) ----
    if (!opts.skipConfig) {
        // Pair each applied tweak with the args the profile used, if the run's profile is
        // still on disk. Not critical - most reverts don't need args.
        for (const auto& c : st.configTweaks) {
            const bool wasApplied =
                c.outcome == "Applied" || c.outcome == "RequiresReboot";
            if (!wasApplied) continue;

            if (opts.dryRun) {
                record("config", c.id, "would-revert", {});
                emitEvent("config", Severity::Info, c.id + ": would revert");
                continue;
            }
            config::TweakArgs noArgs;
            const auto rr = config::revertTweak(c.id, noArgs, elevated);
            std::string outcome;
            Severity sev = Severity::Info;
            switch (rr.outcome) {
                case config::RevertOutcome::Reverted:        outcome = "reverted";      sev = Severity::Success; break;
                case config::RevertOutcome::NothingToRevert: outcome = "reverted";      break; // count as done
                case config::RevertOutcome::NotSupported:    outcome = "not-supported"; sev = Severity::Warning; break;
                case config::RevertOutcome::Skipped:         outcome = "skipped";       sev = Severity::Warning; break;
                case config::RevertOutcome::Failed:          outcome = "failed";        sev = Severity::Warning; break;
            }
            record("config", c.id, outcome, rr.detail);
            emitEvent("config", sev, c.id + ": " + outcome +
                                    (rr.detail.empty() ? "" : " (" + rr.detail + ")"));
        }
    }

    // ---- Cache purge (optional; not a Windows change) ----
    if (opts.purgeCache && !opts.dryRun) {
        const QString cacheRoot =
            opts.cacheDir.isEmpty()
                ? QDir(QCoreApplication::applicationDirPath()).filePath("cache/drivers")
                : QDir(opts.cacheDir).filePath("drivers");
        QDir(cacheRoot).removeRecursively();
        record("cache", "cache/drivers", "removed", cacheRoot.toStdString());
        emitEvent("cache", Severity::Info, "purged " + cacheRoot.toStdString());
    }

    emitEvent("reset", Severity::Info,
         "done: reverted " + std::to_string(res.reverted) + ", removed " +
             std::to_string(res.removed) + ", not-supported " +
             std::to_string(res.notSupported) + ", skipped " +
             std::to_string(res.skipped) + ", failed " + std::to_string(res.failed));
    return res;
}

} // namespace shiftech::core::provisioning

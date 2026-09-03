#include "ResetCommand.h"

#include "../../core/provisioning/ResetEngine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace shiftech::core::provisioning;

namespace shiftech::cli::commands {

namespace {
QString opt(const QStringList& a, const QString& n, const QString& d = {}) {
    const int i = a.indexOf(n);
    return (i >= 0 && i + 1 < a.size()) ? a[i + 1] : d;
}
} // namespace

int ResetCommand::run(const QStringList& args) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#endif
    const bool json = args.contains("--json");

    ResetOptions o;
    o.runIdOrPath = opt(args, "--run");
    if (o.runIdOrPath.isEmpty() && !args.contains("--last") && !args.contains("--run")) {
        // default to --last
    }
    o.dryRun = args.contains("--dry-run");
    o.skipApps = args.contains("--skip-apps");
    o.skipDrivers = args.contains("--skip-drivers");
    o.skipConfig = args.contains("--skip-config");
    o.purgeCache = args.contains("--purge-cache");
    o.logDir = opt(args, "--log-dir");
    o.cacheDir = opt(args, "--cache-dir");

    EventSink sink;
    if (!json) {
        sink = [&out](const ProvisioningEvent& e) {
            const QString ts = QString::fromStdString(e.timestamp).mid(11, 8);
            const char* tag = e.severity == Severity::Success   ? "OK   "
                              : e.severity == Severity::Warning ? "WARN "
                              : e.severity == Severity::Error   ? "ERROR"
                                                               : "INFO ";
            out << "[" << ts << "] " << tag << "  " << QString::fromStdString(e.message)
                << "\n";
            out.flush();
        };
    }

    ResetEngine engine(sink);
    ResetResult r = engine.run(o);

    if (!r.fatalError.empty()) {
        if (json) {
            QJsonObject j;
            j["error"] = QString::fromStdString(r.fatalError);
            out << QJsonDocument(j).toJson(QJsonDocument::Indented);
        } else {
            out << "Error: " << QString::fromStdString(r.fatalError) << "\n";
        }
        return 2;
    }

    if (json) {
        QJsonObject root;
        root["runId"] = QString::fromStdString(r.runId);
        root["dryRun"] = r.dryRun;
        QJsonArray items;
        for (const auto& it : r.items) {
            QJsonObject io;
            io["category"] = QString::fromStdString(it.category);
            io["name"] = QString::fromStdString(it.name);
            io["outcome"] = QString::fromStdString(it.outcome);
            io["detail"] = QString::fromStdString(it.detail);
            items.append(io);
        }
        root["items"] = items;
        QJsonObject sum;
        sum["reverted"] = r.reverted;
        sum["removed"] = r.removed;
        sum["notSupported"] = r.notSupported;
        sum["skipped"] = r.skipped;
        sum["failed"] = r.failed;
        root["summary"] = sum;
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    } else {
        out << "\nReset of run " << QString::fromStdString(r.runId)
            << (r.dryRun ? "  (DRY RUN)" : "") << "\n";
        out << "  removed apps/drivers: " << r.removed << "\n";
        out << "  reverted tweaks:      " << r.reverted << "\n";
        out << "  not auto-undoable:    " << r.notSupported << "\n";
        out << "  skipped:              " << r.skipped << "\n";
        out << "  failed:               " << r.failed << "\n";
        if (r.notSupported > 0)
            out << "\nSome items can't be undone automatically - see the lines above.\n";
    }
    out.flush();

    return (r.failed > 0) ? 1 : 0;
}

} // namespace shiftech::cli::commands

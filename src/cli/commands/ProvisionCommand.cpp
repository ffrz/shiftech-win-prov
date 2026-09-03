#include "ProvisionCommand.h"

#include "../../core/config/ConfigTweak.h"
#include "../../core/logging/StructuredLogger.h"
#include "../../core/provisioning/ProvisioningEngine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace shiftech::core::provisioning;
using shiftech::core::logging::StructuredLogger;

namespace shiftech::cli::commands {

namespace {
QString opt(const QStringList& a, const QString& n, const QString& d = {}) {
    const int i = a.indexOf(n);
    return (i >= 0 && i + 1 < a.size()) ? a[i + 1] : d;
}

const char* sevTag(Severity s) {
    switch (s) {
        case Severity::Info: return "INFO ";
        case Severity::Success: return "OK   ";
        case Severity::Warning: return "WARN ";
        case Severity::Error: return "ERROR";
    }
    return "INFO ";
}
} // namespace

int ConfigCommand::run(const QStringList& args) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#endif
    if (args.value(0) != "list") {
        out << "Usage: provisioner config list [--json]\n";
        return 3;
    }
    const bool json = args.contains("--json");
    const auto cat = shiftech::core::config::catalog();
    if (json) {
        QJsonArray arr;
        for (const auto& t : cat) {
            QJsonObject o;
            o["id"] = QString::fromStdString(t.id);
            o["title"] = QString::fromStdString(t.title);
            o["description"] = QString::fromStdString(t.description);
            o["needsElevation"] = t.needsElevation;
            QJsonArray req;
            for (const auto& r : t.requiredArgs) req.append(QString::fromStdString(r));
            o["requiredArgs"] = req;
            arr.append(o);
        }
        out << QJsonDocument(arr).toJson(QJsonDocument::Indented);
        return 0;
    }
    out << "Config tweaks (put the id in a profile's \"config\" section):\n\n";
    for (const auto& t : cat) {
        out << "  " << QString::fromStdString(t.id).leftJustified(28)
            << QString::fromStdString(t.title)
            << (t.needsElevation ? "  [admin]" : "");
        if (!t.requiredArgs.empty()) {
            QStringList rs;
            for (const auto& r : t.requiredArgs) rs << QString::fromStdString(r);
            out << "  args: " << rs.join(", ");
        }
        out << "\n      " << QString::fromStdString(t.description) << "\n";
    }
    return 0;
}

int ProvisionCommand::run(const QStringList& args) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#endif
    const bool json = args.contains("--json");

    ProvisioningOptions o;
    o.profile = opt(args, "--profile").toStdString();
    o.dryRun = args.contains("--dry-run");
    o.skipDrivers = args.contains("--skip-drivers");
    o.skipApps = args.contains("--skip-apps");
    o.providerOrder = opt(args, "--provider-order").toStdString();
    o.cacheDir = opt(args, "--cache-dir");
    o.profilesDir = opt(args, "--profiles-dir");
    o.appsDir = opt(args, "--apps-dir");
    o.logDir = opt(args, "--log-dir");
    o.mockDriverIndex = opt(args, "--driver-index");
    o.mirrorUrl = opt(args, "--mirror-url").toStdString();

    if (o.profile.empty() && !o.skipApps) {
        out << "Note: no --profile given; application stage will be skipped.\n";
    }

    EventSink sink;
    if (!json) {
        sink = [&out](const ProvisioningEvent& e) {
            const QString ts = QString::fromStdString(e.timestamp).mid(11, 8);
            out << "[" << ts << "] " << sevTag(e.severity) << "  "
                << QString::fromStdString(e.message);
            if (e.progress >= 0) out << "  (" << e.progress << "%)";
            out << "\n";
            out.flush();
        };
    }

    ProvisioningEngine engine(sink);
    ProvisioningResult r = engine.run(o);

    if (json) {
        QJsonObject root;
        root["runId"] = QString::fromStdString(r.state.runId);
        root["runDir"] = r.runDir;
        root["state"] = r.state.toJson();
        root["report"] = r.report.toJson();
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    } else {
        out << "\n" << QString::fromStdString(r.report.toText()) << "\n";
        out << "Log: " << r.runDir << "\n";
    }
    out.flush();

    switch (r.report.status) {
        case RunStatus::Success: return 0;
        case RunStatus::SuccessWithWarnings: return 1;
        case RunStatus::Failed: return 2;
    }
    return 2;
}

int ReportCommand::run(const QStringList& args) {
    QTextStream out(stdout);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#endif
    const bool json = args.contains("--json");
    const QString logDir = opt(args, "--log-dir");

    QString runId = opt(args, "--run");
    if (runId.isEmpty()) {
        runId = StructuredLogger::latestRunId(logDir);
        if (runId.isEmpty()) {
            out << "No provisioning runs found.\n";
            return 2;
        }
    }

    QJsonObject run;
    if (!StructuredLogger::loadRun(runId, logDir, run)) {
        out << "Could not read run '" << runId << "'.\n";
        return 2;
    }

    if (json) {
        out << QJsonDocument(run.value("report").toObject()).toJson(QJsonDocument::Indented);
        return 0;
    }

    // Rebuild text from the persisted state so formatting stays consistent.
    const ProvisioningState st = ProvisioningState::fromJson(run.value("state").toObject());
    const Report rep = buildReport(st);
    out << "Run: " << QString::fromStdString(st.runId) << "\n\n";
    out << QString::fromStdString(rep.toText());
    return 0;
}

} // namespace shiftech::cli::commands

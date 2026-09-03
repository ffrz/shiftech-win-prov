#include "StructuredLogger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

using shiftech::core::provisioning::ProvisioningEvent;
using shiftech::core::provisioning::ProvisioningState;

namespace shiftech::core::logging {

namespace {
QString baseLogDir(const QString& logDir) {
    if (!logDir.isEmpty()) return QDir(logDir).absolutePath();
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("logs");
}
}

StructuredLogger::StructuredLogger(const std::string& runId, const QString& logDir) {
    m_runDir = QDir(baseLogDir(logDir)).absoluteFilePath(QString::fromStdString(runId));
    QDir().mkpath(m_runDir);
}

QString StructuredLogger::runJsonPath() const {
    return QDir(m_runDir).absoluteFilePath("run.json");
}
QString StructuredLogger::stateJsonPath() const {
    return QDir(m_runDir).absoluteFilePath("state.json");
}

QString StructuredLogger::redact(const QString& message) {
    QString m = message;
    // token/password/key = <value>  ->  redact the value
    static const QRegularExpression kv(
        R"((?i)\b(password|passwd|pwd|token|secret|api[_-]?key|bearer)\b\s*[:=]\s*\S+)");
    m.replace(kv, "\\1=***REDACTED***");
    // long hex/base64-ish blobs
    static const QRegularExpression blob(R"(\b[A-Za-z0-9+/_-]{32,}={0,2}\b)");
    m.replace(blob, "***REDACTED***");
    return m;
}

void StructuredLogger::recordEvent(const ProvisioningEvent& e) {
    ProvisioningEvent copy = e;
    copy.message = redact(QString::fromStdString(e.message)).toStdString();
    m_events.push_back(copy);
}

static QJsonObject eventJson(const ProvisioningEvent& e) {
    QJsonObject o;
    o["timestamp"] = QString::fromStdString(e.timestamp);
    o["category"] = QString::fromStdString(e.category);
    o["severity"] = shiftech::core::provisioning::toString(e.severity);
    o["message"] = QString::fromStdString(e.message);
    o["progress"] = e.progress;
    return o;
}

void StructuredLogger::writeState(const ProvisioningState& s) {
    QFile f(stateJsonPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(s.toJson()).toJson(QJsonDocument::Indented));
    }
}

void StructuredLogger::finalize(const ProvisioningState& s, const QJsonObject& report) {
    QJsonArray events;
    for (const auto& e : m_events) events.append(eventJson(e));

    QJsonObject root;
    root["runId"] = QString::fromStdString(s.runId);
    root["state"] = s.toJson();
    root["events"] = events;
    root["report"] = report;

    QFile f(runJsonPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
    writeState(s);
}

bool StructuredLogger::loadRun(const QString& runIdOrPath, const QString& logDir,
                               QJsonObject& out) {
    QString path = runIdOrPath;
    if (!path.endsWith(".json")) {
        path = QDir(QDir(baseLogDir(logDir)).absoluteFilePath(runIdOrPath))
                   .absoluteFilePath("run.json");
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    out = doc.object();
    return true;
}

QString StructuredLogger::latestRunId(const QString& logDir) {
    const QDir dir(baseLogDir(logDir));
    const auto subs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QString latest;
    for (const QString& s : subs) {
        if (QFileInfo::exists(dir.absoluteFilePath(s + "/run.json"))) latest = s;
    }
    return latest; // names are sortable timestamps
}

} // namespace shiftech::core::logging

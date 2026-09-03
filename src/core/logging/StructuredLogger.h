#pragma once

#include "../provisioning/ProvisioningEvent.h"
#include "../provisioning/ProvisioningState.h"
#include <QString>
#include <string>
#include <vector>

class QJsonObject;

namespace shiftech::core::logging {

// Writes one run directory: logs/<runId>/ with run.json (system info + every event +
// state + final report) and state.json (updated after each transition).
// Also mirrors events to the console via an optional callback.
//
// No credentials / tokens are ever written. Usernames that appear in filesystem paths are
// acceptable (they are not secrets); anything that looks like a token/password/key in an
// event message is redacted.
class StructuredLogger {
public:
    // logDir empty => <exeDir>/logs
    StructuredLogger(const std::string& runId, const QString& logDir = QString());

    QString runDir() const { return m_runDir; }

    void recordEvent(const provisioning::ProvisioningEvent& e);
    void writeState(const provisioning::ProvisioningState& s);   // overwrites state.json
    void finalize(const provisioning::ProvisioningState& s, const QJsonObject& report);

    static QString redact(const QString& message);

    // Load a past run's run.json (for `provisioner report`). runId or an absolute path.
    static bool loadRun(const QString& runIdOrPath, const QString& logDir, QJsonObject& out);
    static QString latestRunId(const QString& logDir);

private:
    QString m_runDir;
    std::vector<provisioning::ProvisioningEvent> m_events;
    QString runJsonPath() const;
    QString stateJsonPath() const;
};

} // namespace shiftech::core::logging

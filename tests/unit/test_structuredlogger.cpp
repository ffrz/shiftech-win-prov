#include <QtTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include "../../src/core/logging/StructuredLogger.h"
#include "../../src/core/provisioning/ReportBuilder.h"

using namespace shiftech::core::logging;
using namespace shiftech::core::provisioning;

class TestStructuredLogger : public QObject {
    Q_OBJECT
private slots:
    void redactsSecretsInMessages() {
        QCOMPARE(StructuredLogger::redact("password=hunter2").toStdString(),
                 std::string("password=***REDACTED***"));
        QVERIFY(StructuredLogger::redact("token: abcdef0123456789abcdef0123456789abcd")
                    .contains("REDACTED"));
        QCOMPARE(StructuredLogger::redact("installing Google.Chrome").toStdString(),
                 std::string("installing Google.Chrome"));
    }

    void writesValidRunJsonWithoutSecret() {
        QTemporaryDir tmp;
        StructuredLogger logger("2026-01-01_000000", tmp.path());

        ProvisioningEvent e;
        e.timestamp = "2026-01-01T00:00:00Z";
        e.category = "application";
        e.severity = Severity::Info;
        e.message = "auth token=SUPERSECRETVALUE1234567890abcdef";
        logger.recordEvent(e);

        ProvisioningState st;
        st.runId = "2026-01-01_000000";
        st.transitionTo(Stage::SystemCheck);
        st.stage = Stage::Done;
        Report rep = buildReport(st);
        logger.finalize(st, rep.toJson());

        QFile f(logger.runDir() + "/run.json");
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray content = f.readAll();

        const QJsonDocument doc = QJsonDocument::fromJson(content);
        QVERIFY(doc.isObject());
        QVERIFY(doc.object().contains("events"));
        QVERIFY(doc.object().contains("report"));

        QVERIFY(!QString::fromUtf8(content).contains("SUPERSECRETVALUE"));
        QVERIFY(QString::fromUtf8(content).contains("REDACTED"));
    }

    void latestRunIdAndLoad() {
        QTemporaryDir tmp;
        for (const char* id : {"2026-01-01_000000", "2026-06-15_120000"}) {
            StructuredLogger l(id, tmp.path());
            ProvisioningState st;
            st.runId = id;
            st.stage = Stage::Done;
            l.finalize(st, buildReport(st).toJson());
        }
        QCOMPARE(StructuredLogger::latestRunId(tmp.path()).toStdString(),
                 std::string("2026-06-15_120000"));

        QJsonObject out;
        QVERIFY(StructuredLogger::loadRun("2026-01-01_000000", tmp.path(), out));
        QCOMPARE(out["runId"].toString().toStdString(), std::string("2026-01-01_000000"));
    }
};

QTEST_MAIN(TestStructuredLogger)
#include "test_structuredlogger.moc"

#include <QtTest>
#include "../../src/core/provisioning/ReportBuilder.h"

using namespace shiftech::core::provisioning;

class TestReportBuilder : public QObject {
    Q_OBJECT
private slots:
    void cleanRunIsSuccess() {
        ProvisioningState s;
        s.stage = Stage::Done;
        s.devicesDetected = 10;
        s.devicesNeedingDriver = 2;
        s.drivers = {{"a", "A", "Installed", ""}, {"b", "B", "AlreadyInstalled", ""}};
        s.apps = {{"X", true, "installed", 0}, {"Y", false, "already_installed", 0}};
        s.startedAtMs = 0;
        s.finishedAtMs = 60000;

        Report r = buildReport(s);
        QCOMPARE(r.status, RunStatus::Success);
        QCOMPARE(r.driversInstalled, 1);
        QCOMPARE(r.driversAlreadyInstalled, 1);
        QCOMPARE(r.appsInstalled, 1);
        QVERIFY(r.toText().find("SUCCESS") != std::string::npos);
    }

    void notFoundDriverIsWarning() {
        ProvisioningState s;
        s.stage = Stage::Done;
        s.drivers = {{"a", "A", "NotFound", ""}};
        Report r = buildReport(s);
        QCOMPARE(r.status, RunStatus::SuccessWithWarnings);
        QCOMPARE(r.driversNotFound, 1);
    }

    void failedRequiredAppIsWarning() {
        ProvisioningState s;
        s.stage = Stage::Done;
        s.apps = {{"X", true, "failed", 1}};
        Report r = buildReport(s);
        QCOMPARE(r.status, RunStatus::SuccessWithWarnings);
        QCOMPARE(r.appsFailed, 1);
        QCOMPARE(r.appsFailedRequired, 1);
    }

    void rebootIsWarning() {
        ProvisioningState s;
        s.stage = Stage::Done;
        s.drivers = {{"a", "A", "RequiresReboot", ""}};
        Report r = buildReport(s);
        QVERIFY(r.rebootRequired);
        QCOMPARE(r.status, RunStatus::SuccessWithWarnings);
    }

    void fatalIsFailed() {
        ProvisioningState s;
        s.stage = Stage::Failed;
        s.fatalError = "not elevated";
        Report r = buildReport(s);
        QCOMPARE(r.status, RunStatus::Failed);
        QVERIFY(r.toText().find("FATAL") != std::string::npos);
    }
};

QTEST_MAIN(TestReportBuilder)
#include "test_reportbuilder.moc"

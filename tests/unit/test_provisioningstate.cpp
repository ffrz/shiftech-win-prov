#include <QtTest>
#include <QJsonObject>
#include "../../src/core/provisioning/ProvisioningState.h"

using namespace shiftech::core::provisioning;

class TestProvisioningState : public QObject {
    Q_OBJECT
private slots:
    void legalForwardTransition() {
        ProvisioningState s;
        s.transitionTo(Stage::SystemCheck);
        s.transitionTo(Stage::HardwareScan);
        QCOMPARE(s.stage, Stage::HardwareScan);
        QCOMPARE(s.stageHistory.size(), size_t(2));
    }

    void forwardSkipIsLegal() {
        ProvisioningState s;
        s.transitionTo(Stage::SystemCheck);
        s.transitionTo(Stage::DriverVerify); // --skip-drivers jump
        QCOMPARE(s.stage, Stage::DriverVerify);
    }

    void backwardTransitionThrows() {
        ProvisioningState s;
        s.transitionTo(Stage::HardwareScan);
        QVERIFY_EXCEPTION_THROWN(s.transitionTo(Stage::SystemCheck), std::logic_error);
    }

    void failFromAnyStage() {
        ProvisioningState s;
        s.transitionTo(Stage::DriverInstall);
        s.transitionTo(Stage::Failed);
        QCOMPARE(s.stage, Stage::Failed);
    }

    void noEscapeFromTerminal() {
        ProvisioningState s;
        s.transitionTo(Stage::Done);
        QVERIFY_EXCEPTION_THROWN(s.transitionTo(Stage::Report), std::logic_error);
    }

    void jsonRoundTrip() {
        ProvisioningState s;
        s.runId = "2026-01-02_030405";
        s.dryRun = true;
        s.osName = "Windows 11 Pro";
        s.osBuild = 22631;
        s.arch = "x64";
        s.devicesDetected = 42;
        s.devicesNeedingDriver = 3;
        s.rebootRequired = true;
        s.drivers.push_back({"INST\\1", "NIC", "Installed", ""});
        s.apps.push_back({"Google.Chrome", true, "installed", 0});
        s.transitionTo(Stage::SystemCheck);
        s.startedAtMs = 1000;
        s.finishedAtMs = 5000;

        const QJsonObject j = s.toJson();
        const ProvisioningState back = ProvisioningState::fromJson(j);

        QCOMPARE(back.runId, s.runId);
        QCOMPARE(back.stage, Stage::SystemCheck);
        QCOMPARE(back.dryRun, true);
        QCOMPARE(back.osBuild, 22631);
        QCOMPARE(back.devicesNeedingDriver, 3);
        QCOMPARE(back.rebootRequired, true);
        QCOMPARE(back.drivers.size(), size_t(1));
        QCOMPARE(back.drivers[0].status.c_str(), "Installed");
        QCOMPARE(back.apps.size(), size_t(1));
        QCOMPARE(back.apps[0].required, true);
        QCOMPARE(back.finishedAtMs, int64_t(5000));
    }
};

QTEST_MAIN(TestProvisioningState)
#include "test_provisioningstate.moc"

#include <QtTest>
#include "../../src/core/drivers/DriverVerifier.h"

using namespace shiftech::core::drivers;
using namespace shiftech::core::hardware;

namespace {
Device dev(DeviceStatus s, int problem = 0, const char* ver = "") {
    Device d;
    d.instanceId = "INST\\1";
    d.name = "Test NIC";
    d.status = s;
    d.problemCode = problem;
    d.driverVersion = ver;
    return d;
}
}

class TestDriverVerifier : public QObject {
    Q_OBJECT
private slots:
    void alreadyInstalled() {
        Device before = dev(DeviceStatus::Ok);
        QCOMPARE(classifyTransition(before, before, false),
                 DriverInstallStatus::AlreadyInstalled);
    }

    void installedWhenProblemClears() {
        Device before = dev(DeviceStatus::Problem, 28);
        Device after = dev(DeviceStatus::Ok, 0, "1.0.0.0");
        QCOMPARE(classifyTransition(before, after, false),
                 DriverInstallStatus::Installed);
    }

    void requiresRebootWhenInstallerSaysSo() {
        Device before = dev(DeviceStatus::Problem, 28);
        Device after = dev(DeviceStatus::Ok, 0, "1.0.0.0");
        QCOMPARE(classifyTransition(before, after, true),
                 DriverInstallStatus::RequiresReboot);
    }

    void failedWhenStillProblem() {
        Device before = dev(DeviceStatus::Problem, 28);
        Device after = dev(DeviceStatus::Problem, 28);
        QCOMPARE(classifyTransition(before, after, false),
                 DriverInstallStatus::Failed);
    }

    void deviceGoneWithoutReboot() {
        Device before = dev(DeviceStatus::NoDriver);
        QCOMPARE(classifyTransition(before, std::nullopt, false),
                 DriverInstallStatus::Failed);
    }

    void deviceGoneWithRebootHint() {
        Device before = dev(DeviceStatus::NoDriver);
        QCOMPARE(classifyTransition(before, std::nullopt, true),
                 DriverInstallStatus::RequiresReboot);
    }
};

QTEST_MAIN(TestDriverVerifier)
#include "test_driververifier.moc"

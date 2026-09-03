#include <QtTest>
#include "../../src/core/drivers/DriverProvider.h"

using namespace shiftech::core::system;
using namespace shiftech::core::drivers;

class TestTargetSystem : public QObject {
    Q_OBJECT
private slots:
    void mapsWin11() {
        SystemInfo info;
        info.buildNumber = 22631;
        info.arch = Arch::X64;
        TargetSystem t = currentTarget(info);
        QCOMPARE(t.os, TargetSystem::OsFamily::Win11);
        QCOMPARE(t.arch, TargetSystem::Arch::x64);
        QCOMPARE(t.build, 22631);
    }

    void mapsWin10() {
        SystemInfo info;
        info.buildNumber = 19045;
        info.arch = Arch::X64;
        QCOMPARE(currentTarget(info).os, TargetSystem::OsFamily::Win10);
    }

    void mapsWin8() {
        SystemInfo info;
        info.buildNumber = 9600;
        info.arch = Arch::X64;
        QCOMPARE(currentTarget(info).os, TargetSystem::OsFamily::Win8);
    }

    void mapsWin7() {
        SystemInfo info;
        info.buildNumber = 7601;
        info.arch = Arch::X86;
        TargetSystem t = currentTarget(info);
        QCOMPARE(t.os, TargetSystem::OsFamily::Win7);
        QCOMPARE(t.arch, TargetSystem::Arch::x86);
    }

    void defaultsAreInitialized() {
        TargetSystem t;
        QCOMPARE(t.os, TargetSystem::OsFamily::Win10);
        QCOMPARE(t.arch, TargetSystem::Arch::x64);
        QCOMPARE(t.build, 0);
    }
};

QTEST_MAIN(TestTargetSystem)
#include "test_targetsystem.moc"

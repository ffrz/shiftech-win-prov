#include <QtTest>
#include "../../src/core/drivers/DriverMatch.h"

using namespace shiftech::core::hardware;
using namespace shiftech::core::drivers;

class TestDriverMatch : public QObject {
    Q_OBJECT
private slots:
    void testCompareVersions() {
        QCOMPARE(compareVersions("1.0", "1.0"), 0);
        QCOMPARE(compareVersions("1.1", "1.0"), 1);
        QCOMPARE(compareVersions("1.0", "1.1"), -1);
        QCOMPARE(compareVersions("2.0.0.0", "1.9.9.9"), 1);
        QCOMPARE(compareVersions("10.0", "2.0"), 1);
        QCOMPARE(compareVersions("1.0.1", "1.0"), 1);
        QCOMPARE(compareVersions("1.0.0", "1.0"), 0);
    }

    void testArchFilter() {
        TargetSystem target;
        target.os = TargetSystem::OsFamily::Win10;
        target.arch = TargetSystem::Arch::x64;

        Device dev;
        DriverPackage pkg;
        pkg.arch = TargetSystem::Arch::x86;
        pkg.supportedOs = {"win10"};
        
        QCOMPARE(rankCandidate(dev, pkg, target), -1);

        pkg.arch = TargetSystem::Arch::x64;
        QCOMPARE(rankCandidate(dev, pkg, target), 0);
    }

    void testOsFilter() {
        TargetSystem target;
        target.os = TargetSystem::OsFamily::Win11;
        target.arch = TargetSystem::Arch::x64;

        Device dev;
        DriverPackage pkg;
        pkg.arch = TargetSystem::Arch::x64;
        pkg.supportedOs = {"win10"};

        QCOMPARE(rankCandidate(dev, pkg, target), -1);

        pkg.supportedOs = {"win10", "Win11"};
        QCOMPARE(rankCandidate(dev, pkg, target), 0);
    }

    void testPickBest() {
        TargetSystem target;
        target.os = TargetSystem::OsFamily::Win10;
        target.arch = TargetSystem::Arch::x64;

        Device dev;
        DriverSearchResult result;
        result.found = true;

        DriverPackage pkg1;
        pkg1.version = "1.0";
        pkg1.arch = TargetSystem::Arch::x64;
        pkg1.supportedOs = {"win10"};

        DriverPackage pkg2;
        pkg2.version = "2.0";
        pkg2.arch = TargetSystem::Arch::x64;
        pkg2.supportedOs = {"win10"};

        result.candidates.push_back(pkg1);
        result.candidates.push_back(pkg2);

        auto best = pickBest(dev, result, target);
        QVERIFY(best.has_value());
        QCOMPARE(best->version.c_str(), "2.0");
    }

    void testPickBestNoCandidates() {
        TargetSystem target;
        target.os = TargetSystem::OsFamily::Win10;
        target.arch = TargetSystem::Arch::x64;

        Device dev;
        DriverSearchResult result;
        result.found = true;

        auto best = pickBest(dev, result, target);
        QVERIFY(!best.has_value());
    }

    void testHardwareIdBeatsCompatibleId() {
        TargetSystem target;
        target.os = TargetSystem::OsFamily::Win10;
        target.arch = TargetSystem::Arch::x64;

        Device dev;
        DriverSearchResult result;
        result.found = true;

        // Compatible-ID match with a HIGHER version number...
        DriverPackage compat;
        compat.version = "9.9.9.9";
        compat.arch = TargetSystem::Arch::x64;
        compat.supportedOs = {"win10"};
        compat.matchedVia = MatchVia::CompatibleId;

        // ...must still lose to a Hardware-ID match with a lower version.
        DriverPackage hwid;
        hwid.version = "1.0.0.0";
        hwid.arch = TargetSystem::Arch::x64;
        hwid.supportedOs = {"win10"};
        hwid.matchedVia = MatchVia::HardwareId;

        result.candidates = {compat, hwid};
        auto best = pickBest(dev, result, target);
        QVERIFY(best.has_value());
        QCOMPARE(best->version.c_str(), "1.0.0.0");
        QCOMPARE(best->matchedVia, MatchVia::HardwareId);
    }

    void testVersionTieBreakWithinSameMatchClass() {
        TargetSystem target;
        target.os = TargetSystem::OsFamily::Win11;
        target.arch = TargetSystem::Arch::x64;

        Device dev;
        DriverSearchResult result;
        result.found = true;

        DriverPackage older;
        older.version = "1.0";
        older.arch = TargetSystem::Arch::x64;
        older.supportedOs = {"win11"};
        older.matchedVia = MatchVia::HardwareId;

        DriverPackage newer = older;
        newer.version = "2.0";

        result.candidates = {older, newer};
        QCOMPARE(pickBest(dev, result, target)->version.c_str(), "2.0");
    }
};

QTEST_MAIN(TestDriverMatch)
#include "test_drivermatch.moc"

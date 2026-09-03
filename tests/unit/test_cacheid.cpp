#include <QtTest>
#include "../../src/core/drivers/DriverCache.h"

using namespace shiftech::core::drivers;

namespace {
DriverPackage pkg() {
    DriverPackage p;
    p.provider = "Realtek";
    p.driverName = "Realtek PCIe GbE";
    p.version = "10.0.1.2";
    p.arch = TargetSystem::Arch::x64;
    p.downloadUrl = "https://example.test/rt.zip";
    return p;
}
}

class TestCacheId : public QObject {
    Q_OBJECT
private slots:
    void deterministic() {
        QCOMPARE(DriverCache::packageId(pkg()), DriverCache::packageId(pkg()));
    }

    void sixteenHexNoSeparators() {
        const std::string id = DriverCache::packageId(pkg());
        QCOMPARE(id.size(), size_t(16));
        for (char c : id) {
            QVERIFY((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
        }
    }

    void changesWithVersion() {
        DriverPackage a = pkg();
        DriverPackage b = pkg();
        b.version = "10.0.1.3";
        QVERIFY(DriverCache::packageId(a) != DriverCache::packageId(b));
    }

    void changesWithUrl() {
        DriverPackage a = pkg();
        DriverPackage b = pkg();
        b.downloadUrl = "https://example.test/rt2.zip";
        QVERIFY(DriverCache::packageId(a) != DriverCache::packageId(b));
    }

    void changesWithArch() {
        DriverPackage a = pkg();
        DriverPackage b = pkg();
        b.arch = TargetSystem::Arch::x86;
        QVERIFY(DriverCache::packageId(a) != DriverCache::packageId(b));
    }
};

QTEST_MAIN(TestCacheId)
#include "test_cacheid.moc"

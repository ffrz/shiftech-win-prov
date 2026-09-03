#include <QtTest>
#include <QTemporaryDir>
#include "../../src/core/drivers/DriverCache.h"
#include "../../src/core/drivers/DriverDownloader.h"
#include "../../src/core/drivers/LocalCacheProvider.h"
#include "../../src/core/drivers/DriverMatch.h"

using namespace shiftech::core::drivers;
using namespace shiftech::core::hardware;

class TestLocalCacheProvider : public QObject {
    Q_OBJECT

    // Populate a cache dir with one package served for a known hardware id.
    void seed(const QString& cacheDir, const QString& srcDir) {
        QDir().mkpath(srcDir);
        QFile f(QDir(srcDir).filePath("src.zip"));
        f.open(QIODevice::WriteOnly);
        f.write("SEED-PACKAGE-BYTES");
        f.close();

        DriverPackage p;
        p.provider = "Realtek";
        p.driverName = "Realtek NIC";
        p.version = "2.1.0.0";
        p.arch = TargetSystem::Arch::x64;
        p.supportedOs = {"win10", "win11"};
        p.downloadUrl = QUrl::fromLocalFile(f.fileName()).toString().toStdString();
        p.packageType = PackageType::InfZip;

        DriverCache cache(cacheDir);
        DriverDownloader dl(cache);
        DownloadResult r = dl.fetch(p, {"PCI\\VEN_10EC&DEV_8168"});
        QVERIFY2(r.ok, r.error.c_str());
    }

private slots:
    void resolvesKnownIdOffline() {
        QTemporaryDir tmp;
        const QString cacheDir = tmp.filePath("cache");
        seed(cacheDir, tmp.filePath("src"));

        LocalCacheProvider provider{DriverCache(cacheDir)};
        Device dev;
        dev.hardwareIds = {"PCI\\VEN_10EC&DEV_8168"};
        TargetSystem target;
        target.os = TargetSystem::OsFamily::Win11;
        target.arch = TargetSystem::Arch::x64;

        DriverSearchResult r = provider.search(dev, target);
        QVERIFY2(r.found, r.notFoundReason.c_str());
        QCOMPARE(r.candidates.size(), size_t(1));
        QCOMPARE(r.candidates[0].matchedVia, MatchVia::HardwareId);
        // downloadUrl now points into the cache
        QVERIFY(QString::fromStdString(r.candidates[0].downloadUrl).startsWith("file:"));

        auto best = pickBest(dev, r, target);
        QVERIFY(best.has_value());
    }

    void missesUnknownId() {
        QTemporaryDir tmp;
        const QString cacheDir = tmp.filePath("cache");
        seed(cacheDir, tmp.filePath("src"));

        LocalCacheProvider provider{DriverCache(cacheDir)};
        Device dev;
        dev.hardwareIds = {"PCI\\VEN_DEAD&DEV_BEEF"};
        DriverSearchResult r = provider.search(dev, {});
        QVERIFY(!r.found);
    }

    void survivesCacheDirMove() {
        QTemporaryDir tmp;
        const QString a = tmp.filePath("loc_a");
        seed(a, tmp.filePath("src"));

        // Move the whole cache tree to a new path and resolve from there.
        const QString b = tmp.filePath("loc_b");
        QVERIFY(QDir().rename(a, b));

        LocalCacheProvider provider{DriverCache(b)};
        Device dev;
        dev.hardwareIds = {"PCI\\VEN_10EC&DEV_8168"};
        DriverSearchResult r = provider.search(dev, {});
        QVERIFY2(r.found, r.notFoundReason.c_str());
    }
};

QTEST_MAIN(TestLocalCacheProvider)
#include "test_localcacheprovider.moc"

#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include "../../src/core/drivers/PackageExtractor.h"

using namespace shiftech::core::drivers;

class TestPackageExtractor : public QObject {
    Q_OBJECT
private slots:
    void folderPayloadUsedInPlace() {
        QTemporaryDir tmp;
        const QString src = tmp.filePath("src");
        QDir().mkpath(src + "/sub");
        QFile a(src + "/driver.inf");
        a.open(QIODevice::WriteOnly); a.write("[Version]\n"); a.close();
        QFile b(src + "/sub/other.inf");
        b.open(QIODevice::WriteOnly); b.write("[Version]\n"); b.close();

        PackageExtractor ex;
        auto r = ex.extract(src, tmp.filePath("dest"));
        QVERIFY2(r.ok, r.error.c_str());
        auto infs = PackageExtractor::findInfFiles(r.extractedDir);
        QCOMPARE(infs.size(), size_t(2));
    }

    void bareInfCopied() {
        QTemporaryDir tmp;
        QFile inf(tmp.filePath("x.inf"));
        inf.open(QIODevice::WriteOnly); inf.write("[Version]\n"); inf.close();

        PackageExtractor ex;
        auto r = ex.extract(inf.fileName(), tmp.filePath("dest"));
        QVERIFY2(r.ok, r.error.c_str());
        QCOMPARE(PackageExtractor::findInfFiles(r.extractedDir).size(), size_t(1));
    }

    void zipExtracted() {
        QTemporaryDir tmp;
        // Build a small zip with tar (bsdtar) if available; skip otherwise.
        const QString stage = tmp.filePath("stage");
        QDir().mkpath(stage);
        QFile f(stage + "/net.inf");
        f.open(QIODevice::WriteOnly); f.write("[Version]\nSignature=\"$Windows NT$\"\n"); f.close();

        const QString zip = tmp.filePath("pkg.zip");
        QProcess p;
        p.start("tar", {"-a", "-c", "-f", QDir::toNativeSeparators(zip), "-C",
                        QDir::toNativeSeparators(stage), "net.inf"});
        if (!p.waitForFinished(20000) || p.exitCode() != 0 || !QFileInfo::exists(zip)) {
            QSKIP("tar/zip not available on this host");
        }

        PackageExtractor ex;
        auto r = ex.extract(zip, tmp.filePath("dest"));
        QVERIFY2(r.ok, r.error.c_str());
        QCOMPARE(PackageExtractor::findInfFiles(r.extractedDir).size(), size_t(1));
    }

    void missingPayloadFails() {
        QTemporaryDir tmp;
        PackageExtractor ex;
        auto r = ex.extract(tmp.filePath("nope.zip"), tmp.filePath("dest"));
        QVERIFY(!r.ok);
    }
};

QTEST_MAIN(TestPackageExtractor)
#include "test_packageextractor.moc"

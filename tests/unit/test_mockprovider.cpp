#include <QtTest>
#include "../../src/core/drivers/MockDriverProvider.h"
#include <QCoreApplication>
#include <QDir>

using namespace shiftech::core::hardware;
using namespace shiftech::core::drivers;

class TestMockProvider : public QObject {
    Q_OBJECT
private slots:
    void testKnownId() {
        QString idx = QString(FIXTURES_DIR) + "/driver_index.json";
        MockDriverProvider provider(idx.toStdString());

        Device dev;
        dev.hardwareIds = {"PCI\\VEN_10EC&DEV_8168"};

        TargetSystem target;
        
        DriverSearchResult res = provider.search(dev, target);
        QVERIFY2(res.found, res.notFoundReason.c_str());
        QCOMPARE(res.candidates.size(), 2);
        QCOMPARE(res.candidates[0].version.c_str(), "1.0.0.0");
        QCOMPARE(res.candidates[1].version.c_str(), "2.0.0.0");
    }

    void testUnknownId() {
        QString idx = QString(FIXTURES_DIR) + "/driver_index.json";
        MockDriverProvider provider(idx.toStdString());

        Device dev;
        dev.hardwareIds = {"UNKNOWN_ID"};

        TargetSystem target;
        
        DriverSearchResult res = provider.search(dev, target);
        QVERIFY(!res.found);
        QCOMPARE(res.candidates.size(), 0);
        QVERIFY(!res.notFoundReason.empty());
    }

    void testCompatibleIdMatchIsTagged() {
        QString idx = QString(FIXTURES_DIR) + "/driver_index.json";
        MockDriverProvider provider(idx.toStdString());

        Device dev;
        dev.compatibleIds = {"PCI\\VEN_8086&CC_0300"};

        TargetSystem target;
        DriverSearchResult res = provider.search(dev, target);
        QVERIFY2(res.found, res.notFoundReason.c_str());
        QCOMPARE(res.candidates.size(), 1);
        QCOMPARE(res.candidates[0].matchedVia, MatchVia::CompatibleId);
    }

    void testHardwareIdMatchIsTagged() {
        QString idx = QString(FIXTURES_DIR) + "/driver_index.json";
        MockDriverProvider provider(idx.toStdString());

        Device dev;
        dev.hardwareIds = {"PCI\\VEN_10EC&DEV_8168"};

        TargetSystem target;
        DriverSearchResult res = provider.search(dev, target);
        QVERIFY(res.found);
        QCOMPARE(res.candidates[0].matchedVia, MatchVia::HardwareId);
    }

    void testMissingIndexFile() {
        MockDriverProvider provider("non_existent_file.json");

        Device dev;
        dev.hardwareIds = {"PCI\\VEN_10EC&DEV_8168"};

        TargetSystem target;
        
        DriverSearchResult res = provider.search(dev, target);
        QVERIFY(!res.found);
        QCOMPARE(res.candidates.size(), 0);
        QCOMPARE(res.notFoundReason.c_str(), "Mock index file not found");
    }
};

QTEST_MAIN(TestMockProvider)
#include "test_mockprovider.moc"

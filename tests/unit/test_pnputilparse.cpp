#include <QtTest>
#include "../../src/core/drivers/PnpUtilOutput.h"

using namespace shiftech::core::drivers::pnputil;

class TestPnpUtilParse : public QObject {
    Q_OBJECT
private slots:
    void successExit0() {
        const std::string o =
            "Microsoft PnP Utility\n\n"
            "Adding driver package:  rt_nic.inf\n"
            "Driver package added successfully.\n"
            "Published Name:          oem42.inf\n";
        auto r = parseAddDriver(0, o);
        QVERIFY(r.succeeded);
        QVERIFY(!r.rebootRequired);
        QCOMPARE(r.publishedName.c_str(), "oem42.inf");
    }

    void rebootExit3010() {
        auto r = parseAddDriver(3010, "Driver package added successfully.\n");
        QVERIFY(r.succeeded);
        QVERIFY(r.rebootRequired);
    }

    void rebootFromText() {
        auto r = parseAddDriver(0, "Total driver packages:  1\nReboot required to complete.\n");
        QVERIFY(r.succeeded);
        QVERIFY(r.rebootRequired);
    }

    void failureNonZero() {
        auto r = parseAddDriver(5, "Adding driver package failed. Access denied.\n");
        QVERIFY(!r.succeeded);
    }

    void weirdRebootCode() {
        auto r = parseAddDriver(1073807364, "");
        QVERIFY(r.succeeded);
        QVERIFY(r.rebootRequired);
    }
};

QTEST_MAIN(TestPnpUtilParse)
#include "test_pnputilparse.moc"

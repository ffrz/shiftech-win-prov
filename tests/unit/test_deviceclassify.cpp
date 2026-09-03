#include <QtTest>
#include "../../src/core/hardware/Device.h"

using namespace shiftech::core::hardware;

class TestDeviceClassify : public QObject {
    Q_OBJECT
private slots:
    void testNeedsDriver() {
        Device d;

        // NoDriver -> true
        d.status = DeviceStatus::NoDriver;
        QCOMPARE(d.needsDriver(), true);

        // Unknown -> true
        d.status = DeviceStatus::Unknown;
        QCOMPARE(d.needsDriver(), true);

        // Problem(28) -> true
        d.status = DeviceStatus::Problem;
        d.problemCode = 28;
        QCOMPARE(d.needsDriver(), true);

        // Problem(22 disabled) -> false
        d.status = DeviceStatus::Problem;
        d.problemCode = 22;
        QCOMPARE(d.needsDriver(), false);
        
        // Disabled -> false
        d.status = DeviceStatus::Disabled;
        d.problemCode = 22;
        QCOMPARE(d.needsDriver(), false);

        // Ok -> false
        d.status = DeviceStatus::Ok;
        d.problemCode = 0;
        QCOMPARE(d.needsDriver(), false);
    }
};

QTEST_MAIN(TestDeviceClassify)
#include "test_deviceclassify.moc"

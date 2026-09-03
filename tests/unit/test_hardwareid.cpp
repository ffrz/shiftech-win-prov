#include <QtTest>
#include "../../src/core/hardware/HardwareId.h"

using namespace shiftech::core::hardware;

class TestHardwareId : public QObject {
    Q_OBJECT
private slots:
    void parsePci() {
        PnpId id = parseHardwareId("PCI\\VEN_10EC&DEV_8168&SUBSYS_12345678&REV_06");
        QCOMPARE(id.bus.c_str(), "PCI");
        QCOMPARE(id.vendor.c_str(), "10EC");
        QCOMPARE(id.device.c_str(), "8168");
        QCOMPARE(id.subsys.c_str(), "12345678");
        QCOMPARE(id.rev.c_str(), "06");
        QCOMPARE(id.raw.c_str(), "PCI\\VEN_10EC&DEV_8168&SUBSYS_12345678&REV_06");
    }

    void parseUsb() {
        PnpId id = parseHardwareId("USB\\VID_0BDA&PID_8153&REV_3000");
        QCOMPARE(id.bus.c_str(), "USB");
        QCOMPARE(id.vendor.c_str(), "0BDA");
        QCOMPARE(id.device.c_str(), "8153");
        QCOMPARE(id.rev.c_str(), "3000");
    }

    void parseAcpi() {
        PnpId id = parseHardwareId("ACPI\\PNP0A0A");
        QCOMPARE(id.bus.c_str(), "ACPI");
        QCOMPARE(id.device.c_str(), "PNP0A0A");
    }

    void parseHdAudio() {
        PnpId id = parseHardwareId("HDAUDIO\\FUNC_01&VEN_10EC&DEV_0280&SUBSYS_103C8736");
        QCOMPARE(id.bus.c_str(), "HDAUDIO");
        QCOMPARE(id.vendor.c_str(), "10EC");
        QCOMPARE(id.device.c_str(), "0280");
        QCOMPARE(id.subsys.c_str(), "103C8736");
    }

    void parseEmpty() {
        PnpId id = parseHardwareId("");
        QCOMPARE(id.bus.c_str(), "");
        QCOMPARE(id.raw.c_str(), "");
    }

    void parseGarbage() {
        PnpId id = parseHardwareId("GARBAGE_ID_NO_FORMAT");
        QCOMPARE(id.bus.c_str(), "");
        QCOMPARE(id.vendor.c_str(), "");
        QCOMPARE(id.raw.c_str(), "GARBAGE_ID_NO_FORMAT");
    }
};

QTEST_MAIN(TestHardwareId)
#include "test_hardwareid.moc"

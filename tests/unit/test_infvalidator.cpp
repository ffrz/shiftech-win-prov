#include <QtTest>
#include "../../src/core/drivers/InfValidator.h"

using namespace shiftech::core::drivers;

namespace {
std::string fx(const char* n) { return std::string(FIXTURES_DIR) + "/inf/" + n; }
}

class TestInfValidator : public QObject {
    Q_OBJECT
private slots:
    void goodSignedIsOk() {
        auto v = validateInf(fx("good_signed.inf"));
        QCOMPARE(v.verdict, InfVerdict::Ok);
        QVERIFY(v.hasCatalog);
        QCOMPARE(v.className.c_str(), "Net");
    }

    void unsignedIsWarn() {
        auto v = validateInf(fx("unsigned_no_catalog.inf"));
        QCOMPARE(v.verdict, InfVerdict::Warn);
        QVERIFY(!v.hasCatalog);
    }

    void noVersionIsReject() {
        auto v = validateInf(fx("no_version.inf"));
        QCOMPARE(v.verdict, InfVerdict::Reject);
    }

    void runOnceExeIsWarn() {
        auto v = validateInf(fx("suspicious_exe.inf"));
        QCOMPARE(v.verdict, InfVerdict::Warn);
        QVERIFY(!v.messages.empty());
    }

    void missingFileIsReject() {
        auto v = validateInf(fx("does_not_exist.inf"));
        QCOMPARE(v.verdict, InfVerdict::Reject);
    }

    void emptyTextIsReject() {
        QCOMPARE(validateInfText("").verdict, InfVerdict::Reject);
    }
};

QTEST_MAIN(TestInfValidator)
#include "test_infvalidator.moc"

#include <QtTest>

class TestStub : public QObject {
    Q_OBJECT
private slots:
    void stub() { QVERIFY(true); }
};

QTEST_MAIN(TestStub)
#include "tests_stub.moc"

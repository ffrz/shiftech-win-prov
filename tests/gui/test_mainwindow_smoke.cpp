#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include "../../src/gui/MainWindow.h"

using namespace shiftech::gui;

// Smoke test: the window builds, and synthetic engine signals move the widgets.
// No real ProvisioningEngine is run here.
class TestMainWindowSmoke : public QObject {
    Q_OBJECT
private slots:
    void buildsAndReactsToSignals() {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        auto* driverBar = w.findChild<QProgressBar*>();
        QVERIFY(driverBar);
        auto* currentTask = w.findChildren<QLabel*>().value(1); // system label is [0]
        QVERIFY(currentTask);
        auto* log = w.findChild<QPlainTextEdit*>();
        QVERIFY(log);

        // The controller is a private member; drive the window through its public slots
        // by invoking them the way the signal/slot connections would.
        QMetaObject::invokeMethod(&w, "onStageChanged", Qt::DirectConnection,
                                  Q_ARG(QString, "DriverInstall"));
        QMetaObject::invokeMethod(&w, "onProgress", Qt::DirectConnection,
                                  Q_ARG(QString, "driver"), Q_ARG(int, 42));
        QMetaObject::invokeMethod(
            &w, "onLogEvent", Qt::DirectConnection, Q_ARG(QString, "2026-01-01T10:00:00Z"),
            Q_ARG(int, 0), Q_ARG(QString, "driver"), Q_ARG(QString, "resolving X"),
            Q_ARG(int, 42));

        QTest::qWait(50);
        QCOMPARE(driverBar->value(), 42);
        QVERIFY(log->toPlainText().contains("resolving X"));

        QMetaObject::invokeMethod(&w, "onFinished", Qt::DirectConnection,
                                  Q_ARG(QString, "SUCCESS"),
                                  Q_ARG(QString, "Provisioning Complete\nStatus: SUCCESS\n"),
                                  Q_ARG(QString, QString()));
        QTest::qWait(50);
        auto* saveBtn = [&]() -> QPushButton* {
            for (auto* b : w.findChildren<QPushButton*>())
                if (b->text().startsWith("Save report")) return b;
            return nullptr;
        }();
        QVERIFY(saveBtn);
        QVERIFY(saveBtn->isEnabled());
    }
};

QTEST_MAIN(TestMainWindowSmoke)
#include "test_mainwindow_smoke.moc"

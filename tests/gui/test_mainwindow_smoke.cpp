#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include "../../src/gui/MainWindow.h"
#include "../../src/gui/ChecklistTabs.h"
#include "../../src/core/profiles/Profile.h"

using namespace shiftech::gui;
using namespace shiftech::core::profiles;

class TestMainWindowSmoke : public QObject {
    Q_OBJECT
private slots:
    void windowBuildsWithThreeTabs() {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        auto* tabs = w.findChild<QTabWidget*>();
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->tabText(0), QString("Drivers"));
        QCOMPARE(tabs->tabText(1), QString("Applications"));
        QCOMPARE(tabs->tabText(2), QString("Config"));
    }

    void checklistSeedsFromProfile() {
        ChecklistTabs t;
        Profile p;
        p.name = "fixture";
        p.description = "d";
        p.drivers.enabled = true;
        p.drivers.providerOrder = "localcache";
        AppEntry a;
        a.id = "chrome";
        a.source = AppSource::WinGet;
        a.wingetId = "Google.Chrome";
        a.enabled = true;
        p.applications.push_back(a);
        ConfigEntry c;
        c.id = "show-file-extensions";
        c.enabled = true;
        p.config.push_back(c);

        t.seed(p);
        const Profile eff = t.effectiveProfile();

        // apps: at least the seeded one, checked
        bool sawChrome = false;
        for (const auto& e : eff.applications)
            if (e.id == "chrome") { sawChrome = true; QVERIFY(e.enabled); }
        QVERIFY(sawChrome);

        // config: show-file-extensions checked, some other catalog tweak unchecked
        bool sawSFE = false, sawUnchecked = false;
        for (const auto& e : eff.config) {
            if (e.id == "show-file-extensions") { sawSFE = true; QVERIFY(e.enabled); }
            else if (!e.enabled) sawUnchecked = true;
        }
        QVERIFY(sawSFE);
        QVERIFY(sawUnchecked);

        QCOMPARE(eff.drivers.providerOrder.c_str(), "localcache");
    }

    void engineSignalsUpdateWidgets() {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        QMetaObject::invokeMethod(&w, "onProgress", Qt::DirectConnection,
                                  Q_ARG(QString, "driver"), Q_ARG(int, 42));
        QMetaObject::invokeMethod(
            &w, "onLogEvent", Qt::DirectConnection, Q_ARG(QString, "2026-01-01T10:00:00Z"),
            Q_ARG(int, 0), Q_ARG(QString, "driver"), Q_ARG(QString, "resolving X"),
            Q_ARG(int, 42));
        QTest::qWait(50);

        auto* bar = w.findChild<QProgressBar*>();
        QVERIFY(bar);
        QCOMPARE(bar->value(), 42);
        auto* log = w.findChild<QPlainTextEdit*>();
        QVERIFY(log && log->toPlainText().contains("resolving X"));
    }
};

QTEST_MAIN(TestMainWindowSmoke)
#include "test_mainwindow_smoke.moc"

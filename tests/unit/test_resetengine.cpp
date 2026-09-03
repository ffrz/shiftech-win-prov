#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include "../../src/core/provisioning/ResetEngine.h"
#include "../../src/core/provisioning/ProvisioningState.h"
#include "../../src/core/provisioning/ReportBuilder.h"

using namespace shiftech::core::provisioning;

namespace {
// Write a minimal logs/<runId>/run.json describing a run that installed some things.
QString writeFakeRun(const QString& logDir, const QString& runId) {
    ProvisioningState st;
    st.runId = runId.toStdString();
    st.stage = Stage::Done;

    DriverItemResult d;
    d.instanceId = "INST\\1";
    d.deviceName = "Test NIC";
    d.status = "Installed";
    d.publishedInfs = {"oem42.inf"};
    st.drivers.push_back(d);

    AppItemResult a1;
    a1.id = "chrome"; a1.status = "installed"; a1.source = "winget"; a1.wingetId = "Google.Chrome";
    st.apps.push_back(a1);
    AppItemResult a2;
    a2.id = "7zip"; a2.status = "already_installed"; a2.source = "winget"; a2.wingetId = "7zip.7zip";
    st.apps.push_back(a2);              // pre-existing -> reset must NOT touch it
    AppItemResult a3;
    a3.id = "winrar"; a3.status = "installed"; a3.source = "local";
    st.apps.push_back(a3);

    st.configTweaks.push_back({"show-file-extensions", "Applied", ""});
    st.configTweaks.push_back({"clean-taskbar-pins", "Applied", ""});
    st.configTweaks.push_back({"disable-hibernate", "AlreadyApplied", ""}); // not applied by us

    const QString dir = QDir(logDir).filePath(runId);
    QDir().mkpath(dir);
    QJsonObject root;
    root["runId"] = runId;
    root["state"] = st.toJson();
    root["report"] = buildReport(st).toJson();
    QFile f(QDir(dir).filePath("run.json"));
    f.open(QIODevice::WriteOnly);
    f.write(QJsonDocument(root).toJson());
    f.close();
    return dir;
}
}

class TestResetEngine : public QObject {
    Q_OBJECT
private slots:
    void dryRunListsWhatWouldBeUndone() {
        QTemporaryDir tmp;
        writeFakeRun(tmp.path(), "2026-01-01_000000");

        ResetOptions o;
        o.logDir = tmp.path();
        o.dryRun = true;

        ResetEngine e;
        ResetResult r = e.run(o);
        QVERIFY(r.fatalError.empty());
        QCOMPARE(r.runId.c_str(), "2026-01-01_000000");

        // apps: chrome (winget, installed) + winrar (local, installed); NOT 7zip
        auto has = [&](const std::string& cat, const std::string& name) {
            for (const auto& it : r.items)
                if (it.category == cat && it.name == name) return true;
            return false;
        };
        QVERIFY(has("app", "chrome"));
        QVERIFY(has("app", "winrar"));
        QVERIFY(!has("app", "7zip"));

        // driver: the published oem42.inf
        QVERIFY(has("driver", "oem42.inf"));

        // config: the two we applied; NOT the already-applied one
        QVERIFY(has("config", "show-file-extensions"));
        QVERIFY(has("config", "clean-taskbar-pins"));
        QVERIFY(!has("config", "disable-hibernate"));

        // dry run => nothing actually removed/reverted
        QCOMPARE(r.removed, 0);
        QCOMPARE(r.reverted, 0);
    }

    void missingRunIsFatal() {
        QTemporaryDir tmp;
        ResetOptions o;
        o.logDir = tmp.path();
        o.runIdOrPath = "nope";
        ResetEngine e;
        ResetResult r = e.run(o);
        QVERIFY(!r.fatalError.empty());
    }

    void noRunsIsFatal() {
        QTemporaryDir tmp;
        ResetOptions o;
        o.logDir = tmp.path();
        ResetEngine e;
        ResetResult r = e.run(o);
        QVERIFY(!r.fatalError.empty());
    }

    void skipFlagsHonoured() {
        QTemporaryDir tmp;
        writeFakeRun(tmp.path(), "2026-02-02_000000");
        ResetOptions o;
        o.logDir = tmp.path();
        o.dryRun = true;
        o.skipApps = true;
        o.skipConfig = true;

        ResetEngine e;
        ResetResult r = e.run(o);
        for (const auto& it : r.items)
            QVERIFY(it.category == "driver");   // only drivers left
    }
};

QTEST_MAIN(TestResetEngine)
#include "test_resetengine.moc"

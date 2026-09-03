#include <QtTest>
#include <QTemporaryDir>
#include "../../src/core/applications/LocalInstallerProvider.h"

using namespace shiftech::core::applications;

class TestLocalInstaller : public QObject {
    Q_OBJECT

    QString makeApp(const QString& root, const QString& id, const QString& manifest,
                    const QString& installerName = "setup.exe") {
        const QString dir = QDir(root).filePath(id);
        QDir().mkpath(dir);
        QFile inst(QDir(dir).filePath(installerName));
        inst.open(QIODevice::WriteOnly); inst.write("MZ"); inst.close();
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly); m.write(manifest.toUtf8()); m.close();
        return dir;
    }

private slots:
    void parsesValidManifest() {
        QTemporaryDir tmp;
        makeApp(tmp.path(), "winrar", R"({
          "name": "WinRAR",
          "installer": "setup.exe",
          "silentArgs": ["/S"],
          "detect": { "type": "registry", "keys": ["HKLM\\SOFTWARE\\WinRAR"] },
          "expectedExitCodes": [0, 3010]
        })");

        std::string err;
        auto m = loadLocalAppManifest(QDir(tmp.path()).filePath("winrar"), err);
        QVERIFY2(m.has_value(), err.c_str());
        QCOMPARE(m->id.c_str(), "winrar");
        QCOMPARE(m->name.c_str(), "WinRAR");
        QCOMPARE(m->silentArgs.size(), size_t(1));
        QCOMPARE(m->detectType.c_str(), "registry");
        QCOMPARE(m->expectedExitCodes.size(), size_t(2));
    }

    void rejectsNonExeInstaller() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("bad");
        QDir().mkpath(dir);
        QFile s(QDir(dir).filePath("install.bat"));
        s.open(QIODevice::WriteOnly); s.write("echo hi"); s.close();
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly);
        m.write(R"({ "name": "Bad", "installer": "install.bat" })");
        m.close();

        std::string err;
        auto r = loadLocalAppManifest(dir, err);
        QVERIFY(!r.has_value());
        QVERIFY(QString::fromStdString(err).contains(".exe or .msi"));
    }

    void missingInstallerFileRejected() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("gone");
        QDir().mkpath(dir);
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly);
        m.write(R"({ "name": "Gone", "installer": "nowhere.exe" })");
        m.close();

        std::string err;
        QVERIFY(!loadLocalAppManifest(dir, err).has_value());
    }

    void missingManifestRejected() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("empty");
        QDir().mkpath(dir);
        std::string err;
        QVERIFY(!loadLocalAppManifest(dir, err).has_value());
    }

    void availableListsAppsWithManifest() {
        QTemporaryDir tmp;
        makeApp(tmp.path(), "a", R"({ "name":"A", "installer":"setup.exe" })");
        makeApp(tmp.path(), "b", R"({ "name":"B", "installer":"setup.exe" })");
        QDir().mkpath(QDir(tmp.path()).filePath("c_no_manifest"));

        LocalInstallerProvider p(tmp.path());
        QCOMPARE(p.available().size(), size_t(2));
    }

    void isInstalledFalseForMissingApp() {
        QTemporaryDir tmp;
        LocalInstallerProvider p(tmp.path());
        QCOMPARE(p.isInstalled("nope"), false);
    }
};

QTEST_MAIN(TestLocalInstaller)
#include "test_localinstaller.moc"

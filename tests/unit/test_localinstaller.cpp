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

    void parsesPortableManifest() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("aact");
        QDir().mkpath(dir);
        QFile arc(QDir(dir).filePath("t.7z"));
        arc.open(QIODevice::WriteOnly); arc.write("7z\xBC\xAF\x27\x1C"); arc.close();
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly);
        m.write(R"({
          "name": "AAct", "kind": "portable",
          "archive": "t.7z", "extractTo": "%DESKTOP%\\AAct",
          "flattenSingleRoot": true, "shortcutExe": "AAct_x64.exe",
          "detect": { "type": "folder", "keys": ["%DESKTOP%\\AAct"] }
        })");
        m.close();

        std::string err;
        auto man = loadLocalAppManifest(dir, err);
        QVERIFY2(man.has_value(), err.c_str());
        QVERIFY(man->kind == LocalAppKind::Portable);
        QCOMPARE(man->archiveFile.c_str(), "t.7z");
        QVERIFY(man->flattenSingleRoot);
        QCOMPARE(man->shortcutExe.c_str(), "AAct_x64.exe");
        QCOMPARE(man->detectType.c_str(), "folder");
    }

    void portableRejectsNonArchive() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("bad");
        QDir().mkpath(dir);
        QFile a(QDir(dir).filePath("x.exe")); a.open(QIODevice::WriteOnly); a.write("MZ"); a.close();
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly);
        m.write(R"({ "name":"Bad", "kind":"portable", "archive":"x.exe", "extractTo":"%DESKTOP%\\x" })");
        m.close();
        std::string err;
        QVERIFY(!loadLocalAppManifest(dir, err).has_value());
    }

    void parsesIsoManifest() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("office-2016");
        QDir().mkpath(dir);
        QFile iso(QDir(dir).filePath("o.iso"));
        iso.open(QIODevice::WriteOnly); iso.write("CD001"); iso.close();
        QFile cfg(QDir(dir).filePath("config.xml"));
        cfg.open(QIODevice::WriteOnly); cfg.write("<Configuration/>"); cfg.close();
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly);
        m.write(R"({
          "name": "Office 2016", "kind": "iso",
          "image": "o.iso", "setup": "setup.exe",
          "setupArgs": ["/config", "%APP%\\config.xml"],
          "detect": { "type": "registry",
                      "keys": ["HKLM\\SOFTWARE\\Microsoft\\Office\\16.0\\Common\\InstallRoot"] },
          "expectedExitCodes": [0, 3010]
        })");
        m.close();

        std::string err;
        auto man = loadLocalAppManifest(dir, err);
        QVERIFY2(man.has_value(), err.c_str());
        QVERIFY(man->kind == LocalAppKind::Iso);
        QCOMPARE(man->imageFile.c_str(), "o.iso");
        QCOMPARE(man->isoSetup.c_str(), "setup.exe");
        QVERIFY(!man->isoSetupFromApp);
        QCOMPARE(man->isoSetupArgs.size(), size_t(2));
        QCOMPARE(man->isoExitCodes.size(), size_t(2));
    }

    void isoRejectsNonImage() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("bad");
        QDir().mkpath(dir);
        QFile a(QDir(dir).filePath("x.zip")); a.open(QIODevice::WriteOnly); a.write("PK"); a.close();
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly);
        m.write(R"({ "name":"Bad", "kind":"iso", "image":"x.zip" })");
        m.close();
        std::string err;
        QVERIFY(!loadLocalAppManifest(dir, err).has_value());
        QVERIFY(QString::fromStdString(err).contains(".iso or .img"));
    }

    void isoMissingImageRejected() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("gone");
        QDir().mkpath(dir);
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly);
        m.write(R"({ "name":"Gone", "kind":"iso", "image":"nowhere.iso" })");
        m.close();
        std::string err;
        QVERIFY(!loadLocalAppManifest(dir, err).has_value());
        QVERIFY(QString::fromStdString(err).contains("iso file not found"));
    }

    void isoSetupFromAppNeedsTheBundledSetup() {
        QTemporaryDir tmp;
        const QString dir = QDir(tmp.path()).filePath("office-2019");
        QDir().mkpath(dir);
        QFile iso(QDir(dir).filePath("o.iso"));
        iso.open(QIODevice::WriteOnly); iso.write("CD001"); iso.close();
        const char* manifest = R"({
          "name": "Office 2019", "kind": "iso", "image": "o.iso",
          "setup": "setup.exe", "setupFrom": "app",
          "setupArgs": ["/configure", "%APP%\\configuration.xml"]
        })";
        QFile m(QDir(dir).filePath("app.json"));
        m.open(QIODevice::WriteOnly); m.write(manifest); m.close();

        std::string err;
        QVERIFY(!loadLocalAppManifest(dir, err).has_value());  // no bundled setup.exe yet
        QVERIFY(QString::fromStdString(err).contains("setup program not found"));

        QFile s(QDir(dir).filePath("setup.exe"));
        s.open(QIODevice::WriteOnly); s.write("MZ"); s.close();
        auto man = loadLocalAppManifest(dir, err);
        QVERIFY2(man.has_value(), err.c_str());
        QVERIFY(man->isoSetupFromApp);
    }

    void expandsPathTokens() {
        const QString home = QDir::homePath();
        QVERIFY(expandPath("%USERPROFILE%\\Foo").startsWith(home));
        QVERIFY(!expandPath("%DESKTOP%").contains('%'));
    }
};

QTEST_MAIN(TestLocalInstaller)
#include "test_localinstaller.moc"

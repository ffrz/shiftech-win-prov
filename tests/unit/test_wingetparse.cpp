#include <QtTest>
#include "../../src/core/applications/WinGetOutput.h"

using namespace shiftech::core::applications::winget;

class TestWinGetParse : public QObject {
    Q_OBJECT
private slots:
    void notInstalledWhenNoPackageFound() {
        QCOMPARE(listOutputSaysInstalled(
            0, "No installed package found matching input criteria."), false);
    }

    void notInstalledOnNonZeroExit() {
        QCOMPARE(listOutputSaysInstalled(-1, ""), false);
    }

    void installedWhenTablePrinted() {
        const std::string table =
            "Name                 Id              Version   Available Source\n"
            "-----------------------------------------------------------------\n"
            "Google Chrome        Google.Chrome   120.0.0   121.0.0   winget\n";
        QCOMPARE(listOutputSaysInstalled(0, table), true);
    }

    void transientCodeRetries() {
        QCOMPARE(isTransientInstallFailure(static_cast<int>(0x8A150101)), true); // download error
    }

    void permanentCodeDoesNotRetry() {
        QCOMPARE(isTransientInstallFailure(static_cast<int>(0x8A150014)), false); // no applicable installer (made up permanent)
        QCOMPARE(isTransientInstallFailure(0), false);
    }
};

QTEST_MAIN(TestWinGetParse)
#include "test_wingetparse.moc"

#include <QtTest>
#include "../../src/core/config/ConfigTweak.h"

using namespace shiftech::core::config;

class TestConfigTweak : public QObject {
    Q_OBJECT
private slots:
    void catalogIsNonEmptyAndUnique() {
        const auto c = catalog();
        QVERIFY(c.size() >= 10);
        QSet<QString> ids;
        for (const auto& t : c) {
            QVERIFY2(!ids.contains(QString::fromStdString(t.id)), t.id.c_str());
            ids.insert(QString::fromStdString(t.id));
            QVERIFY(!t.title.empty());
            QVERIFY(!t.description.empty());
        }
    }

    void knownTweakLookup() {
        QVERIFY(isKnownTweak("show-file-extensions"));
        QVERIFY(isKnownTweak("set-timezone"));
        QVERIFY(!isKnownTweak("make-computer-fast"));
    }

    void unknownTweakFails() {
        auto r = runTweak("nope", {}, true);
        QCOMPARE(r.outcome, TweakOutcome::Failed);
        QVERIFY(QString::fromStdString(r.detail).contains("unknown"));
    }

    void missingRequiredArgFails() {
        // set-timezone requires "id"
        auto r = runTweak("set-timezone", {}, true);
        QCOMPARE(r.outcome, TweakOutcome::Failed);
        QVERIFY(QString::fromStdString(r.detail).contains("required arg"));
    }

    void needsElevationSkippedWhenNotElevated() {
        // disable-password-expiry needs admin
        auto r = runTweak("disable-password-expiry", {}, /*elevated*/ false);
        QCOMPARE(r.outcome, TweakOutcome::Skipped);
        QVERIFY(QString::fromStdString(r.detail).contains("Administrator"));
    }

    void nonElevatedTweakAllowedUnelevated() {
        // show-file-extensions does NOT need admin; running it here would touch HKCU.
        // Just assert it isn't skipped for elevation reasons (it may report Applied or
        // AlreadyApplied depending on the test machine).
        auto r = runTweak("show-file-extensions", {}, false);
        QVERIFY(r.outcome != TweakOutcome::Skipped);
    }
};

QTEST_MAIN(TestConfigTweak)
#include "test_configtweak.moc"

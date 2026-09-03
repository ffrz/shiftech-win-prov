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

    // NOTE: we deliberately do NOT call runTweak()/revertTweak() for real on a tweak
    // that mutates this machine (registry, power plan, accounts). Those paths are
    // exercised on a VM per docs/tasks/MILESTONE-10.md. Here we only test the guards.

    void revertUnknownFails() {
        auto r = revertTweak("nope", {}, true);
        QCOMPARE(r.outcome, RevertOutcome::Failed);
    }

    void revertElevationGate() {
        auto r = revertTweak("disable-password-expiry", {}, /*elevated*/ false);
        QCOMPARE(r.outcome, RevertOutcome::Skipped);
    }

    void revertNotSupportedForIrreversibleTweaks() {
        auto r = revertTweak("clean-taskbar-pins", {}, false);
        QCOMPARE(r.outcome, RevertOutcome::NotSupported);
    }

    void adminTweaksAreSkippedNotRunWhenUnelevated() {
        // For every catalog tweak that needs admin, an unelevated revert must be a no-op
        // Skip (it must NOT touch the machine).
        for (const auto& t : catalog()) {
            if (!t.needsElevation) continue;
            auto r = revertTweak(t.id, {}, /*elevated*/ false);
            QVERIFY2(r.outcome == RevertOutcome::Skipped, t.id.c_str());
        }
    }
};

QTEST_MAIN(TestConfigTweak)
#include "test_configtweak.moc"

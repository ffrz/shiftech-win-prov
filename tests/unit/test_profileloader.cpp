#include <QtTest>
#include "../../src/core/profiles/ProfileLoader.h"

using namespace shiftech::core::profiles;

namespace {
std::string fixture(const char* name) {
    return std::string(FIXTURES_DIR) + "/profiles/" + name;
}
}

class TestProfileLoader : public QObject {
    Q_OBJECT
private slots:
    void loadsValidProfile() {
        auto r = ProfileLoader::load(fixture("valid.json"));
        QVERIFY(std::holds_alternative<Profile>(r));
        const Profile& p = std::get<Profile>(r);
        QCOMPARE(p.name.c_str(), "valid");

        // drivers section
        QCOMPARE(p.drivers.enabled, true);
        QCOMPARE(p.drivers.providerOrder.c_str(), "localcache,mirror");
        QCOMPARE(p.drivers.exclude.size(), size_t(1));

        // applications: 3 total, 2 enabled
        QCOMPARE(p.applications.size(), size_t(3));
        QCOMPARE(p.enabledApps().size(), size_t(2));
        QCOMPARE(p.applications[0].id.c_str(), "chrome");
        QCOMPARE(p.applications[0].localId.c_str(), "chrome");
        QCOMPARE(p.applications[0].wingetId.c_str(), "Google.Chrome");
        QVERIFY(p.applications[0].hasLocal() && p.applications[0].hasWinget());
        QCOMPARE(p.applications[0].required, true);
        QVERIFY(p.applications[1].hasLocal() && !p.applications[1].hasWinget());
        QVERIFY(!p.applications[2].hasLocal() && p.applications[2].hasWinget());
        QCOMPARE(p.applications[2].enabled, false);

        // config: 2 total, 1 enabled
        QCOMPARE(p.config.size(), size_t(2));
        QCOMPARE(p.enabledConfig().size(), size_t(1));
        QCOMPARE(p.config[1].id.c_str(), "set-timezone");
        QCOMPARE(p.config[1].args.at("id").c_str(), "SE Asia Standard Time");
    }

    void rejectsUnknownTweakId() {
        auto r = ProfileLoader::load(fixture("bad_tweak.json"));
        QVERIFY(std::holds_alternative<ProfileLoadError>(r));
    }

    void rejectsBadAppSource() {
        auto r = ProfileLoader::load(fixture("bad_source.json"));
        QVERIFY(std::holds_alternative<ProfileLoadError>(r));
    }

    void backwardCompatOldFormat() {
        // legacy shapes: bare {id}, and {id, source:"winget"} — both still load
        auto r = ProfileLoader::load(fixture("legacy.json"));
        QVERIFY(std::holds_alternative<Profile>(r));
        const Profile& p = std::get<Profile>(r);
        QVERIFY(p.applications[0].hasWinget());
        QCOMPARE(p.applications[0].wingetId.c_str(), "Google.Chrome"); // falls back to id
    }

    void backwardCompatSourceKey() {
        auto r = ProfileLoader::load(fixture("legacy_source.json"));
        QVERIFY(std::holds_alternative<Profile>(r));
        const Profile& p = std::get<Profile>(r);
        QVERIFY(p.applications[0].hasWinget() && !p.applications[0].hasLocal());
        QCOMPARE(p.applications[0].wingetId.c_str(), "Mozilla.Firefox");
        QVERIFY(p.applications[1].hasLocal() && !p.applications[1].hasWinget());
        QCOMPARE(p.applications[1].localId.c_str(), "winrar");
    }

    void rejectsMissingDescription() {
        auto r = ProfileLoader::load(fixture("missing_description.json"));
        QVERIFY(std::holds_alternative<ProfileLoadError>(r));
    }

    void rejectsDuplicateId() {
        auto r = ProfileLoader::load(fixture("duplicate_id.json"));
        QVERIFY(std::holds_alternative<ProfileLoadError>(r));
    }

    void rejectsUnknownKey() {
        auto r = ProfileLoader::load(fixture("unknown_key.json"));
        QVERIFY(std::holds_alternative<ProfileLoadError>(r));
    }

    void rejectsNameStemMismatch() {
        auto r = ProfileLoader::load(fixture("name_mismatch.json"));
        QVERIFY(std::holds_alternative<ProfileLoadError>(r));
    }

    void rejectsMissingFile() {
        auto r = ProfileLoader::load(fixture("does_not_exist.json"));
        QVERIFY(std::holds_alternative<ProfileLoadError>(r));
    }

    void shippedProfilesAreValid() {
        for (const char* name : {"standard", "office", "technician", "developer"}) {
            const std::string path =
                std::string(SHIPPED_PROFILES_DIR) + "/" + name + ".json";
            auto r = ProfileLoader::load(path);
            QVERIFY2(std::holds_alternative<Profile>(r), name);
        }
    }
};

QTEST_MAIN(TestProfileLoader)
#include "test_profileloader.moc"

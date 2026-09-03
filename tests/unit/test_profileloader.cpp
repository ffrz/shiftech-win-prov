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
        QCOMPARE(p.applications.size(), size_t(2));
        QCOMPARE(p.applications[0].id.c_str(), "Google.Chrome");
        QCOMPARE(p.applications[0].required, true);
        QCOMPARE(p.applications[1].required, false); // default
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

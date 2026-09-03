#include <QtTest>
#include "../../src/core/drivers/ProviderChain.h"
#include "../../src/core/drivers/DriverProviderFactory.h"

using namespace shiftech::core::drivers;
using namespace shiftech::core::hardware;

namespace {

class FakeProvider : public DriverProvider {
public:
    FakeProvider(std::string n, bool found, bool throws = false)
        : m_name(std::move(n)), m_found(found), m_throws(throws) {}

    DriverSearchResult search(const Device&, const TargetSystem&) override {
        if (m_throws) throw std::runtime_error("boom");
        DriverSearchResult r;
        if (m_found) {
            DriverPackage p;
            p.driverName = m_name + "-driver";
            p.arch = TargetSystem::Arch::x64;
            p.supportedOs = {"win10", "win11"};
            r.candidates.push_back(p);
            r.found = true;
        } else {
            r.notFoundReason = m_name + " has nothing";
        }
        return r;
    }
    std::string name() const override { return m_name; }

private:
    std::string m_name;
    bool m_found;
    bool m_throws;
};

} // namespace

class TestProviderChain : public QObject {
    Q_OBJECT
private slots:
    void fallsThroughToFirstHit() {
        ProviderChain c;
        c.add(std::make_unique<FakeProvider>("a", false));
        c.add(std::make_unique<FakeProvider>("b", true));
        c.add(std::make_unique<FakeProvider>("c", true));

        Device d;
        DriverSearchResult r = c.resolve(d, {});
        QVERIFY(r.found);
        QCOMPARE(r.candidates.size(), size_t(1));
        QCOMPARE(r.candidates[0].driverName.c_str(), "b-driver");
    }

    void throwingProviderIsSkipped() {
        ProviderChain c;
        c.add(std::make_unique<FakeProvider>("bad", false, /*throws*/ true));
        c.add(std::make_unique<FakeProvider>("good", true));

        Device d;
        DriverSearchResult r = c.resolve(d, {});
        QVERIFY(r.found);
        QCOMPARE(r.candidates[0].driverName.c_str(), "good-driver");
    }

    void allMissAggregatesReasons() {
        ProviderChain c;
        c.add(std::make_unique<FakeProvider>("a", false));
        c.add(std::make_unique<FakeProvider>("b", false));

        Device d;
        DriverSearchResult r = c.resolve(d, {});
        QVERIFY(!r.found);
        QVERIFY(QString::fromStdString(r.notFoundReason).contains("a has nothing"));
        QVERIFY(QString::fromStdString(r.notFoundReason).contains("b has nothing"));
    }

    void factoryDefaultOrder() {
        std::string err;
        FactoryOptions fo;
        auto chain = buildProviderChain("", fo, err);
        QVERIFY2(chain.has_value(), err.c_str());
        const auto names = chain->names();
        QCOMPARE(names.size(), size_t(3));
        QCOMPARE(names[0].c_str(), "localcache");
        QCOMPARE(names[1].c_str(), "windowsupdate");
        QCOMPARE(names[2].c_str(), "mirror");
    }

    void factoryRejectsDriverpack() {
        std::string err;
        FactoryOptions fo;
        auto chain = buildProviderChain("localcache,driverpack", fo, err);
        QVERIFY(!chain.has_value());
        QVERIFY(QString::fromStdString(err).contains("ADR-0007"));
    }

    void factoryRejectsUnknown() {
        std::string err;
        FactoryOptions fo;
        auto chain = buildProviderChain("bogus", fo, err);
        QVERIFY(!chain.has_value());
    }
};

QTEST_MAIN(TestProviderChain)
#include "test_providerchain.moc"

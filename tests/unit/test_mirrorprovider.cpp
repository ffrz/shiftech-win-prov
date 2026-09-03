#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include "../../src/core/drivers/MirrorProvider.h"

using namespace shiftech::core::drivers;
using namespace shiftech::core::hardware;

// Tiny HTTP server that serves a fixed index.json body.
class MiniMirror : public QObject {
    Q_OBJECT
public:
    QByteArray indexBody;
    int status = 200;

    MiniMirror() {
        server.listen(QHostAddress::LocalHost, 0);
        connect(&server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket* s = server.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, this, [this, s] {
                s->readAll();
                QByteArray h = "HTTP/1.1 " + QByteArray::number(status) +
                               (status == 200 ? " OK\r\n" : " ERR\r\n");
                h += "Content-Length: " + QByteArray::number(indexBody.size()) + "\r\n";
                h += "Connection: close\r\n\r\n";
                s->write(h);
                s->write(indexBody);
                s->flush();
                s->disconnectFromHost();
            });
        });
    }
    QString base() const { return QString("http://127.0.0.1:%1").arg(server.serverPort()); }

private:
    QTcpServer server;
};

class TestMirrorProvider : public QObject {
    Q_OBJECT

    Device nic() const {
        Device d;
        d.hardwareIds = {"PCI\\VEN_10EC&DEV_8168"};
        return d;
    }

private slots:
    void resolvesFromIndex() {
        MiniMirror m;
        m.indexBody = R"({
          "PCI\\VEN_10EC&DEV_8168": [
            { "driverName": "Realtek NIC", "version": "2.0.0.0", "provider": "Realtek",
              "supportedOs": ["win10","win11"], "arch": "x64",
              "path": "realtek/rt.zip", "packageType": "InfZip",
              "checksum": "abc123", "checksumAlgo": "sha256" }
          ]
        })";
        MirrorProvider p(m.base().toStdString());
        auto r = p.search(nic(), {});
        QVERIFY2(r.found, r.notFoundReason.c_str());
        QCOMPARE(r.candidates.size(), size_t(1));
        QCOMPARE(r.candidates[0].downloadUrl.c_str(),
                 (m.base() + "/realtek/rt.zip").toStdString().c_str());
        QCOMPARE(r.candidates[0].checksum.c_str(), "abc123");
        QCOMPARE(r.candidates[0].matchedVia, MatchVia::HardwareId);
    }

    void missUnknownId() {
        MiniMirror m;
        m.indexBody = R"({ "PCI\\VEN_AAAA&DEV_BBBB": [] })";
        MirrorProvider p(m.base().toStdString());
        auto r = p.search(nic(), {});
        QVERIFY(!r.found);
    }

    void malformedIndex() {
        MiniMirror m;
        m.indexBody = "{ this is not json";
        MirrorProvider p(m.base().toStdString());
        auto r = p.search(nic(), {});
        QVERIFY(!r.found);
        QVERIFY(QString::fromStdString(r.notFoundReason).contains("parse"));
    }

    void unconfigured() {
        MirrorProvider p;
        auto r = p.search(nic(), {});
        QVERIFY(!r.found);
        QVERIFY(QString::fromStdString(r.notFoundReason).contains("no mirror configured"));
    }

    void fileUrlBase() {
        QTemporaryDir tmp;
        QFile f(tmp.filePath("index.json"));
        f.open(QIODevice::WriteOnly);
        f.write(R"({ "PCI\\VEN_10EC&DEV_8168": [
            { "driverName": "X", "version": "1.0", "provider": "Y",
              "supportedOs": ["win11"], "arch": "x64", "path": "x.zip",
              "packageType": "InfZip", "checksum": "", "checksumAlgo": "" } ] })");
        f.close();
        MirrorProvider p(QUrl::fromLocalFile(tmp.path()).toString().toStdString());
        auto r = p.search(nic(), {});
        QVERIFY2(r.found, r.notFoundReason.c_str());
    }
};

QTEST_MAIN(TestMirrorProvider)
#include "test_mirrorprovider.moc"

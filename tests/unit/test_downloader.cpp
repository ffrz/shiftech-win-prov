#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include "../../src/core/drivers/DriverDownloader.h"

using namespace shiftech::core::drivers;

// A tiny HTTP/1.1 server for offline download tests. Serves a fixed body, optionally
// honours Range, and can be told to drop the first connection mid-body.
class MiniHttp : public QObject {
    Q_OBJECT
public:
    QByteArray body = "SHIFTECH-TEST-DRIVER-PAYLOAD-0123456789";
    bool honourRange = true;
    int dropFirstNBytes = -1;   // >=0: first request sends only this many bytes then closes
    int requestCount = 0;

    explicit MiniHttp(QObject* parent = nullptr) : QObject(parent) {
        server.listen(QHostAddress::LocalHost, 0);
        connect(&server, &QTcpServer::newConnection, this, &MiniHttp::onConn);
    }
    quint16 port() const { return server.serverPort(); }

private slots:
    void onConn() {
        QTcpSocket* s = server.nextPendingConnection();
        connect(s, &QTcpSocket::readyRead, this, [this, s]() {
            const QByteArray req = s->readAll();
            ++requestCount;
            qint64 start = 0;
            const int rp = req.indexOf("Range: bytes=");
            if (honourRange && rp >= 0) {
                start = req.mid(rp + 13, req.indexOf('\r', rp) - (rp + 13)).split('-')[0].toLongLong();
            }
            QByteArray slice = body.mid(static_cast<int>(start));
            const bool partial = (start > 0);

            QByteArray head;
            head += partial ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
            head += "Content-Length: " + QByteArray::number(slice.size()) + "\r\n";
            if (honourRange) head += "Accept-Ranges: bytes\r\n";
            head += "Connection: close\r\n\r\n";
            s->write(head);

            if (dropFirstNBytes >= 0 && requestCount == 1) {
                s->write(slice.left(dropFirstNBytes));
                s->flush();
                s->disconnectFromHost();
                return;
            }
            s->write(slice);
            s->flush();
            s->disconnectFromHost();
        });
    }

private:
    QTcpServer server;
};

class TestDownloader : public QObject {
    Q_OBJECT

    QString url(const MiniHttp& h) const {
        return QString("http://127.0.0.1:%1/driver.zip").arg(h.port());
    }
    DriverPackage pkg(const QString& u) const {
        DriverPackage p;
        p.provider = "Test";
        p.driverName = "Fixture";
        p.version = "1.0";
        p.arch = TargetSystem::Arch::x64;
        p.downloadUrl = u.toStdString();
        p.packageType = PackageType::InfZip;
        return p;
    }

private slots:
    void downloadsAndCaches() {
        QTemporaryDir tmp;
        MiniHttp http;
        DriverCache cache(tmp.path());
        DriverDownloader dl(cache);

        DriverPackage p = pkg(url(http));
        DownloadResult r1 = dl.fetch(p, {"PCI\\VEN_1&DEV_2"});
        QVERIFY2(r1.ok, r1.error.c_str());
        QVERIFY(!r1.fromCache);
        QVERIFY(QFileInfo::exists(r1.payloadPath));

        // Second fetch => cache hit, no new request.
        const int before = http.requestCount;
        DownloadResult r2 = dl.fetch(p, {"PCI\\VEN_1&DEV_2"});
        QVERIFY(r2.ok);
        QVERIFY(r2.fromCache);
        QCOMPARE(http.requestCount, before);
    }

    void checksumMismatchFails() {
        QTemporaryDir tmp;
        MiniHttp http;
        DriverCache cache(tmp.path());
        DriverDownloader dl(cache);

        DriverPackage p = pkg(url(http));
        p.checksum = "deadbeef"; // wrong
        p.checksumAlgo = "sha256";
        DownloadResult r = dl.fetch(p, {"X"});
        QVERIFY(!r.ok);
        QVERIFY(QString::fromStdString(r.error).contains("checksum"));
    }

    void checksumMatchSucceeds() {
        QTemporaryDir tmp;
        MiniHttp http;
        DriverCache cache(tmp.path());
        DriverDownloader dl(cache);

        DriverPackage p = pkg(url(http));
        p.checksum = QString::fromLatin1(
            QCryptographicHash::hash(http.body, QCryptographicHash::Sha256).toHex()).toStdString();
        p.checksumAlgo = "sha256";
        DownloadResult r = dl.fetch(p, {"X"});
        QVERIFY2(r.ok, r.error.c_str());
    }

    void resumesAfterMidStreamDrop() {
        QTemporaryDir tmp;
        MiniHttp http;
        http.dropFirstNBytes = 10; // first response truncated
        DriverCache cache(tmp.path());
        DownloadOptions opts;
        opts.maxAttempts = 3;
        DriverDownloader dl(cache, opts);

        DriverPackage p = pkg(url(http));
        DownloadResult r = dl.fetch(p, {"X"});
        QVERIFY2(r.ok, r.error.c_str());
        QFile f(r.payloadPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), http.body);
        QVERIFY(http.requestCount >= 2); // needed a retry
    }

    void http404Fails() {
        QTemporaryDir tmp;
        DriverCache cache(tmp.path());
        DownloadOptions opts;
        opts.maxAttempts = 1;
        opts.perAttemptTimeoutMs = 1500;
        DriverDownloader dl(cache, opts);
        // nothing listening on this port
        DriverPackage p = pkg("http://127.0.0.1:1/nope.zip");
        DownloadResult r = dl.fetch(p, {"X"});
        QVERIFY(!r.ok);
    }

    void fileUrlSource() {
        QTemporaryDir tmp;
        QFile src(tmp.filePath("src.zip"));
        QVERIFY(src.open(QIODevice::WriteOnly));
        src.write("LOCAL-FIXTURE-BYTES");
        src.close();

        DriverCache cache(tmp.filePath("cache"));
        DriverDownloader dl(cache);
        DriverPackage p = pkg(QUrl::fromLocalFile(src.fileName()).toString());
        DownloadResult r = dl.fetch(p, {"X"});
        QVERIFY2(r.ok, r.error.c_str());
        QVERIFY(r.payloadUrl.startsWith("file:"));
    }
};

QTEST_MAIN(TestDownloader)
#include "test_downloader.moc"

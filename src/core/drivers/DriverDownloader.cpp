#include "DriverDownloader.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace shiftech::core::drivers {

namespace {

// Choose a payload file name from the URL, defaulting by package type.
QString payloadName(const DriverPackage& pkg) {
    const QUrl u(QString::fromStdString(pkg.downloadUrl));
    const QString base = QFileInfo(u.path()).fileName();
    if (!base.isEmpty() && base.contains('.')) return base;
    switch (pkg.packageType) {
        case PackageType::InfCab: return "package.cab";
        case PackageType::InfFolder: return "package";
        case PackageType::InfZip:
        default: return "package.zip";
    }
}

int backoffMs(int attempt) {  // 1s, 4s, 9s, ...
    return attempt * attempt * 1000;
}

} // namespace

DriverDownloader::DriverDownloader(DriverCache cache, DownloadOptions opts)
    : m_cache(std::move(cache)), m_opts(opts) {}

DownloadResult DriverDownloader::fetch(const DriverPackage& pkg,
                                       const std::vector<std::string>& deviceIds,
                                       const ProgressFn& progress) {
    DownloadResult r;

    // 1. Already in the portable cache and usable?
    if (m_cache.isUsable(pkg)) {
        r.ok = true;
        r.fromCache = true;
        r.payloadUrl = m_cache.cachedPayloadUrl(pkg);
        r.payloadPath = QUrl(r.payloadUrl).toLocalFile();
        r.sha256 = DriverCache::sha256OfFile(r.payloadPath).toStdString();
        return r;
    }

    const std::string id = DriverCache::packageId(pkg);
    const QString dir = m_cache.packageDir(id);
    QDir().mkpath(dir);
    const QString dest = QDir(dir).absoluteFilePath(payloadName(pkg));

    const QUrl url(QString::fromStdString(pkg.downloadUrl));
    DownloadResult dl;
    if (url.isLocalFile() || url.scheme() == "file") {
        dl = copyFile(pkg, url.toLocalFile(), dest);
    } else if (url.scheme() == "http" || url.scheme() == "https") {
        dl = fetchHttp(pkg, dest, progress);
    } else {
        r.error = "unsupported URL scheme: " + url.scheme().toStdString();
        return r;
    }

    if (!dl.ok) return dl;

    // 2. Checksum verification when the package declares one.
    const QString sha = DriverCache::sha256OfFile(dest);
    dl.sha256 = sha.toStdString();
    if (!pkg.checksum.empty()) {
        const QString algo = QString::fromStdString(pkg.checksumAlgo).toLower();
        if (algo.isEmpty() || algo == "sha256") {
            if (sha.toLower() != QString::fromStdString(pkg.checksum).toLower()) {
                QFile::remove(dest);
                dl.ok = false;
                dl.error = "checksum mismatch";
                return dl;
            }
        } else {
            dl.ok = false;
            dl.error = "unsupported checksum algo: " + pkg.checksumAlgo;
            QFile::remove(dest);
            return dl;
        }
    }

    // 3. Record metadata + index entry.
    CachedPackage entry;
    entry.package = pkg;
    entry.deviceIds = deviceIds;
    entry.payloadFileName = QFileInfo(dest).fileName().toStdString();
    entry.sha256 = dl.sha256;
    entry.fetchedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    m_cache.store(entry);

    dl.payloadPath = dest;
    dl.payloadUrl = QUrl::fromLocalFile(dest).toString();
    return dl;
}

DownloadResult DriverDownloader::copyFile(const DriverPackage&, const QString& srcLocalPath,
                                          const QString& destPath) {
    DownloadResult r;
    QFileInfo src(srcLocalPath);
    if (!src.exists()) {
        r.error = "source file not found: " + srcLocalPath.toStdString();
        return r;
    }
    if (src.isDir()) {
        r.error = "folder packages are not downloaded; point downloadUrl at an archive";
        return r;
    }
    QFile::remove(destPath);
    if (!QFile::copy(srcLocalPath, destPath)) {
        r.error = "failed to copy " + srcLocalPath.toStdString();
        return r;
    }
    r.ok = true;
    return r;
}

DownloadResult DriverDownloader::fetchHttp(const DriverPackage& pkg, const QString& destPath,
                                           const ProgressFn& progress) {
    DownloadResult r;
    const QString partPath = destPath + ".part";
    QNetworkAccessManager nam;

    for (int attempt = 1; attempt <= m_opts.maxAttempts; ++attempt) {
        if (attempt > 1) {
            QThread::msleep(static_cast<unsigned long>(backoffMs(attempt - 1)));
        }

        qint64 have = 0;
        if (m_opts.allowResume) {
            QFileInfo pi(partPath);
            if (pi.exists()) have = pi.size();
        } else {
            QFile::remove(partPath);
        }

        QNetworkRequest req{QUrl(QString::fromStdString(pkg.downloadUrl))};
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        if (have > 0) {
            req.setRawHeader("Range", "bytes=" + QByteArray::number(have) + "-");
        }

        QNetworkReply* reply = nam.get(req);

        QFile out(partPath);
        const auto openMode = (have > 0) ? (QIODevice::WriteOnly | QIODevice::Append)
                                         : (QIODevice::WriteOnly | QIODevice::Truncate);
        if (!out.open(openMode)) {
            reply->abort();
            reply->deleteLater();
            r.error = "cannot open " + partPath.toStdString();
            return r;
        }

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        bool timedOut = false;
        QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            reply->abort();
        });
        QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
            out.write(reply->readAll());
        });
        QObject::connect(reply, &QNetworkReply::downloadProgress, &loop,
                         [&](qint64 rcv, qint64 tot) {
                             if (progress) progress(have + rcv, have + tot);
                         });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        timeout.start(m_opts.perAttemptTimeoutMs);
        loop.exec();
        timeout.stop();

        out.write(reply->readAll());
        out.close();

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        reply->deleteLater();

        if (timedOut) {
            r.error = "timeout after " + std::to_string(m_opts.perAttemptTimeoutMs) + "ms";
            continue;
        }
        if (netErr != QNetworkReply::NoError && netErr != QNetworkReply::OperationCanceledError) {
            r.error = "network error " + std::to_string(static_cast<int>(netErr));
            continue;
        }
        // 200 (full) or 206 (partial) are success; a server that ignored Range returns 200
        // and we just overwrote from the start (openMode Truncate only when have==0, so if
        // it returned 200 on a resume we must restart clean).
        if (status == 200 && have > 0) {
            QFile::remove(partPath);
            continue; // retry from scratch
        }
        if (status != 200 && status != 206) {
            r.error = "HTTP " + std::to_string(status);
            continue;
        }

        QFile::remove(destPath);
        if (!QFile::rename(partPath, destPath)) {
            r.error = "cannot finalize " + destPath.toStdString();
            return r;
        }
        r.ok = true;
        return r;
    }

    return r; // r.error holds the last failure
}

} // namespace shiftech::core::drivers

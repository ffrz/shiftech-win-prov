#include "MirrorProvider.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <set>

namespace shiftech::core::drivers {

namespace {

QString joinUrl(const std::string& base, const QString& rel) {
    if (rel.startsWith("http://") || rel.startsWith("https://") || rel.startsWith("file:"))
        return rel;
    QString b = QString::fromStdString(base);
    if (b.endsWith('/')) b.chop(1);
    QString r = rel;
    while (r.startsWith('/')) r = r.mid(1);
    return b + "/" + r;
}

TargetSystem::Arch archFromStr(const QString& s) {
    return s.compare("x86", Qt::CaseInsensitive) == 0 ? TargetSystem::Arch::x86
                                                      : TargetSystem::Arch::x64;
}

PackageType typeFromStr(const QString& s) {
    if (s == "InfZip") return PackageType::InfZip;
    if (s == "InfCab") return PackageType::InfCab;
    if (s == "InfFolder") return PackageType::InfFolder;
    return PackageType::Unknown;
}

// Fetch a URL's body with a timeout. http(s):// and file://.
bool fetch(const QString& url, int timeoutMs, QString& outBody, std::string& err) {
    const QUrl u(url);
    if (u.isLocalFile() || u.scheme() == "file") {
        QFile f(u.toLocalFile());
        if (!f.open(QIODevice::ReadOnly)) {
            err = "cannot open " + u.toLocalFile().toStdString();
            return false;
        }
        outBody = QString::fromUtf8(f.readAll());
        return true;
    }

    QNetworkAccessManager nam;
    QNetworkRequest req{u};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam.get(req);

    QEventLoop loop;
    QTimer t;
    t.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&t, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    t.start(timeoutMs);
    loop.exec();
    t.stop();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto netErr = reply->error();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (timedOut) {
        err = "timeout fetching " + url.toStdString();
        return false;
    }
    if (netErr != QNetworkReply::NoError) {
        err = "network error " + std::to_string(static_cast<int>(netErr));
        return false;
    }
    if (status && status != 200) {
        err = "HTTP " + std::to_string(status);
        return false;
    }
    outBody = QString::fromUtf8(body);
    return true;
}

} // namespace

MirrorProvider::MirrorProvider(std::string baseUrl, int timeoutMs)
    : m_baseUrl(std::move(baseUrl)), m_timeoutMs(timeoutMs) {}

bool MirrorProvider::ensureIndex() {
    if (m_fetched) return m_fetchError.empty();
    m_fetched = true;
    if (m_baseUrl.empty()) {
        m_fetchError = "no mirror configured (--mirror-url)";
        return false;
    }
    QString body;
    if (!fetch(joinUrl(m_baseUrl, "index.json"), m_timeoutMs, body, m_fetchError)) {
        return false;
    }
    m_cachedIndex = body;
    return true;
}

DriverSearchResult MirrorProvider::search(const hardware::Device& device,
                                          const TargetSystem& target) {
    (void)target;
    DriverSearchResult result;

    if (!ensureIndex()) {
        result.notFoundReason = m_fetchError;
        return result;
    }

    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(m_cachedIndex.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        result.notFoundReason = "mirror index.json parse error";
        return result;
    }
    const QJsonObject root = doc.object();

    std::set<QString> seen;
    auto collect = [&](const std::vector<std::string>& ids, MatchVia via) {
        for (const auto& idStd : ids) {
            const QString key = QString::fromStdString(idStd);
            if (!root.contains(key)) continue;
            for (const auto& v : root.value(key).toArray()) {
                const QJsonObject e = v.toObject();
                const QString path = e.value("path").toString();
                if (path.isEmpty()) continue;
                const QString url = joinUrl(m_baseUrl, path);
                if (!seen.insert(url).second) continue;

                DriverPackage p;
                p.driverName = e.value("driverName").toString().toStdString();
                p.version = e.value("version").toString().toStdString();
                p.provider = e.value("provider").toString().toStdString();
                for (const auto& os : e.value("supportedOs").toArray())
                    p.supportedOs.push_back(os.toString().toStdString());
                p.arch = archFromStr(e.value("arch").toString());
                p.downloadUrl = url.toStdString();
                p.packageType = typeFromStr(e.value("packageType").toString());
                p.checksum = e.value("checksum").toString().toStdString();
                p.checksumAlgo = e.value("checksumAlgo").toString().toStdString();
                p.matchedVia = via;
                p.matchedId = idStd;
                result.candidates.push_back(p);
            }
        }
    };

    collect(device.hardwareIds, MatchVia::HardwareId);
    collect(device.compatibleIds, MatchVia::CompatibleId);

    if (!result.candidates.empty()) {
        result.found = true;
    } else {
        result.notFoundReason = "not in mirror index";
    }
    return result;
}

} // namespace shiftech::core::drivers

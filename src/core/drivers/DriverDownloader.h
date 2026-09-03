#pragma once

#include "DriverCache.h"
#include "DriverProvider.h"
#include <QString>
#include <functional>
#include <string>

namespace shiftech::core::drivers {

struct DownloadOptions {
    int maxAttempts = 3;
    int perAttemptTimeoutMs = 120000;
    bool allowResume = true;
};

struct DownloadResult {
    bool ok = false;
    bool fromCache = false;
    QString payloadPath;        // absolute path to the payload file in the cache
    QString payloadUrl;         // file:// URL to the same
    std::string sha256;         // computed checksum of the payload (lowercase hex)
    std::string error;          // populated when !ok
};

using ProgressFn = std::function<void(qint64 received, qint64 total)>;

// Downloads a driver package into the portable cache. http(s):// and file:// sources.
// Deterministic cache dir; re-download is skipped when the package is already usable.
class DriverDownloader {
public:
    explicit DriverDownloader(DriverCache cache, DownloadOptions opts = {});

    // deviceIds: the hardware/compatible IDs this package resolves, recorded in the
    // cache index so LocalCacheProvider can find it later.
    DownloadResult fetch(const DriverPackage& pkg,
                         const std::vector<std::string>& deviceIds,
                         const ProgressFn& progress = {});

private:
    DriverCache m_cache;
    DownloadOptions m_opts;

    DownloadResult fetchHttp(const DriverPackage& pkg, const QString& destPath,
                             const ProgressFn& progress);
    DownloadResult copyFile(const DriverPackage& pkg, const QString& srcLocalPath,
                            const QString& destPath);
};

} // namespace shiftech::core::drivers

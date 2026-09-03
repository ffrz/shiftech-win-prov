#pragma once

#include "DriverProvider.h"
#include <QString>

namespace shiftech::core::drivers {

// Resolves drivers from an internal HTTP/file-share mirror.
//
// The mirror root must contain `index.json`:
//   {
//     "PCI\\VEN_10EC&DEV_8168": [
//       { "driverName": "...", "version": "1.2.3.4", "provider": "...",
//         "supportedOs": ["win10","win11"], "arch": "x64",
//         "path": "realtek/rt_nic.zip",        // relative to the mirror root (or absolute URL)
//         "packageType": "InfZip",
//         "checksum": "<sha256 hex>", "checksumAlgo": "sha256" }
//     ]
//   }
//
// `downloadUrl` in returned packages is <baseUrl>/<path>. Any checksum in the index is
// passed through so DriverDownloader verifies it. Network/parse failure => found=false.
class MirrorProvider : public DriverProvider {
public:
    // baseUrl: http(s):// or file:// root that contains index.json. Empty => unconfigured.
    explicit MirrorProvider(std::string baseUrl = {}, int timeoutMs = 15000);

    DriverSearchResult search(const hardware::Device& device,
                              const TargetSystem& target) override;
    std::string name() const override { return "mirror"; }

private:
    std::string m_baseUrl;
    int m_timeoutMs;
    QString m_cachedIndex;   // raw index.json text, fetched once per process
    bool m_fetched = false;
    std::string m_fetchError;

    bool ensureIndex();
};

} // namespace shiftech::core::drivers

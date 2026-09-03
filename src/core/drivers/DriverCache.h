#pragma once

#include "DriverProvider.h"
#include <QString>
#include <optional>
#include <string>
#include <vector>

namespace shiftech::core::drivers {

// A cached package: the DriverPackage plus the device IDs it was stored under and the
// name + checksum of the payload file inside the package dir.
struct CachedPackage {
    DriverPackage package;
    std::vector<std::string> deviceIds;   // hardware/compatible IDs this package serves
    std::string payloadFileName;          // e.g. "package.zip" (relative to the package dir)
    std::string sha256;                   // lowercase hex, may be empty
    std::string fetchedAt;                // ISO-8601 UTC
};

// Portable, deterministic on-disk cache of downloaded driver packages.
//
// Layout (all relative to the cache root; default <exeDir>/cache/drivers):
//   <root>/index.json                 { "<hwOrCompatId>": ["<packageId>", ...] }
//   <root>/<packageId>/<payload file>
//   <root>/<packageId>/metadata.json  CachedPackage, no absolute paths
//
// index.json is a convenience lookup; it can always be rebuilt from the metadata files,
// so the tree survives being copied to a USB drive and run from another drive letter.
class DriverCache {
public:
    // rootDir empty => <executable dir>/cache/drivers
    explicit DriverCache(const QString& rootDir = QString());

    QString root() const { return m_root; }

    // Stable 16-hex-char id from provider|driverName|version|arch|downloadUrl.
    // Path-independent, no separators.
    static std::string packageId(const DriverPackage& pkg);

    QString packageDir(const std::string& id) const;      // <root>/<id>
    QString packageDir(const DriverPackage& pkg) const;

    // Cached AND usable: dir + metadata exist and (checksum matches, or no checksum and
    // the payload file is non-empty).
    bool isUsable(const DriverPackage& pkg) const;

    // file:// URL to the cached payload, or empty if not usable.
    QString cachedPayloadUrl(const DriverPackage& pkg) const;

    std::optional<CachedPackage> read(const std::string& id) const;

    // Store metadata for a package whose payload is already written into packageDir(pkg).
    bool store(const CachedPackage& entry) const;

    // index.json
    std::vector<std::string> lookup(const std::string& deviceId) const; // -> packageIds
    void addToIndex(const std::string& packageId, const std::vector<std::string>& deviceIds);
    int rebuildIndex(); // scan all metadata.json, rewrite index.json; returns package count

    static QString sha256OfFile(const QString& path); // lowercase hex, empty on error

private:
    QString m_root;
    QString indexPath() const;
    void addToIndexInternal(const std::string& packageId,
                            const std::vector<std::string>& deviceIds) const;
};

} // namespace shiftech::core::drivers

#pragma once

#include "DriverCache.h"
#include "ProviderChain.h"
#include <QString>
#include <optional>
#include <string>

namespace shiftech::core::drivers {

struct FactoryOptions {
    QString cacheDir;              // empty => default portable location
    std::string mirrorUrl;         // empty => mirror provider reports "no mirror configured"
    std::string wsusScanPackage;   // empty => Windows Update online mode
    QString mockIndexPath;         // for order entry "mock"
};

// Build a provider chain from a comma-separated order spec.
// Known tokens: "localcache", "windowsupdate", "mirror", "mock".
// "driverpack" is explicitly rejected (ADR-0007).
// Returns std::nullopt and sets `error` on an unknown/rejected token.
std::optional<ProviderChain> buildProviderChain(const std::string& orderSpec,
                                                const FactoryOptions& opts,
                                                std::string& error);

// The default order when none is given.
inline const char* kDefaultProviderOrder = "localcache,windowsupdate,mirror";

} // namespace shiftech::core::drivers

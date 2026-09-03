#include "DriverProviderFactory.h"

#include "LocalCacheProvider.h"
#include "MirrorProvider.h"
#include "MockDriverProvider.h"
#include "WindowsUpdateProvider.h"

#include <cctype>
#include <sstream>

namespace shiftech::core::drivers {

std::optional<ProviderChain> buildProviderChain(const std::string& orderSpec,
                                                const FactoryOptions& opts,
                                                std::string& error) {
    ProviderChain chain;

    std::string spec = orderSpec.empty() ? kDefaultProviderOrder : orderSpec;
    std::stringstream ss(spec);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // trim
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())))
            token.erase(token.begin());
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))
            token.pop_back();
        if (token.empty()) continue;

        if (token == "localcache") {
            chain.add(std::make_unique<LocalCacheProvider>(DriverCache(opts.cacheDir)));
        } else if (token == "windowsupdate") {
            chain.add(std::make_unique<WindowsUpdateProvider>(opts.wsusScanPackage));
        } else if (token == "mirror") {
            chain.add(std::make_unique<MirrorProvider>(opts.mirrorUrl));
        } else if (token == "mock") {
            if (opts.mockIndexPath.isEmpty()) {
                error = "provider 'mock' needs --driver-index <path>";
                return std::nullopt;
            }
            chain.add(std::make_unique<MockDriverProvider>(opts.mockIndexPath.toStdString()));
        } else if (token == "driverpack") {
            error = "provider 'driverpack' is not supported (ADR-0007: license forbids "
                    "redistribution)";
            return std::nullopt;
        } else {
            error = "unknown provider '" + token + "'";
            return std::nullopt;
        }
    }

    if (chain.empty()) {
        error = "provider order resolved to an empty chain";
        return std::nullopt;
    }
    return chain;
}

} // namespace shiftech::core::drivers

#pragma once

#include "DriverCache.h"
#include "DriverProvider.h"

namespace shiftech::core::drivers {

// Resolves drivers from the portable on-disk cache only. No network. Never throws.
// downloadUrl in returned packages is a file:// path inside the cache.
class LocalCacheProvider : public DriverProvider {
public:
    explicit LocalCacheProvider(DriverCache cache);

    DriverSearchResult search(const hardware::Device& device,
                              const TargetSystem& target) override;
    std::string name() const override { return "localcache"; }

private:
    DriverCache m_cache;
};

} // namespace shiftech::core::drivers

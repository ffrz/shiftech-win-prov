#include "LocalCacheProvider.h"

#include <QFileInfo>
#include <QUrl>
#include <set>

namespace shiftech::core::drivers {

LocalCacheProvider::LocalCacheProvider(DriverCache cache) : m_cache(std::move(cache)) {}

DriverSearchResult LocalCacheProvider::search(const hardware::Device& device,
                                              const TargetSystem& target) {
    (void)target; // arch/OS filtering is done by pickBest on the returned candidates
    DriverSearchResult result;

    std::set<std::string> seen; // dedupe packageIds

    auto collect = [&](const std::vector<std::string>& ids, MatchVia via) {
        for (const auto& did : ids) {
            for (const auto& pid : m_cache.lookup(did)) {
                if (!seen.insert(pid).second) continue;
                const auto entry = m_cache.read(pid);
                if (!entry) continue;
                DriverPackage pkg = entry->package;
                // point at the local payload
                const QString payload =
                    QUrl(m_cache.cachedPayloadUrl(pkg)).toString();
                if (payload.isEmpty()) continue; // not usable (missing/corrupt payload)
                pkg.downloadUrl = payload.toStdString();
                pkg.matchedVia = via;
                pkg.matchedId = did;
                result.candidates.push_back(pkg);
            }
        }
    };

    collect(device.hardwareIds, MatchVia::HardwareId);
    collect(device.compatibleIds, MatchVia::CompatibleId);

    if (!result.candidates.empty()) {
        result.found = true;
    } else {
        result.notFoundReason = "not in local driver cache";
    }
    return result;
}

} // namespace shiftech::core::drivers

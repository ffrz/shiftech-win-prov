#include "MirrorProvider.h"

namespace shiftech::core::drivers {

MirrorProvider::MirrorProvider(std::string baseUrl) : m_baseUrl(std::move(baseUrl)) {}

DriverSearchResult MirrorProvider::search(const hardware::Device& device,
                                          const TargetSystem& target) {
    (void)device;
    (void)target;
    DriverSearchResult r;
    r.found = false;
    r.notFoundReason = m_baseUrl.empty()
                           ? "no mirror configured (--mirror-url)"
                           : "MirrorProvider not implemented yet (post-Milestone 3)";
    return r;
}

} // namespace shiftech::core::drivers

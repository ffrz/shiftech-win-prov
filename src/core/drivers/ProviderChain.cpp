#include "ProviderChain.h"

namespace shiftech::core::drivers {

void ProviderChain::add(std::unique_ptr<DriverProvider> provider) {
    if (provider) m_providers.push_back(std::move(provider));
}

std::vector<std::string> ProviderChain::names() const {
    std::vector<std::string> out;
    out.reserve(m_providers.size());
    for (const auto& p : m_providers) out.push_back(p->name());
    return out;
}

DriverSearchResult ProviderChain::resolve(const hardware::Device& device,
                                          const TargetSystem& target) {
    DriverSearchResult aggregate;
    std::string reasons;

    for (const auto& provider : m_providers) {
        DriverSearchResult r;
        try {
            r = provider->search(device, target);
        } catch (const std::exception& e) {
            r.found = false;
            r.notFoundReason = std::string("provider threw: ") + e.what();
        } catch (...) {
            r.found = false;
            r.notFoundReason = "provider threw unknown exception";
        }

        if (r.found && !r.candidates.empty()) {
            return r;
        }
        if (!reasons.empty()) reasons += "; ";
        reasons += provider->name() + ": " +
                   (r.notFoundReason.empty() ? "no match" : r.notFoundReason);
    }

    aggregate.found = false;
    aggregate.notFoundReason = reasons.empty() ? "no providers configured" : reasons;
    return aggregate;
}

} // namespace shiftech::core::drivers

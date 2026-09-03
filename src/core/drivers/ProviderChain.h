#pragma once

#include "DriverProvider.h"
#include <memory>
#include <string>
#include <vector>

namespace shiftech::core::drivers {

// An ordered list of providers. resolve() tries each in turn and returns the first
// result with found == true. A provider that throws is caught and treated as a miss,
// so one bad provider never aborts resolution.
class ProviderChain {
public:
    void add(std::unique_ptr<DriverProvider> provider);
    bool empty() const { return m_providers.empty(); }
    std::vector<std::string> names() const;

    DriverSearchResult resolve(const hardware::Device& device, const TargetSystem& target);

private:
    std::vector<std::unique_ptr<DriverProvider>> m_providers;
};

} // namespace shiftech::core::drivers

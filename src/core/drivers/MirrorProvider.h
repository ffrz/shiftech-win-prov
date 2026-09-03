#pragma once

#include "DriverProvider.h"

namespace shiftech::core::drivers {

// Resolves drivers from an internal HTTP/file-share mirror with a JSON index keyed by
// Hardware ID. For hardware Windows Update does not carry; fully under our control.
//
// STUB (Milestone 3): compiles, returns found=false unless a mirror base is configured
// (and even then the real fetch/index parsing is a follow-up).
class MirrorProvider : public DriverProvider {
public:
    // baseUrl: http(s):// or file:// root that contains index.json
    explicit MirrorProvider(std::string baseUrl = {});

    DriverSearchResult search(const hardware::Device& device,
                              const TargetSystem& target) override;
    std::string name() const override { return "mirror"; }

private:
    std::string m_baseUrl;
};

} // namespace shiftech::core::drivers

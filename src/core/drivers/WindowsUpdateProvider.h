#pragma once

#include "DriverProvider.h"

namespace shiftech::core::drivers {

// Resolves drivers via the Windows Update Agent (COM IUpdateSearcher,
// "IsInstalled=0 and Type='Driver'"). Microsoft-hosted, WHQL-signed.
//
// STUB (Milestone 3): compiles, always returns found=false with a clear reason, so the
// provider chain is exercised. Real implementation lands in a Milestone 3.5 / 4 follow-up
// (see docs/DRIVER_PROVIDER.md "WindowsUpdateProvider").
class WindowsUpdateProvider : public DriverProvider {
public:
    // scanPackagePath: optional wsusscn2.cab for offline / air-gapped scans.
    explicit WindowsUpdateProvider(std::string scanPackagePath = {});

    DriverSearchResult search(const hardware::Device& device,
                              const TargetSystem& target) override;
    std::string name() const override { return "windowsupdate"; }

private:
    std::string m_scanPackagePath;
};

} // namespace shiftech::core::drivers

#pragma once

#include "DriverProvider.h"
#include <string>

namespace shiftech::core::drivers {

// Resolves drivers via the Windows Update Agent (COM IUpdateSearcher,
// "IsInstalled=0 and Type='Driver'"). Microsoft-hosted, WHQL-signed.
//
// - Online mode (default): searches Windows Update directly.
// - Offline mode: if `scanPackagePath` (a wsusscn2.cab) is given, or
//   <exeDir>/cache/wsusscn2.cab exists, the search is done against that package
//   (air-gapped machines).
//
// A COM failure / timeout returns found=false with a reason — never throws.
class WindowsUpdateProvider : public DriverProvider {
public:
    explicit WindowsUpdateProvider(std::string scanPackagePath = {}, int timeoutMs = 90000);

    DriverSearchResult search(const hardware::Device& device,
                              const TargetSystem& target) override;
    std::string name() const override { return "windowsupdate"; }

private:
    std::string m_scanPackagePath;
    int m_timeoutMs;
};

} // namespace shiftech::core::drivers

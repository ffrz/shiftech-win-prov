#include "WindowsUpdateProvider.h"

namespace shiftech::core::drivers {

WindowsUpdateProvider::WindowsUpdateProvider(std::string scanPackagePath)
    : m_scanPackagePath(std::move(scanPackagePath)) {}

DriverSearchResult WindowsUpdateProvider::search(const hardware::Device& device,
                                                 const TargetSystem& target) {
    (void)device;
    (void)target;
    DriverSearchResult r;
    r.found = false;
    r.notFoundReason = "WindowsUpdateProvider not implemented yet (Milestone 3.5)";
    return r;
}

} // namespace shiftech::core::drivers

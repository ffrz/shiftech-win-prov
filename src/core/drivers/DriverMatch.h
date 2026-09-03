#pragma once

#include "DriverProvider.h"
#include <optional>

namespace shiftech::core::drivers {

int compareVersions(const std::string& v1, const std::string& v2);

int rankCandidate(const hardware::Device& device, const DriverPackage& pkg, const TargetSystem& target);

std::optional<DriverPackage> pickBest(const hardware::Device& device, const DriverSearchResult& searchResult, const TargetSystem& target);

} // namespace shiftech::core::drivers

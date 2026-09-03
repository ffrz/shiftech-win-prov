#include "DriverMatch.h"
#include <algorithm>
#include <sstream>

namespace shiftech::core::drivers {

namespace {

std::vector<int> parseVersion(const std::string& v) {
    std::vector<int> segments;
    std::stringstream ss(v);
    std::string segment;
    while (std::getline(ss, segment, '.')) {
        try {
            segments.push_back(std::stoi(segment));
        } catch (...) {
            segments.push_back(0);
        }
    }
    return segments;
}

std::string osFamilyToString(TargetSystem::OsFamily os) {
    switch (os) {
        case TargetSystem::OsFamily::Win7: return "win7";
        case TargetSystem::OsFamily::Win8: return "win8";
        case TargetSystem::OsFamily::Win10: return "win10";
        case TargetSystem::OsFamily::Win11: return "win11";
        default: return "";
    }
}

} // namespace

int compareVersions(const std::string& v1, const std::string& v2) {
    std::vector<int> p1 = parseVersion(v1);
    std::vector<int> p2 = parseVersion(v2);
    
    size_t max_len = std::max(p1.size(), p2.size());
    for (size_t i = 0; i < max_len; ++i) {
        int val1 = (i < p1.size()) ? p1[i] : 0;
        int val2 = (i < p2.size()) ? p2[i] : 0;
        if (val1 < val2) return -1;
        if (val1 > val2) return 1;
    }
    return 0;
}

int rankCandidate(const hardware::Device& device, const DriverPackage& pkg, const TargetSystem& target) {
    (void)device;
    // 1. Arch filter
    if (pkg.arch != target.arch) {
        return -1; // incompatible
    }
    
    // 2. OS filter
    std::string osStr = osFamilyToString(target.os);
    bool osSupported = false;
    for (const auto& os : pkg.supportedOs) {
        std::string osLower = os;
        std::transform(osLower.begin(), osLower.end(), osLower.begin(), ::tolower);
        if (osLower == osStr) {
            osSupported = true;
            break;
        }
    }
    if (!osSupported) {
        return -1;
    }
    
    // Since DriverPackage doesn't include the matching ID, we just assign a base rank (0) to compatible packages.
    // The provider is expected to return packages ordered by match quality (HWID > CompatID).
    return 0;
}

std::optional<DriverPackage> pickBest(const hardware::Device& device, const DriverSearchResult& searchResult, const TargetSystem& target) {
    if (!searchResult.found || searchResult.candidates.empty()) {
        return std::nullopt;
    }

    std::optional<DriverPackage> bestPkg;

    for (const auto& pkg : searchResult.candidates) {
        int rank = rankCandidate(device, pkg, target);
        if (rank < 0) continue; // Incompatible

        if (!bestPkg) {
            bestPkg = pkg;
        } else {
            // Tie break with version
            if (compareVersions(pkg.version, bestPkg->version) > 0) {
                bestPkg = pkg;
            }
        }
    }

    return bestPkg;
}

} // namespace shiftech::core::drivers

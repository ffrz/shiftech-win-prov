#include "DriverMatch.h"
#include <algorithm>
#include <sstream>

namespace shiftech::core::drivers {

namespace {

std::vector<long> parseVersion(const std::string& v) {
    std::vector<long> segments;
    std::stringstream ss(v);
    std::string segment;
    while (std::getline(ss, segment, '.')) {
        try {
            segments.push_back(std::stol(segment));
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
    }
    return "";
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Points awarded for how the package matched the device's IDs.
int matchViaScore(MatchVia via) {
    switch (via) {
        case MatchVia::HardwareId:   return 1000;
        case MatchVia::CompatibleId: return 100;
        case MatchVia::Unspecified:  return 0;
    }
    return 0;
}

} // namespace

int compareVersions(const std::string& v1, const std::string& v2) {
    std::vector<long> p1 = parseVersion(v1);
    std::vector<long> p2 = parseVersion(v2);

    size_t max_len = std::max(p1.size(), p2.size());
    for (size_t i = 0; i < max_len; ++i) {
        long a = (i < p1.size()) ? p1[i] : 0;
        long b = (i < p2.size()) ? p2[i] : 0;
        if (a < b) return -1;
        if (a > b) return 1;
    }
    return 0;
}

int rankCandidate(const hardware::Device& device, const DriverPackage& pkg,
                  const TargetSystem& target) {
    (void)device;

    // Hard filters: wrong arch or unsupported OS => not a candidate at all.
    if (pkg.arch != target.arch) {
        return -1;
    }

    const std::string wantedOs = osFamilyToString(target.os);
    bool osSupported = false;
    for (const auto& os : pkg.supportedOs) {
        if (toLower(os) == wantedOs) {
            osSupported = true;
            break;
        }
    }
    if (!osSupported) {
        return -1;
    }

    // Soft ranking: HardwareId hit outranks CompatibleId hit.
    // Version is the tie-breaker and is applied by pickBest, not here.
    return matchViaScore(pkg.matchedVia);
}

std::optional<DriverPackage> pickBest(const hardware::Device& device,
                                      const DriverSearchResult& searchResult,
                                      const TargetSystem& target) {
    if (!searchResult.found || searchResult.candidates.empty()) {
        return std::nullopt;
    }

    std::optional<DriverPackage> best;
    int bestRank = -1;

    for (const auto& pkg : searchResult.candidates) {
        const int rank = rankCandidate(device, pkg, target);
        if (rank < 0) {
            continue; // filtered out (arch / OS)
        }

        if (!best) {
            best = pkg;
            bestRank = rank;
            continue;
        }

        if (rank > bestRank) {
            best = pkg;
            bestRank = rank;
        } else if (rank == bestRank &&
                   compareVersions(pkg.version, best->version) > 0) {
            best = pkg;
        }
    }

    return best;
}

} // namespace shiftech::core::drivers

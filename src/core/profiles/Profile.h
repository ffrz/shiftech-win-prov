#pragma once

#include <string>
#include <vector>

namespace shiftech::core::profiles {

struct AppEntry {
    std::string id;
    bool required = false;
};

struct Profile {
    std::string name;
    std::string description;
    std::vector<AppEntry> applications;
};

} // namespace shiftech::core::profiles

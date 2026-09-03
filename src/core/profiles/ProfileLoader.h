#pragma once

#include "Profile.h"
#include <string>
#include <optional>
#include <variant>

namespace shiftech::core::profiles {

struct ProfileLoadError {
    std::string message;
};

class ProfileLoader {
public:
    static std::variant<Profile, ProfileLoadError> load(const std::string& path);
};

} // namespace shiftech::core::profiles

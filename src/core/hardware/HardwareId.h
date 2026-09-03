#pragma once

#include <string>
#include <string_view>

namespace shiftech::core::hardware {

struct PnpId {
    std::string bus;
    std::string vendor;
    std::string device;
    std::string subsys;
    std::string rev;
    std::string raw;
};

PnpId parseHardwareId(std::string_view hwid);

} // namespace shiftech::core::hardware

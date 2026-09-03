#pragma once

#include "Device.h"
#include <vector>

namespace shiftech::core::hardware {

class DeviceEnumerator {
public:
    std::vector<Device> enumerate();
    std::vector<Device> enumerateNeedingDriver();
};

} // namespace shiftech::core::hardware

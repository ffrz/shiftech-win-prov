#pragma once

#include "SystemInfo.h"

namespace shiftech::core::system {

class SystemInspector {
public:
    static SystemInfo inspect();
};

} // namespace shiftech::core::system

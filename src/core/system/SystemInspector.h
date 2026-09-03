#pragma once

#include "SystemInfo.h"

namespace shiftech::core::system {

class SystemInspector {
public:
    static SystemInfo inspect();

    // Cheap standalone check (no full inspect()).
    static bool isElevated();
};

} // namespace shiftech::core::system

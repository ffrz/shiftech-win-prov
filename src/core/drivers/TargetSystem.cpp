#include "DriverProvider.h"

namespace shiftech::core::drivers {

TargetSystem currentTarget(const system::SystemInfo& info) {
    TargetSystem ts;
    
    if (info.buildNumber >= 22000) ts.os = TargetSystem::OsFamily::Win11;
    else if (info.buildNumber >= 10240) ts.os = TargetSystem::OsFamily::Win10;
    else if (info.buildNumber >= 9200) ts.os = TargetSystem::OsFamily::Win8;
    else ts.os = TargetSystem::OsFamily::Win7;
    
    ts.build = info.buildNumber;
    
    if (info.arch == system::Arch::X64) {
        ts.arch = TargetSystem::Arch::x64;
    } else {
        ts.arch = TargetSystem::Arch::x86;
    }
    
    return ts;
}

} // namespace shiftech::core::drivers

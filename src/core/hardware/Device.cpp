#include "Device.h"

namespace shiftech::core::hardware {

bool Device::needsDriver() const {
    if (status == DeviceStatus::NoDriver || status == DeviceStatus::Unknown) {
        return true;
    }
    
    if (status == DeviceStatus::Problem) {
        // Needs driver set of problem codes:
        // 28: CM_PROB_FAILED_INSTALL (No driver installed)
        // 1: CM_PROB_NOT_CONFIGURED (Device not configured)
        // 3: CM_PROB_OUT_OF_MEMORY (Driver may be corrupt)
        // 24: CM_PROB_DEVICE_NOT_THERE (Device not present but configured, could need re-enumeration/driver)
        // 31: CM_PROB_FAILED_ADD (Device failed to load driver)
        if (problemCode == 28 || problemCode == 1 || problemCode == 3 || 
            problemCode == 24 || problemCode == 31) {
            return true;
        }
    }
    
    return false;
}

} // namespace shiftech::core::hardware

#include "ProvisioningEvent.h"

namespace shiftech::core::provisioning {

const char* toString(Severity s) {
    switch (s) {
        case Severity::Info: return "info";
        case Severity::Success: return "success";
        case Severity::Warning: return "warning";
        case Severity::Error: return "error";
    }
    return "info";
}

} // namespace shiftech::core::provisioning

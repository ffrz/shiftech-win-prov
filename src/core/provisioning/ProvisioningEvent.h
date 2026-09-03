#pragma once

#include <functional>
#include <string>

namespace shiftech::core::provisioning {

enum class Severity { Info, Success, Warning, Error };

const char* toString(Severity s);

struct ProvisioningEvent {
    std::string timestamp;   // ISO-8601 UTC
    std::string category;    // "system" | "hardware" | "driver" | "application" | "report" | "pipeline"
    Severity severity = Severity::Info;
    std::string message;
    int progress = -1;       // 0..100 for the current stage, -1 = n/a
};

// Non-Qt event sink. The engine calls this for every meaningful step.
using EventSink = std::function<void(const ProvisioningEvent&)>;

} // namespace shiftech::core::provisioning

#pragma once

#include <map>
#include <string>
#include <vector>

namespace shiftech::core::config {

enum class TweakState { NotApplied, Applied, Unknown };
enum class TweakOutcome { Applied, AlreadyApplied, Failed, Skipped, RequiresReboot };

const char* toString(TweakOutcome o);

struct TweakArgs {
    std::map<std::string, std::string> values;
    std::string get(const std::string& key, const std::string& def = {}) const {
        auto it = values.find(key);
        return it == values.end() ? def : it->second;
    }
    bool has(const std::string& key) const { return values.count(key) > 0; }
};

struct TweakInfo {
    std::string id;
    std::string title;
    std::string description;   // what it changes, shown in the log/GUI
    bool needsElevation = false;
    std::vector<std::string> requiredArgs;  // arg keys that must be present
};

struct TweakResult {
    std::string id;
    TweakOutcome outcome = TweakOutcome::Failed;
    std::string detail;
};

// A single Windows tweak: describe / check / apply. Implementations live in
// ConfigTweaks.cpp and do the actual registry / process work.
class ConfigTweak {
public:
    virtual ~ConfigTweak() = default;
    virtual TweakInfo info() const = 0;
    virtual TweakState check() const = 0;                 // is it already applied?
    virtual TweakResult apply(const TweakArgs& args) = 0; // do it (or report AlreadyApplied)
};

// The built-in catalog.
std::vector<TweakInfo> catalog();
bool isKnownTweak(const std::string& id);

// Run one tweak by id. Handles: unknown id, missing required args, not-elevated,
// already-applied. Never throws.
TweakResult runTweak(const std::string& id, const TweakArgs& args, bool elevated);

} // namespace shiftech::core::config

#pragma once

#include <map>
#include <string>
#include <vector>

namespace shiftech::core::config {

enum class TweakState { NotApplied, Applied, Unknown };
enum class TweakOutcome { Applied, AlreadyApplied, Failed, Skipped, RequiresReboot };
enum class RevertOutcome { Reverted, NothingToRevert, Failed, Skipped, NotSupported };

const char* toString(TweakOutcome o);
const char* toString(RevertOutcome o);

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

struct RevertResult {
    std::string id;
    RevertOutcome outcome = RevertOutcome::Failed;
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
    // Best-effort undo to Windows defaults. Not every tweak can be perfectly reverted
    // (e.g. cleaned taskbar pins can't be restored) - those return NotSupported.
    virtual RevertResult revert(const TweakArgs& args) = 0;
};

// The built-in catalog.
std::vector<TweakInfo> catalog();
bool isKnownTweak(const std::string& id);

// Run one tweak by id. Handles: unknown id, missing required args, not-elevated,
// already-applied. Never throws.
TweakResult runTweak(const std::string& id, const TweakArgs& args, bool elevated);

// Revert one tweak by id (best effort). Same guards as runTweak. Never throws.
RevertResult revertTweak(const std::string& id, const TweakArgs& args, bool elevated);

} // namespace shiftech::core::config

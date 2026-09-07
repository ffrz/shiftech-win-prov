#pragma once

#include <map>
#include <string>
#include <vector>

class QJsonObject;

namespace shiftech::core::profiles {

// --- drivers section ---
struct DriverSection {
    bool enabled = true;
    std::string providerOrder;            // "" => engine default
    bool installUnsigned = false;
    std::vector<std::string> exclude;     // hardware/instance IDs to leave alone
};

// --- applications section ---
//
// An app entry can have a local installer (apps/<localId>/), a winget id, or both.
// Resolution order at install time: local first, then winget as a fallback.
struct AppEntry {
    std::string id;                       // unique display key within the profile
    std::string localId;                  // apps/<localId>/ folder; "" => no local source
    std::string wingetId;                 // winget package id; "" => no winget source
    bool enabled = true;
    bool required = false;

    bool hasLocal() const { return !localId.empty(); }
    bool hasWinget() const { return !wingetId.empty(); }
};

// --- config section ---
struct ConfigEntry {
    std::string id;                       // built-in tweak id
    bool enabled = true;
    std::map<std::string, std::string> args;
};

struct Profile {
    std::string name;
    std::string description;
    DriverSection drivers;
    std::vector<AppEntry> applications;
    std::vector<ConfigEntry> config;

    // Convenience: only the enabled items.
    std::vector<AppEntry> enabledApps() const;
    std::vector<ConfigEntry> enabledConfig() const;

    // Serialize back to the on-disk JSON shape (for the GUI's "Save as profile…").
    QJsonObject toJson() const;
};

} // namespace shiftech::core::profiles

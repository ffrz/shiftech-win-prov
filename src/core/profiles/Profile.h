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
enum class AppSource { WinGet, Local };

struct AppEntry {
    std::string id;                       // unique key; also apps/<id>/ folder for Local
    AppSource source = AppSource::WinGet;
    std::string wingetId;                 // required when source == WinGet
    bool enabled = true;
    bool required = false;
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

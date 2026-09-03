#include "ProfileLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <unordered_set>

namespace shiftech::core::profiles {

std::vector<AppEntry> Profile::enabledApps() const {
    std::vector<AppEntry> out;
    for (const auto& a : applications) if (a.enabled) out.push_back(a);
    return out;
}
std::vector<ConfigEntry> Profile::enabledConfig() const {
    std::vector<ConfigEntry> out;
    for (const auto& c : config) if (c.enabled) out.push_back(c);
    return out;
}

namespace {

using E = ProfileLoadError;

bool onlyKeys(const QJsonObject& o, std::initializer_list<const char*> allowed,
              std::string& badKey) {
    for (const QString& k : o.keys()) {
        bool ok = false;
        for (const char* a : allowed) if (k == a) { ok = true; break; }
        if (!ok) { badKey = k.toStdString(); return false; }
    }
    return true;
}

// The built-in config tweak ids — must match ConfigTweaks.cpp.
const std::unordered_set<std::string>& knownTweaks() {
    static const std::unordered_set<std::string> s = {
        "disable-password-expiry", "clean-taskbar-pins", "clean-startup-items",
        "disable-startup-item", "show-file-extensions", "show-hidden-files",
        "disable-fast-startup", "set-power-high-performance", "disable-hibernate",
        "set-timezone", "enable-rdp", "set-computer-name"};
    return s;
}

} // namespace

std::variant<Profile, ProfileLoadError> ProfileLoader::load(const std::string& path) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return E{"cannot open profile: " + path};
    }
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError) {
        return E{"JSON parse error: " + perr.errorString().toStdString()};
    }
    if (!doc.isObject()) return E{"profile root must be a JSON object"};
    const QJsonObject root = doc.object();

    Profile p;

    // name / stem
    const QString stem = QFileInfo(QString::fromStdString(path)).completeBaseName();
    if (!root.contains("name") || !root["name"].isString())
        return E{"missing or invalid 'name'"};
    p.name = root["name"].toString().toStdString();
    if (QString::fromStdString(p.name) != stem)
        return E{"profile 'name' (" + p.name + ") must match the file stem (" +
                 stem.toStdString() + ")"};

    if (!root.contains("description") || !root["description"].isString())
        return E{"missing or invalid 'description'"};
    p.description = root["description"].toString().toStdString();

    std::string badKey;
    if (!onlyKeys(root, {"name", "description", "drivers", "applications", "config"}, badKey))
        return E{"unknown top-level key '" + badKey + "'"};

    // --- drivers ---
    if (root.contains("drivers")) {
        if (!root["drivers"].isObject()) return E{"'drivers' must be an object"};
        const QJsonObject d = root["drivers"].toObject();
        if (!onlyKeys(d, {"enabled", "providerOrder", "installUnsigned", "exclude"}, badKey))
            return E{"unknown key in 'drivers': '" + badKey + "'"};
        p.drivers.enabled = d.value("enabled").toBool(true);
        p.drivers.providerOrder = d.value("providerOrder").toString().toStdString();
        p.drivers.installUnsigned = d.value("installUnsigned").toBool(false);
        for (const auto& v : d.value("exclude").toArray())
            p.drivers.exclude.push_back(v.toString().toStdString());
    }

    // --- applications ---
    std::unordered_set<std::string> ids;
    for (const auto& v : root.value("applications").toArray()) {
        if (!v.isObject()) return E{"each application entry must be an object"};
        const QJsonObject a = v.toObject();
        if (!onlyKeys(a, {"id", "source", "wingetId", "enabled", "required"}, badKey))
            return E{"unknown key in application entry: '" + badKey + "'"};

        AppEntry e;
        e.id = a.value("id").toString().toStdString();
        if (e.id.empty()) return E{"application entry missing 'id'"};
        if (!ids.insert(e.id).second) return E{"duplicate application id '" + e.id + "'"};

        const QString src = a.value("source").toString("winget");
        if (src == "winget") e.source = AppSource::WinGet;
        else if (src == "local") e.source = AppSource::Local;
        else return E{"application '" + e.id + "': source must be 'winget' or 'local'"};

        e.wingetId = a.value("wingetId").toString().toStdString();
        if (e.source == AppSource::WinGet && e.wingetId.empty())
            e.wingetId = e.id; // allow shorthand: id == wingetId
        e.enabled = a.value("enabled").toBool(true);
        e.required = a.value("required").toBool(false);
        p.applications.push_back(e);
    }

    // --- config ---
    std::unordered_set<std::string> cfgIds;
    for (const auto& v : root.value("config").toArray()) {
        if (!v.isObject()) return E{"each config entry must be an object"};
        const QJsonObject c = v.toObject();
        if (!onlyKeys(c, {"id", "enabled", "args"}, badKey))
            return E{"unknown key in config entry: '" + badKey + "'"};
        ConfigEntry e;
        e.id = c.value("id").toString().toStdString();
        if (e.id.empty()) return E{"config entry missing 'id'"};
        if (!knownTweaks().count(e.id))
            return E{"unknown config tweak id '" + e.id + "'"};
        if (!cfgIds.insert(e.id).second)
            return E{"duplicate config tweak '" + e.id + "'"};
        e.enabled = c.value("enabled").toBool(true);
        for (const QString& k : c.value("args").toObject().keys())
            e.args[k.toStdString()] = c.value("args").toObject().value(k).toString().toStdString();
        p.config.push_back(e);
    }

    return p;
}

} // namespace shiftech::core::profiles

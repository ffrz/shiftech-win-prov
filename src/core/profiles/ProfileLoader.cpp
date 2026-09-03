#include "ProfileLoader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <unordered_set>

namespace shiftech::core::profiles {

std::variant<Profile, ProfileLoadError> ProfileLoader::load(const std::string& path) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ProfileLoadError{"Failed to open file: " + path};
    }

    QByteArray data = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return ProfileLoadError{"JSON parse error: " + parseError.errorString().toStdString()};
    }

    if (!doc.isObject()) {
        return ProfileLoadError{"Profile root must be a JSON object"};
    }

    QJsonObject root = doc.object();
    Profile profile;

    // Validate name matches file stem
    QFileInfo fileInfo(QString::fromStdString(path));
    QString expectedName = fileInfo.completeBaseName();
    
    if (!root.contains("name") || !root["name"].isString()) {
        return ProfileLoadError{"Missing or invalid 'name'"};
    }
    profile.name = root["name"].toString().toStdString();
    
    if (profile.name != expectedName.toStdString()) {
        return ProfileLoadError{"Profile name '" + profile.name + "' does not match file stem '" + expectedName.toStdString() + "'"};
    }

    if (!root.contains("description") || !root["description"].isString()) {
        return ProfileLoadError{"Missing or invalid 'description'"};
    }
    profile.description = root["description"].toString().toStdString();

    if (!root.contains("applications") || !root["applications"].isArray()) {
        return ProfileLoadError{"Missing or invalid 'applications' array"};
    }

    QJsonArray appsArray = root["applications"].toArray();
    std::unordered_set<std::string> seenIds;

    for (int i = 0; i < appsArray.size(); ++i) {
        if (!appsArray[i].isObject()) {
            return ProfileLoadError{"Application entry at index " + std::to_string(i) + " is not an object"};
        }
        
        QJsonObject appObj = appsArray[i].toObject();
        AppEntry entry;

        if (!appObj.contains("id") || !appObj["id"].isString() || appObj["id"].toString().isEmpty()) {
            return ProfileLoadError{"Missing or empty 'id' at application index " + std::to_string(i)};
        }
        entry.id = appObj["id"].toString().toStdString();

        if (seenIds.find(entry.id) != seenIds.end()) {
            return ProfileLoadError{"Duplicate application id '" + entry.id + "'"};
        }
        seenIds.insert(entry.id);

        if (appObj.contains("required")) {
            if (!appObj["required"].isBool()) {
                return ProfileLoadError{"'required' must be a boolean at application index " + std::to_string(i)};
            }
            entry.required = appObj["required"].toBool();
        } else {
            entry.required = false;
        }

        // Check for unknown keys in appObj
        for (const QString& key : appObj.keys()) {
            if (key != "id" && key != "required") {
                return ProfileLoadError{"Unknown key '" + key.toStdString() + "' in application entry '" + entry.id + "'"};
            }
        }

        profile.applications.push_back(entry);
    }

    // Check for unknown keys in root
    for (const QString& key : root.keys()) {
        if (key != "name" && key != "description" && key != "applications") {
            return ProfileLoadError{"Unknown key '" + key.toStdString() + "' in profile root"};
        }
    }

    return profile;
}

} // namespace shiftech::core::profiles

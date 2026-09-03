#pragma once

#include "ApplicationProvider.h"
#include <QString>
#include <optional>
#include <string>
#include <vector>

namespace shiftech::core::applications {

// The parsed apps/<id>/app.json manifest.
struct LocalAppManifest {
    std::string id;               // = folder name
    std::string name;
    std::string installerFile;    // relative to the app folder; .exe or .msi only
    std::vector<std::string> silentArgs;
    std::string detectType;       // "registry" | "file" | "arp"
    std::vector<std::string> detectKeys;   // registry key paths or file paths
    std::string detectName;       // ARP display-name substring
    std::vector<int> expectedExitCodes;    // default {0, 1641, 3010}
};

// Loads a manifest from a folder. Returns nullopt + fills `error` on any problem.
std::optional<LocalAppManifest> loadLocalAppManifest(const QString& appFolder,
                                                     std::string& error);

// Installs applications from apps/<id>/ folders on the (portable) medium.
//   - isInstalled(id): runs the manifest's detect rule
//   - install(id): runs the declared installer with silentArgs (EXE) or msiexec (MSI)
// appsRoot: the directory that contains the per-app folders. Empty => <exeDir>/apps
// (with the usual repo-relative fallbacks).
class LocalInstallerProvider : public ApplicationProvider {
public:
    explicit LocalInstallerProvider(QString appsRoot = QString());

    std::string name() const override { return "local"; }
    bool isInstalled(const std::string& id) override;
    InstallResult install(const std::string& id, const InstallOptions& options = {}) override;

    // For the GUI: what local apps are available on the medium.
    std::vector<LocalAppManifest> available() const;
    QString appsRoot() const { return m_root; }

private:
    QString m_root;
    QString folderFor(const std::string& id) const;
};

} // namespace shiftech::core::applications

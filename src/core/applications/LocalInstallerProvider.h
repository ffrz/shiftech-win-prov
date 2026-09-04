#pragma once

#include "ApplicationProvider.h"
#include <QString>
#include <optional>
#include <string>
#include <vector>

namespace shiftech::core::applications {

enum class LocalAppKind {
    Installer,   // .exe / .msi -> run it silently
    Portable,    // .zip / .7z  -> extract to a folder, optional shortcut
};

// The parsed apps/<id>/app.json manifest.
struct LocalAppManifest {
    std::string id;               // = folder name
    std::string name;
    LocalAppKind kind = LocalAppKind::Installer;

    // kind == Installer
    std::string installerFile;    // relative to the app folder; .exe or .msi
    std::vector<std::string> silentArgs;
    std::vector<int> expectedExitCodes;    // default {0, 1641, 3010}

    // kind == Portable
    std::string archiveFile;      // relative; .zip / .7z
    std::string extractTo;        // may contain %USERPROFILE%, %PUBLIC%, %DESKTOP%, etc.
    bool flattenSingleRoot = false; // if the archive has one top-level folder, drop it
    std::string shortcutExe;      // relative to extractTo; if set, make a Desktop shortcut to it
    std::string shortcutName;     // shortcut file name (default = name)

    // detect (both kinds)
    std::string detectType;       // "registry" | "file" | "arp" | "folder"
    std::vector<std::string> detectKeys;   // registry key paths / file paths / a folder path
    std::string detectName;       // ARP display-name substring
};

// Loads a manifest from a folder. Returns nullopt + fills `error` on any problem.
std::optional<LocalAppManifest> loadLocalAppManifest(const QString& appFolder,
                                                     std::string& error);

// Expand %USERPROFILE% / %PUBLIC% / %DESKTOP% / %PUBLIC_DESKTOP% / %PROGRAMDATA% and
// generic %ENV% in a manifest path. Exposed for tests.
QString expandPath(const QString& raw);

// Installs / deploys applications from apps/<id>/ folders on the (portable) medium.
//   - Installer: run the .exe/.msi silently; detect via registry/file/arp
//   - Portable:  extract the archive to `extractTo`, optionally drop a Desktop shortcut;
//                detect via the `folder` rule (extractTo exists and is non-empty)
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

    // "" if apps/<id>/app.json parses AND the installer/archive file is present;
    // otherwise a human-readable reason (missing installer, bad manifest, …).
    std::string manifestError(const std::string& id) const;

private:
    QString m_root;
    QString folderFor(const std::string& id) const;
    InstallResult runInstaller(const LocalAppManifest& m, const QString& folder);
    InstallResult deployPortable(const LocalAppManifest& m, const QString& folder);
};

} // namespace shiftech::core::applications

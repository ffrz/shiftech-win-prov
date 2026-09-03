#include "AppsInstallCommand.h"
#include <iostream>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include "../../core/profiles/ProfileLoader.h"
#include "../../core/applications/WinGetProvider.h"

using namespace shiftech::core::profiles;
using namespace shiftech::core::applications;

namespace shiftech::cli::commands {

int AppsInstallCommand::execute(const std::vector<std::string>& args) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Install applications from a profile");
    
    QCommandLineOption profileOpt(QStringList() << "p" << "profile", "Profile name or file stem (e.g. 'standard')", "profile");
    parser.addOption(profileOpt);

    QCommandLineOption dryRunOpt(QStringList() << "d" << "dry-run", "Print the plan without installing");
    parser.addOption(dryRunOpt);

    QCommandLineOption profilesDirOpt(QStringList() << "profiles-dir", "Directory containing profiles", "dir");
    parser.addOption(profilesDirOpt);

    QStringList qtArgs;
    qtArgs << "provisioner.exe" << "apps" << "install";
    for (const auto& a : args) {
        qtArgs << QString::fromStdString(a);
    }
    
    parser.process(qtArgs);

    if (!parser.isSet(profileOpt)) {
        std::cerr << "Error: --profile is required.\n";
        return 2;
    }

    QString profileName = parser.value(profileOpt);
    QString profilesDir = parser.value(profilesDirOpt);
    if (profilesDir.isEmpty()) {
        profilesDir = QCoreApplication::applicationDirPath() + "/profiles";
    }

    QString profilePath = QDir(profilesDir).filePath(profileName + ".json");
    
    // Fallback if the user passed an absolute path
    if (QFileInfo(profileName).isAbsolute() && profileName.endsWith(".json")) {
        profilePath = profileName;
    }

    auto loadRes = ProfileLoader::load(profilePath.toStdString());
    if (std::holds_alternative<ProfileLoadError>(loadRes)) {
        std::cerr << "Error loading profile: " << std::get<ProfileLoadError>(loadRes).message << "\n";
        return 2;
    }

    Profile profile = std::get<Profile>(loadRes);
    bool dryRun = parser.isSet(dryRunOpt);

    std::cout << "Shiftech Win Provisioner - apps install\n\n";
    std::cout << "Profile: " << profile.name << " (" << profile.description << ")\n";
    std::cout << "Applications to provision: " << profile.applications.size() << "\n";
    if (dryRun) {
        std::cout << "Mode: DRY-RUN\n";
    }
    std::cout << "----------------------------------------\n";

    WinGetProvider winget;
    if (!dryRun && !winget.isAvailable()) {
        std::cerr << "Error: WinGet is not available on this system.\n";
        return 2;
    }

    int failCount = 0;
    int installedCount = 0;
    int skippedCount = 0;

    for (const auto& app : profile.applications) {
        std::cout << "App: " << app.id << " (Required: " << (app.required ? "YES" : "NO") << ")\n";
        
        if (winget.isInstalled(app.id)) {
            std::cout << "  Status: ALREADY INSTALLED\n";
            skippedCount++;
            continue;
        }

        if (dryRun) {
            std::cout << "  Status: WILL INSTALL\n";
            continue;
        }

        std::cout << "  Status: INSTALLING...\n";
        InstallOptions opts;
        InstallResult res = winget.install(app.id, opts);
        if (res.ok) {
            std::cout << "  Result: SUCCESS\n";
            installedCount++;
        } else {
            std::cerr << "  Result: FAILED\n";
            std::cerr << "  Log: " << res.log << "\n";
            if (app.required) {
                std::cerr << "  WARNING: Required application failed to install.\n";
            }
            failCount++;
        }
    }

    std::cout << "----------------------------------------\n";
    if (dryRun) {
        std::cout << "Dry-run complete.\n";
        return 0;
    }

    std::cout << "Summary:\n";
    std::cout << "  Installed: " << installedCount << "\n";
    std::cout << "  Already Installed (Skipped): " << skippedCount << "\n";
    std::cout << "  Failed: " << failCount << "\n";

    if (failCount > 0) {
        return 1;
    }
    return 0;
}

} // namespace shiftech::cli::commands

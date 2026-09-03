#include "AppsInstallCommand.h"

#include "../../core/applications/WinGetProvider.h"
#include "../../core/profiles/ProfileLoader.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTextStream>

using namespace shiftech::core::profiles;
using namespace shiftech::core::applications;

namespace shiftech::cli::commands {

namespace {

QString findProfile(const QString& nameOrPath, const QString& explicitDir) {
    if (QFileInfo(nameOrPath).isAbsolute() && nameOrPath.endsWith(".json")) {
        return QFileInfo::exists(nameOrPath) ? nameOrPath : QString();
    }
    const QString exeDir = QCoreApplication::applicationDirPath();
    QStringList dirs;
    if (!explicitDir.isEmpty()) {
        dirs << explicitDir;
    } else {
        dirs << exeDir + "/profiles" << exeDir + "/../profiles" << exeDir + "/../../profiles";
    }
    for (const QString& d : dirs) {
        const QString c = QDir(d).filePath(nameOrPath + ".json");
        if (QFileInfo::exists(c)) return c;
    }
    return {};
}

} // namespace

int AppsInstallCommand::execute(const std::vector<std::string>& args) {
    QTextStream out(stdout);
    QTextStream errs(stderr);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#endif

    QCommandLineParser parser;
    QCommandLineOption profileOpt({"p", "profile"}, "Profile name or .json path", "profile");
    QCommandLineOption dryRunOpt({"d", "dry-run"}, "Print the plan without installing");
    QCommandLineOption profilesDirOpt("profiles-dir", "Directory containing profiles", "dir");
    QCommandLineOption jsonOpt("json", "Machine-readable output");
    parser.addOptions({profileOpt, dryRunOpt, profilesDirOpt, jsonOpt});

    QStringList qtArgs{"provisioner"};
    for (const auto& a : args) qtArgs << QString::fromStdString(a);
    if (!parser.parse(qtArgs)) {
        errs << "Error: " << parser.errorText() << "\n";
        return 3;
    }
    if (!parser.isSet(profileOpt)) {
        errs << "Error: --profile is required.\n";
        return 3;
    }

    const bool dryRun = parser.isSet(dryRunOpt);
    const bool json = parser.isSet(jsonOpt);

    const QString path = findProfile(parser.value(profileOpt), parser.value(profilesDirOpt));
    if (path.isEmpty()) {
        errs << "Error: profile '" << parser.value(profileOpt)
             << "' not found. Pass --profiles-dir <dir> or an absolute .json path.\n";
        return 2;
    }

    auto loaded = ProfileLoader::load(path.toStdString());
    if (std::holds_alternative<ProfileLoadError>(loaded)) {
        errs << "Error: invalid profile: "
             << QString::fromStdString(std::get<ProfileLoadError>(loaded).message) << "\n";
        return 2;
    }
    const Profile profile = std::get<Profile>(loaded);

    WinGetProvider winget;
    const bool wingetOk = winget.isAvailable();

    int installed = 0, already = 0, failed = 0, failedRequired = 0, skipped = 0;
    QJsonArray items;

    if (!json) {
        out << "Shiftech Win Provisioner - apps install" << (dryRun ? "  (DRY RUN)" : "")
            << "\n\n";
        out << "Profile: " << QString::fromStdString(profile.name) << " — "
            << QString::fromStdString(profile.description) << "\n";
        out << "Applications: " << profile.applications.size() << "\n";
        if (!wingetOk)
            out << "winget: NOT AVAILABLE — every app will be skipped\n";
        out << "----------------------------------------\n";
    }

    for (const auto& app : profile.applications) {
        QJsonObject item;
        item["id"] = QString::fromStdString(app.id);
        item["required"] = app.required;

        QString status;
        if (!wingetOk) {
            status = "skipped_no_winget";
            ++skipped;
        } else if (winget.isInstalled(app.id)) {
            status = "already_installed";
            ++already;
        } else if (dryRun) {
            status = "would_install";
        } else {
            const InstallResult r = winget.install(app.id, {});
            if (r.ok) {
                status = "installed";
                ++installed;
            } else {
                status = "failed";
                ++failed;
                if (app.required) ++failedRequired;
                item["exitCode"] = r.exitCode;
            }
        }
        item["status"] = status;
        items.append(item);

        if (!json) {
            out << "  " << QString::fromStdString(app.id).leftJustified(34)
                << status << (app.required && status == "failed" ? "  (REQUIRED)" : "")
                << "\n";
        }
    }

    if (json) {
        QJsonObject root;
        root["profile"] = QString::fromStdString(profile.name);
        root["dryRun"] = dryRun;
        root["wingetAvailable"] = wingetOk;
        root["items"] = items;
        QJsonObject sum;
        sum["installed"] = installed;
        sum["alreadyInstalled"] = already;
        sum["failed"] = failed;
        sum["failedRequired"] = failedRequired;
        sum["skipped"] = skipped;
        root["summary"] = sum;
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    } else {
        out << "----------------------------------------\n";
        if (dryRun) {
            out << "Dry run complete.\n";
        } else {
            out << "Installed: " << installed << "   Already: " << already
                << "   Failed: " << failed << "   Skipped: " << skipped << "\n";
        }
    }
    out.flush();

    // Exit: 2 already handled (profile). 1 = warnings (a required app failed, or
    // everything was skipped because winget is missing). 0 = clean.
    if (failedRequired > 0) return 1;
    if (!wingetOk && !profile.applications.empty()) return 1;
    return 0;
}

} // namespace shiftech::cli::commands

#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>
#include "commands/ScanCommand.h"
#include "commands/DriversScanCommand.h"
#include "commands/DriversResolveCommand.h"
#include "commands/DriversInstallCommand.h"
#include "commands/AppsInstallCommand.h"
#include "commands/ProvisionCommand.h"

namespace {

void printUsage(QTextStream& out) {
    out << "Shiftech Win Provisioner\n\n"
        << "Usage:\n"
        << "  provisioner scan [--json]\n"
        << "  provisioner drivers scan [--json] [--provider <name>] [--driver-index <path>]\n"
        << "  provisioner drivers resolve [--download] [--json] [--provider-order <csv>]\n"
        << "                              [--cache-dir <p>] [--driver-index <p>] [--mirror-url <u>]\n"
        << "  provisioner drivers install [--dry-run] [--json] [--only <instanceId>]\n"
        << "  provisioner apps install --profile <name> [--dry-run] [--profiles-dir <dir>]\n"
        << "  provisioner provision --profile <name> [--dry-run] [--skip-drivers] [--skip-apps]\n"
        << "  provisioner report [--last | --run <id>] [--json]\n"
        << "\nGlobal:\n"
        << "  --json        machine-readable output where supported\n"
        << "  -h, --help    show this help\n";
}

// True if the raw arg list contains --json anywhere.
bool hasJson(const QStringList& args) {
    return args.contains("--json");
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("provisioner");
    app.setApplicationVersion("1.0");

    QTextStream out(stdout);

    // Raw args after the program name; we dispatch on the subcommand ourselves so
    // each command owns its own option parsing (QCommandLineParser cannot forward
    // unknown options to a subcommand).
    QStringList args = app.arguments();
    args.removeFirst();

    if (args.isEmpty() || args.first() == "-h" || args.first() == "--help") {
        printUsage(out);
        return args.isEmpty() ? 3 : 0;
    }

    const QString command = args.takeFirst();

    if (command == "scan") {
        return shiftech::cli::commands::ScanCommand::run(hasJson(args));
    }

    if (command == "drivers") {
        if (args.isEmpty()) {
            out << "Missing subcommand for 'drivers' (expected: scan)\n";
            return 3;
        }
        const QString sub = args.takeFirst();
        if (sub == "scan") {
            QString provider;   // empty => provider chain
            QString indexFile;
            for (int i = 0; i < args.size(); ++i) {
                if (args[i] == "--provider" && i + 1 < args.size()) provider = args[++i];
                else if (args[i] == "--driver-index" && i + 1 < args.size()) indexFile = args[++i];
            }
            return shiftech::cli::commands::DriversScanCommand::run(hasJson(args), provider, indexFile);
        }
        if (sub == "resolve") {
            return shiftech::cli::commands::DriversResolveCommand::run(args);
        }
        if (sub == "install") {
            return shiftech::cli::commands::DriversInstallCommand::run(args);
        }
        out << "Unknown drivers subcommand: " << sub << " (expected: scan, resolve, install)\n";
        return 3;
    }

    if (command == "apps") {
        if (args.isEmpty()) {
            out << "Missing subcommand for 'apps' (expected: install)\n";
            return 3;
        }
        const QString sub = args.takeFirst();
        if (sub == "install") {
            shiftech::cli::commands::AppsInstallCommand cmd;
            std::vector<std::string> forwarded;
            for (const QString& a : args) forwarded.push_back(a.toStdString());
            return cmd.execute(forwarded);
        }
        out << "Unknown apps subcommand: " << sub << "\n";
        return 3;
    }

    if (command == "provision") {
        return shiftech::cli::commands::ProvisionCommand::run(args);
    }

    if (command == "report") {
        return shiftech::cli::commands::ReportCommand::run(args);
    }

    out << "Unknown command: " << command << "\n\n";
    printUsage(out);
    return 3;
}

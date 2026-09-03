#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>
#include "commands/ScanCommand.h"
#include "commands/DriversScanCommand.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("provisioner");
    app.setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Shiftech Win Provisioner");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption jsonOption("json", "emit machine-readable JSON to stdout instead of human text");
    parser.addOption(jsonOption);

    QCommandLineOption noColorOption("no-color", "disable ANSI color");
    parser.addOption(noColorOption);

    QCommandLineOption verboseOption(QStringList() << "v" << "verbose", "debug-level console output");
    parser.addOption(verboseOption);
    
    QCommandLineOption providerOption("provider", "Driver provider to use (mock)", "name", "mock");
    parser.addOption(providerOption);
    
    QCommandLineOption driverIndexOption("driver-index", "Path to mock driver index", "path");
    parser.addOption(driverIndexOption);

    parser.addPositionalArgument("command", "Command to run (e.g. scan)");
    parser.addPositionalArgument("subcommand", "Subcommand (e.g. scan for drivers command)");

    parser.parse(app.arguments());

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(3);
    }

    QString command = args.first();
    bool json = parser.isSet(jsonOption);

    if (command == "scan") {
        return shiftech::cli::commands::ScanCommand::run(json);
    } else if (command == "drivers") {
        if (args.size() < 2) {
            QTextStream out(stdout);
            out << "Missing subcommand for drivers.\n";
            return 3;
        }
        QString subcommand = args[1];
        if (subcommand == "scan") {
            QString provider = parser.value(providerOption);
            QString indexFile = parser.value(driverIndexOption);
            return shiftech::cli::commands::DriversScanCommand::run(json, provider, indexFile);
        } else {
            QTextStream out(stdout);
            out << "Unknown drivers subcommand: " << subcommand << "\n";
            return 3;
        }
    } else {
        QTextStream out(stdout);
        out << "Unknown command: " << command << "\n";
        parser.showHelp(3);
    }

    return 0;
}

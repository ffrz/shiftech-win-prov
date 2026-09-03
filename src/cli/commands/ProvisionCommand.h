#pragma once

#include <QStringList>

namespace shiftech::cli::commands {

// provisioner provision --profile <name> [--dry-run] [--skip-drivers] [--skip-apps]
//                       [--provider-order <csv>] [--cache-dir <p>] [--driver-index <p>]
//                       [--profiles-dir <p>] [--json]
class ProvisionCommand {
public:
    static int run(const QStringList& args);
};

// provisioner report [--last | --run <id>] [--json] [--log-dir <p>]
class ReportCommand {
public:
    static int run(const QStringList& args);
};

} // namespace shiftech::cli::commands

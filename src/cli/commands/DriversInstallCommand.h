#pragma once

#include <QStringList>

namespace shiftech::cli::commands {

// provisioner drivers install [--dry-run] [--json] [--only <instanceId>]
//                             [--provider-order <csv>] [--cache-dir <p>]
//                             [--driver-index <p>] [--mirror-url <u>]
class DriversInstallCommand {
public:
    static int run(const QStringList& args);
};

} // namespace shiftech::cli::commands

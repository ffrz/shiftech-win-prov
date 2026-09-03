#pragma once

#include <QStringList>

namespace shiftech::cli::commands {

// provisioner reset [--last | --run <id>] [--dry-run] [--json]
//                   [--skip-apps] [--skip-drivers] [--skip-config] [--purge-cache]
//                   [--log-dir <p>] [--cache-dir <p>]
//
// Undoes a provisioning run using its run.json: uninstalls apps the run installed,
// deletes drivers it published (pnputil /delete-driver), reverts config tweaks it applied.
class ResetCommand {
public:
    static int run(const QStringList& args);
};

} // namespace shiftech::cli::commands

#pragma once

#include <QStringList>

namespace shiftech::cli::commands {

// provisioner drivers resolve [--download] [--json] [--cache-dir <p>]
//                             [--provider-order <csv>] [--driver-index <p>]
//                             [--mirror-url <u>] [--wsus-scan <cab>]
class DriversResolveCommand {
public:
    static int run(const QStringList& args);
};

} // namespace shiftech::cli::commands

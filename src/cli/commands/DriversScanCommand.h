#pragma once

#include <QString>

namespace shiftech::cli::commands {

class DriversScanCommand {
public:
    static int run(bool jsonOutput, const QString& providerName, const QString& indexFile);
};

} // namespace shiftech::cli::commands

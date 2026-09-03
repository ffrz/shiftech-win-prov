#pragma once

#include <string>
#include <vector>

namespace shiftech::cli::commands {

class AppsInstallCommand {
public:
    int execute(const std::vector<std::string>& args);
};

} // namespace shiftech::cli::commands

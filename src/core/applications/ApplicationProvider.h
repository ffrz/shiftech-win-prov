#pragma once

#include <string>

namespace shiftech::core::applications {

struct InstallOptions {
    // We can add options here if needed, like silent mode overriding, etc.
};

struct InstallResult {
    bool ok = false;
    int exitCode = -1;
    std::string log;
    bool alreadyInstalled = false;
};

class ApplicationProvider {
public:
    virtual ~ApplicationProvider() = default;

    virtual std::string name() const = 0;
    virtual bool isInstalled(const std::string& id) = 0;
    virtual InstallResult install(const std::string& id, const InstallOptions& options = {}) = 0;
};

} // namespace shiftech::core::applications

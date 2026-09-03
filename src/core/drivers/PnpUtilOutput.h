#pragma once

#include <string>

// Pure parsing of `pnputil /add-driver ... /install` output. No process spawning here.
namespace shiftech::core::drivers::pnputil {

struct AddDriverResult {
    bool succeeded = false;
    bool rebootRequired = false;
    std::string publishedName;   // oemNN.inf when pnputil reports it, else empty
};

// Interpret an exit code + captured stdout+stderr from `pnputil /add-driver`.
// pnputil exit codes: 0 = ok, 259 (ERROR_NO_MORE_ITEMS) sometimes on "no devices",
// 3010 (ERROR_SUCCESS_REBOOT_REQUIRED) = ok but reboot, 1073807364 also seen for reboot.
AddDriverResult parseAddDriver(int exitCode, const std::string& output);

} // namespace shiftech::core::drivers::pnputil

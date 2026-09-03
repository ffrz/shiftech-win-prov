#include "PnpUtilOutput.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace shiftech::core::drivers::pnputil {

namespace {
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}

AddDriverResult parseAddDriver(int exitCode, const std::string& output) {
    AddDriverResult r;
    const std::string low = toLower(output);

    // ERROR_SUCCESS_REBOOT_REQUIRED == 3010; some builds surface 0xD0000BC4 as the
    // signed int 1073807364. Both mean "installed, reboot needed".
    const bool rebootCode = (exitCode == 3010) || (exitCode == 1073807364);
    r.rebootRequired = rebootCode ||
                       low.find("reboot required") != std::string::npos ||
                       low.find("restart") != std::string::npos;

    r.succeeded = (exitCode == 0) || rebootCode ||
                  low.find("driver package added successfully") != std::string::npos ||
                  low.find("successfully installed") != std::string::npos;

    // "Published Name:            oem23.inf"
    std::smatch m;
    static const std::regex pub(R"(published name\s*:\s*([A-Za-z0-9_.-]+\.inf))",
                                std::regex::icase);
    if (std::regex_search(output, m, pub)) {
        r.publishedName = m[1].str();
    }

    return r;
}

} // namespace shiftech::core::drivers::pnputil

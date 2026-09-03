#pragma once

#include "DriverProvider.h"
#include <QString>
#include <string>
#include <vector>

namespace shiftech::core::drivers {

struct ExtractResult {
    bool ok = false;
    QString extractedDir;   // directory containing the driver files
    std::string error;
};

// Unpacks a driver package payload into <packageDir>/extracted/.
//   .zip  -> tar.exe -xf  (bsdtar, Win10 1803+; Shell fallback otherwise)
//   .cab  -> expand.exe -F:* ... (all Windows)
//   folder / .inf -> used in place (no extraction)
// All external calls are time-boxed; never runs a bundled .exe.
class PackageExtractor {
public:
    // payloadPath: a .zip/.cab file, or a directory / .inf already on disk.
    // destDir: where to put the files (created if needed).
    ExtractResult extract(const QString& payloadPath, const QString& destDir,
                          int timeoutMs = 120000);

    // Recursively list *.inf files under dir.
    static std::vector<std::string> findInfFiles(const QString& dir);
};

} // namespace shiftech::core::drivers

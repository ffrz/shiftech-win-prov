#pragma once

#include <string>

// Pure parsing / classification helpers for winget CLI output.
// No process spawning here so this is unit-testable offline.
namespace shiftech::core::applications::winget {

// Decide whether `winget list --id <id> --exact` indicates the package IS installed.
// winget prints a "No installed package found matching input criteria." line (and may
// still exit 0) when nothing matches.
bool listOutputSaysInstalled(int exitCode, const std::string& stdOut);

// Whether an install exit code looks like a transient failure worth one retry
// (source/network/server errors) rather than a permanent one (not found, hash
// mismatch, blocked by policy).
bool isTransientInstallFailure(int exitCode);

} // namespace shiftech::core::applications::winget

#pragma once

#include <string>
#include <vector>

namespace shiftech::core::drivers {

enum class InfVerdict { Ok, Warn, Reject };

struct InfValidation {
    InfVerdict verdict = InfVerdict::Reject;
    bool hasCatalog = false;      // CatalogFile referenced => likely signed
    std::string className;
    std::string classGuid;
    std::vector<std::string> messages;
};

// Static sanity check of an .inf file before handing it to pnputil.
// - Reject: not a parseable INF / no [Version] / no Class(GUID)
// - Warn:   no CatalogFile (unsigned), or a suspicious directive
// - Ok:     well-formed and catalog-referenced
// This does NOT verify a signature (pnputil + Windows enforce that); it filters out
// obviously-bad packages and flags unsigned ones for the ADR-0006 warn+skip policy.
InfValidation validateInf(const std::string& infFilePath);

// Overload for tests: validate already-read INF text.
InfValidation validateInfText(const std::string& infText);

} // namespace shiftech::core::drivers

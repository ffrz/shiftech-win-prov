#include "HardwareId.h"
#include <regex>

namespace shiftech::core::hardware {

PnpId parseHardwareId(std::string_view hwid) {
    PnpId result;
    std::string hwid_str(hwid);
    result.raw = hwid_str;

    if (hwid_str.empty()) {
        return result;
    }

    // Attempt to match PCI\\VEN_XXXX&DEV_XXXX&SUBSYS_XXXX&REV_XX
    std::regex pci_regex(R"(^PCI\\VEN_([0-9A-Fa-f]{4})&DEV_([0-9A-Fa-f]{4})(?:&SUBSYS_([0-9A-Fa-f]{8}))?(?:&REV_([0-9A-Fa-f]{2}))?)", std::regex_constants::icase);
    std::smatch match;
    
    if (std::regex_search(hwid_str, match, pci_regex)) {
        result.bus = "PCI";
        if (match.size() > 1 && match[1].matched) result.vendor = match[1].str();
        if (match.size() > 2 && match[2].matched) result.device = match[2].str();
        if (match.size() > 3 && match[3].matched) result.subsys = match[3].str();
        if (match.size() > 4 && match[4].matched) result.rev = match[4].str();
        return result;
    }

    // Attempt to match USB\\VID_XXXX&PID_XXXX&REV_XXXX
    std::regex usb_regex(R"(^USB\\VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4})(?:&REV_([0-9A-Fa-f]{4}))?)", std::regex_constants::icase);
    if (std::regex_search(hwid_str, match, usb_regex)) {
        result.bus = "USB";
        if (match.size() > 1 && match[1].matched) result.vendor = match[1].str();
        if (match.size() > 2 && match[2].matched) result.device = match[2].str();
        if (match.size() > 3 && match[3].matched) result.rev = match[3].str();
        return result;
    }
    
    // Attempt to match ACPI\\XXXX
    std::regex acpi_regex(R"(^ACPI\\([A-Za-z0-9_-]+))", std::regex_constants::icase);
    if (std::regex_search(hwid_str, match, acpi_regex)) {
        result.bus = "ACPI";
        if (match.size() > 1 && match[1].matched) result.device = match[1].str();
        return result;
    }

    // Attempt to match HDAUDIO\\FUNC_XX&VEN_XXXX&DEV_XXXX
    std::regex hdaudio_regex(R"(^HDAUDIO\\FUNC_[0-9A-Fa-f]{2}&VEN_([0-9A-Fa-f]{4})&DEV_([0-9A-Fa-f]{4})(?:&SUBSYS_([0-9A-Fa-f]{8}))?)", std::regex_constants::icase);
    if (std::regex_search(hwid_str, match, hdaudio_regex)) {
        result.bus = "HDAUDIO";
        if (match.size() > 1 && match[1].matched) result.vendor = match[1].str();
        if (match.size() > 2 && match[2].matched) result.device = match[2].str();
        if (match.size() > 3 && match[3].matched) result.subsys = match[3].str();
        return result;
    }

    // Unrecognized input -> only raw is set
    return result;
}

} // namespace shiftech::core::hardware

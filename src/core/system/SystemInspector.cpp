#include "SystemInspector.h"
#include <windows.h>
#include <QSettings>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>

// For RtlGetVersion
typedef NTSTATUS(WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

namespace shiftech::core::system {

bool SystemInspector::isElevated() {
    HANDLE hToken = nullptr;
    bool elevated = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(elevation);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            elevated = elevation.TokenIsElevated != 0;
        }
        CloseHandle(hToken);
    }
    return elevated;
}

SystemInfo SystemInspector::inspect() {
    SystemInfo info;

    // 1. Version (RtlGetVersion)
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        RtlGetVersionPtr pRtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(reinterpret_cast<void*>(GetProcAddress(hNtdll, "RtlGetVersion")));
        if (pRtlGetVersion) {
            RTL_OSVERSIONINFOW rovi{};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (pRtlGetVersion(&rovi) == 0) {
                info.buildNumber = rovi.dwBuildNumber;
            }
        }
    }

    // 2. Version from Registry
    QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", QSettings::NativeFormat);
    QString productName = reg.value("ProductName").toString();
    // Windows 11 keeps ProductName = "Windows 10 ..." in the registry. Correct it
    // from the build number so user-facing output isn't misleading.
    if (info.buildNumber >= 22000 && productName.contains("Windows 10")) {
        productName.replace("Windows 10", "Windows 11");
    }
    info.productName = productName.toStdString();
    info.editionId = reg.value("EditionID").toString().toStdString();
    
    // DisplayVersion exists on newer Windows 10/11, otherwise fallback to ReleaseId
    if (reg.contains("DisplayVersion")) {
        info.displayVersion = reg.value("DisplayVersion").toString().toStdString();
    } else {
        info.displayVersion = reg.value("ReleaseId").toString().toStdString();
    }

    // 3. Architecture
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        info.arch = Arch::X64;
    } else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        info.arch = Arch::X86;
    } else {
        info.arch = Arch::Unsupported;
    }

    // 4. Elevation
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            info.elevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }

    // 5. wingetAvailable
    QString pathEnv = qEnvironmentVariable("PATH");
    QStringList paths = pathEnv.split(';');
    for (const QString& path : paths) {
        if (path.isEmpty()) continue;
        QString fullPath = path;
        if (!fullPath.endsWith('\\') && !fullPath.endsWith('/')) {
            fullPath += "\\";
        }
        fullPath += "winget.exe";
        if (QFileInfo::exists(fullPath)) {
            info.wingetAvailable = true;
            break;
        }
    }

    // 6. pnputilAvailable
    QString sysRoot = qEnvironmentVariable("SystemRoot");
    if (sysRoot.isEmpty()) {
        sysRoot = "C:\\Windows";
    }
    info.pnputilAvailable = QFileInfo::exists(sysRoot + "\\System32\\pnputil.exe");

    return info;
}

} // namespace shiftech::core::system

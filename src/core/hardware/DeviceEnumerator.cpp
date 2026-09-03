#include "DeviceEnumerator.h"
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <QString>
#include <QDebug>
#include <QSettings>
#include <algorithm>
#include <cctype>

namespace shiftech::core::hardware {

namespace {

class DevInfoList {
    HDEVINFO hDevInfo;
public:
    explicit DevInfoList(HDEVINFO h) : hDevInfo(h) {}
    ~DevInfoList() { if (hDevInfo != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(hDevInfo); }
    HDEVINFO get() const { return hDevInfo; }
    bool isValid() const { return hDevInfo != INVALID_HANDLE_VALUE; }
};

std::string wstrToUtf8(const std::wstring& wstr) {
    return QString::fromStdWString(wstr).toStdString();
}

std::wstring getPropertyString(HDEVINFO devInfo, SP_DEVINFO_DATA* devInfoData, DWORD property) {
    DWORD reqSize = 0;
    DWORD dataType = 0;
    SetupDiGetDeviceRegistryPropertyW(devInfo, devInfoData, property, &dataType, nullptr, 0, &reqSize);
    if (reqSize == 0 || dataType != REG_SZ) return L"";

    std::vector<BYTE> buffer(reqSize);
    if (SetupDiGetDeviceRegistryPropertyW(devInfo, devInfoData, property, &dataType, buffer.data(), reqSize, &reqSize)) {
        return std::wstring(reinterpret_cast<const wchar_t*>(buffer.data()));
    }
    return L"";
}

std::vector<std::string> getPropertyMultiString(HDEVINFO devInfo, SP_DEVINFO_DATA* devInfoData, DWORD property) {
    std::vector<std::string> result;
    DWORD reqSize = 0;
    DWORD dataType = 0;
    SetupDiGetDeviceRegistryPropertyW(devInfo, devInfoData, property, &dataType, nullptr, 0, &reqSize);
    if (reqSize == 0 || dataType != REG_MULTI_SZ) return result;

    std::vector<BYTE> buffer(reqSize);
    if (SetupDiGetDeviceRegistryPropertyW(devInfo, devInfoData, property, &dataType, buffer.data(), reqSize, &reqSize)) {
        const wchar_t* ptr = reinterpret_cast<const wchar_t*>(buffer.data());
        while (*ptr) {
            std::wstring wstr(ptr);
            result.push_back(wstrToUtf8(wstr));
            ptr += wstr.length() + 1;
        }
    }
    return result;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // namespace

std::vector<Device> DeviceEnumerator::enumerate() {
    std::vector<Device> devices;

    DevInfoList devInfo(SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT));
    if (!devInfo.isValid()) {
        qWarning() << "SetupDiGetClassDevsW failed with error:" << GetLastError();
        return devices;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo.get(), i, &devInfoData); ++i) {
        Device dev;

        // Instance ID
        DWORD reqSize = 0;
        SetupDiGetDeviceInstanceIdW(devInfo.get(), &devInfoData, nullptr, 0, &reqSize);
        if (reqSize > 0) {
            std::vector<wchar_t> instIdBuf(reqSize);
            if (SetupDiGetDeviceInstanceIdW(devInfo.get(), &devInfoData, instIdBuf.data(), reqSize, nullptr)) {
                dev.instanceId = wstrToUtf8(instIdBuf.data());
            }
        }

        // Friendly Name / Description
        std::wstring name = getPropertyString(devInfo.get(), &devInfoData, SPDRP_FRIENDLYNAME);
        if (name.empty()) {
            name = getPropertyString(devInfo.get(), &devInfoData, SPDRP_DEVICEDESC);
        }
        dev.name = name.empty() ? "Unknown Device" : wstrToUtf8(name);

        dev.className = wstrToUtf8(getPropertyString(devInfo.get(), &devInfoData, SPDRP_CLASS));
        dev.classGuid = toLower(wstrToUtf8(getPropertyString(devInfo.get(), &devInfoData, SPDRP_CLASSGUID)));
        dev.manufacturer = wstrToUtf8(getPropertyString(devInfo.get(), &devInfoData, SPDRP_MFG));
        
        dev.hardwareIds = getPropertyMultiString(devInfo.get(), &devInfoData, SPDRP_HARDWAREID);
        dev.compatibleIds = getPropertyMultiString(devInfo.get(), &devInfoData, SPDRP_COMPATIBLEIDS);

        // Status + Problem Code
        ULONG status = 0;
        ULONG problem = 0;
        CONFIGRET cr = CM_Get_DevNode_Status(&status, &problem, devInfoData.DevInst, 0);
        
        dev.problemCode = 0;
        dev.status = DeviceStatus::Ok;

        std::wstring driverKey = getPropertyString(devInfo.get(), &devInfoData, SPDRP_DRIVER);
        bool hasDriver = !driverKey.empty();

        if (cr == CR_SUCCESS) {
            if (status & DN_HAS_PROBLEM) {
                dev.problemCode = problem;
                dev.status = DeviceStatus::Problem;
                
                // Unknown device detection
                if (dev.classGuid == "{4d36e97e-e325-11ce-bfc1-08002be10318}" || 
                    (dev.className.empty() && problem == 28 && !hasDriver)) { // 28 is CM_PROB_FAILED_INSTALL
                    dev.status = DeviceStatus::Unknown;
                }
            } else {
                if (!hasDriver) {
                    dev.status = DeviceStatus::NoDriver;
                }
            }
        } else {
            // Fallback if CM_Get_DevNode_Status fails but it's present in SetupAPI
            if (!hasDriver) dev.status = DeviceStatus::NoDriver;
        }

        // CM_PROB_DISABLED is 22
        if (dev.status == DeviceStatus::Problem && dev.problemCode == 22) {
            dev.status = DeviceStatus::Disabled;
        }

        // Driver Info from Registry
        if (hasDriver) {
            QString regPath = QString("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\%1")
                                .arg(QString::fromStdWString(driverKey));
            QSettings reg(regPath, QSettings::NativeFormat);
            dev.driverVersion = reg.value("DriverVersion").toString().toStdString();
            dev.driverProvider = reg.value("ProviderName").toString().toStdString();
            dev.driverDate = reg.value("DriverDate").toString().toStdString();
        }

        devices.push_back(dev);
    }

    return devices;
}

std::vector<Device> DeviceEnumerator::enumerateNeedingDriver() {
    std::vector<Device> all = enumerate();
    std::vector<Device> needing;
    for (const auto& d : all) {
        if (d.needsDriver()) {
            needing.push_back(d);
        }
    }
    return needing;
}

} // namespace shiftech::core::hardware

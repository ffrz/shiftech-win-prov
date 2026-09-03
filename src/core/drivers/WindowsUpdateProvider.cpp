#include "WindowsUpdateProvider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
// MinGW ships no wuguid import library. Including <initguid.h> before <wuapi.h> makes the
// header's DEFINE_GUIDs allocate storage in this TU instead of just declaring them.
#include <initguid.h>
#include <wuapi.h>

// MinGW's <wuapi.h> only declares the base IUpdate / IUpdateSearcher / IUpdateSession /
// ISearchResult interfaces — not IWindowsDriverUpdate5, IUpdateServiceManager, etc. So the
// driver-specific properties (DriverHardwareID, DriverProvider, DriverVerDate) and the
// download URLs are read via IDispatch late-binding, which works regardless of headers.

namespace shiftech::core::drivers {

namespace {

template <class T>
struct Com {
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T** put() { return &p; }
    T* operator->() const { return p; }
    T* get() const { return p; }
    explicit operator bool() const { return p != nullptr; }
    void reset() { if (p) { p->Release(); p = nullptr; } }
};

struct BStr {
    BSTR b = nullptr;
    ~BStr() { if (b) SysFreeString(b); }
    BSTR* put() { return &b; }
    std::string str() const {
        return b ? QString::fromWCharArray(b, static_cast<int>(SysStringLen(b))).toStdString()
                 : std::string();
    }
};

struct Variant {
    VARIANT v;
    Variant() { VariantInit(&v); }
    ~Variant() { VariantClear(&v); }
    VARIANT* put() { return &v; }
};

// disp->PropertyName  (no args)
bool getProp(IDispatch* disp, const wchar_t* name, VARIANT* out) {
    if (!disp) return false;
    DISPID id = 0;
    LPOLESTR n = const_cast<LPOLESTR>(name);
    if (FAILED(disp->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id))) return false;
    DISPPARAMS noArgs = {nullptr, nullptr, 0, 0};
    return SUCCEEDED(disp->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET,
                                  &noArgs, out, nullptr, nullptr));
}

// disp->Method(index) returning an IDispatch* (collection item accessor)
IDispatch* getItem(IDispatch* disp, const wchar_t* name, long index) {
    if (!disp) return nullptr;
    DISPID id = 0;
    LPOLESTR n = const_cast<LPOLESTR>(name);
    if (FAILED(disp->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id))) return nullptr;
    VARIANT arg;
    VariantInit(&arg);
    arg.vt = VT_I4;
    arg.lVal = index;
    DISPPARAMS dp = {&arg, nullptr, 1, 0};
    Variant res;
    if (FAILED(disp->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp,
                            res.put(), nullptr, nullptr)))
        return nullptr;
    if (res.v.vt == VT_DISPATCH && res.v.pdispVal) {
        res.v.pdispVal->AddRef();
        return res.v.pdispVal;
    }
    return nullptr;
}

std::string propStr(IDispatch* disp, const wchar_t* name) {
    Variant v;
    if (!getProp(disp, name, v.put())) return {};
    if (v.v.vt == VT_BSTR && v.v.bstrVal)
        return QString::fromWCharArray(v.v.bstrVal, static_cast<int>(SysStringLen(v.v.bstrVal)))
            .toStdString();
    return {};
}

long propLong(IDispatch* disp, const wchar_t* name) {
    Variant v;
    if (!getProp(disp, name, v.put())) return 0;
    if (v.v.vt == VT_I4) return v.v.lVal;
    if (v.v.vt == VT_I2) return v.v.iVal;
    return 0;
}

IDispatch* propDisp(IDispatch* disp, const wchar_t* name) {
    Variant v;
    if (!getProp(disp, name, v.put())) return nullptr;
    if (v.v.vt == VT_DISPATCH && v.v.pdispVal) {
        v.v.pdispVal->AddRef();
        return v.v.pdispVal;
    }
    return nullptr;
}

bool idMatches(const std::string& a, const std::string& b) {
    return !a.empty() && !b.empty() && _stricmp(a.c_str(), b.c_str()) == 0;
}

const char* osToken(TargetSystem::OsFamily os) {
    switch (os) {
        case TargetSystem::OsFamily::Win7: return "win7";
        case TargetSystem::OsFamily::Win8: return "win8";
        case TargetSystem::OsFamily::Win10: return "win10";
        case TargetSystem::OsFamily::Win11: return "win11";
    }
    return "win10";
}

} // namespace

WindowsUpdateProvider::WindowsUpdateProvider(std::string scanPackagePath, int timeoutMs)
    : m_scanPackagePath(std::move(scanPackagePath)), m_timeoutMs(timeoutMs) {}

DriverSearchResult WindowsUpdateProvider::search(const hardware::Device& device,
                                                 const TargetSystem& target) {
    DriverSearchResult result;

    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool weInit = (hrInit == S_OK || hrInit == S_FALSE);
    struct Uninit {
        bool doIt;
        ~Uninit() { if (doIt) CoUninitialize(); }
    } uninit{weInit};
    if (hrInit != S_OK && hrInit != S_FALSE && hrInit != RPC_E_CHANGED_MODE) {
        result.notFoundReason = "CoInitializeEx failed";
        return result;
    }

    Com<IUpdateSession> session;
    if (FAILED(CoCreateInstance(CLSID_UpdateSession, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IUpdateSession,
                                reinterpret_cast<void**>(session.put()))) ||
        !session) {
        result.notFoundReason = "cannot create UpdateSession (WUA unavailable)";
        return result;
    }

    Com<IUpdateSearcher> searcher;
    if (FAILED(session->CreateUpdateSearcher(searcher.put())) || !searcher) {
        result.notFoundReason = "CreateUpdateSearcher failed";
        return result;
    }

    QString scanCab = QString::fromStdString(m_scanPackagePath);
    if (scanCab.isEmpty()) {
        const QString def =
            QDir(QCoreApplication::applicationDirPath()).filePath("cache/wsusscn2.cab");
        if (QFileInfo::exists(def)) scanCab = def;
    }
    // Offline (wsusscn2.cab) scanning needs IUpdateServiceManager, which MinGW's headers
    // do not declare. If a cab was requested we still run an online search and note it;
    // full offline support is a follow-up if a real air-gapped need appears.
    searcher->put_ServerSelection(ssWindowsUpdate);
    searcher->put_Online(VARIANT_TRUE);

    BSTR criteria = SysAllocString(L"IsInstalled=0 and Type='Driver'");
    Com<ISearchResult> sr;
    const HRESULT hrSearch = searcher->Search(criteria, sr.put());
    SysFreeString(criteria);
    if (FAILED(hrSearch) || !sr) {
        result.notFoundReason = "WUA search failed (hr=" + std::to_string(hrSearch) + ")";
        return result;
    }

    Com<IUpdateCollection> updates;
    if (FAILED(sr->get_Updates(updates.put())) || !updates) {
        result.notFoundReason = "no update collection";
        return result;
    }
    LONG count = 0;
    updates->get_Count(&count);

    for (LONG i = 0; i < count; ++i) {
        Com<IUpdate> upd;
        if (FAILED(updates->get_Item(i, upd.put())) || !upd) continue;

        IDispatch* d = nullptr;
        upd->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&d));
        struct DRel {
            IDispatch* p;
            ~DRel() { if (p) p->Release(); }
        } drel{d};
        if (!d) continue;

        const std::string driverHwId = propStr(d, L"DriverHardwareID");

        MatchVia via = MatchVia::Unspecified;
        std::string matchedId;
        for (const auto& id : device.hardwareIds)
            if (idMatches(id, driverHwId)) { via = MatchVia::HardwareId; matchedId = id; break; }
        if (via == MatchVia::Unspecified)
            for (const auto& id : device.compatibleIds)
                if (idMatches(id, driverHwId)) {
                    via = MatchVia::CompatibleId;
                    matchedId = id;
                    break;
                }
        if (via == MatchVia::Unspecified) continue;

        DriverPackage pkg;
        pkg.driverName = propStr(d, L"Title");
        pkg.provider = propStr(d, L"DriverProvider");
        pkg.version = propStr(d, L"DriverVerDate"); // sortable date string
        if (pkg.version.empty()) pkg.version = "0";
        pkg.arch = target.arch;
        pkg.supportedOs = {osToken(target.os)};
        pkg.matchedVia = via;
        pkg.matchedId = matchedId;

        // DownloadContents -> [0].DownloadUrl
        if (IDispatch* contents = propDisp(d, L"DownloadContents")) {
            struct CRel { IDispatch* p; ~CRel() { if (p) p->Release(); } } crel{contents};
            const long cc = propLong(contents, L"Count");
            for (long k = 0; k < cc; ++k) {
                IDispatch* item = getItem(contents, L"Item", k);
                if (!item) continue;
                struct IRel { IDispatch* p; ~IRel() { if (p) p->Release(); } } irel{item};
                const std::string u = propStr(item, L"DownloadUrl");
                if (!u.empty()) {
                    pkg.downloadUrl = u;
                    const bool cab = u.size() > 4 &&
                                     _stricmp(u.c_str() + u.size() - 4, ".cab") == 0;
                    pkg.packageType = cab ? PackageType::InfCab : PackageType::InfZip;
                    break;
                }
            }
        }
        if (pkg.downloadUrl.empty()) continue;

        result.candidates.push_back(pkg);
    }

    if (!result.candidates.empty()) {
        result.found = true;
    } else {
        result.notFoundReason =
            count == 0 ? "Windows Update has no driver updates for this machine"
                       : "no Windows Update driver matched this device's hardware IDs";
    }
    return result;
}

} // namespace shiftech::core::drivers

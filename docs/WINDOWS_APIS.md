# Windows APIs used per module

Rule: **prefer a Windows API over parsing tool output** where the API is more reliable.
`pnputil` is the exception — it is the supported way to *install* a driver package.

## hardware/ — DeviceEnumerator

Enumerate present devices and read their config-manager state.

| Need | API | Notes |
|------|-----|-------|
| Enumerate device instances | `SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES \| DIGCF_PRESENT)` then `SetupDiEnumDeviceInfo` | present devices across all classes |
| Instance ID | `SetupDiGetDeviceInstanceId` | e.g. `PCI\VEN_10EC&DEV_8168\...` |
| Hardware IDs / Compatible IDs | `SetupDiGetDeviceRegistryProperty` with `SPDRP_HARDWAREID` / `SPDRP_COMPATIBLEIDS` | REG_MULTI_SZ |
| Friendly name / description | `SPDRP_FRIENDLYNAME`, fallback `SPDRP_DEVICEDESC` | |
| Class / ClassGUID | `SPDRP_CLASS`, `SPDRP_CLASSGUID` | |
| Manufacturer | `SPDRP_MFG` | |
| Status + problem code | `CM_Get_DevNode_Status(&status, &problem, devInst, 0)` | `DN_HAS_PROBLEM`, `CM_PROB_*` (e.g. 28 = no driver installed) |
| Driver version / provider / date | `SetupDiGetDeviceRegistryProperty(SPDRP_DRIVER)` → open `HKLM\SYSTEM\CurrentControlSet\Control\Class\{classGuid}\{nnnn}` → `DriverVersion`, `ProviderName`, `DriverDate` | or `SetupDiGetDriverInfoDetail` after `SetupDiBuildDriverInfoList(SPDIT_COMPATDRIVER)` |
| "Unknown device" detection | class GUID `{4d36e97e-e325-11ce-bfc1-08002be10318}` or empty class + problem 28 | |

Link libraries: `setupapi`, `cfgmgr32`, `newdev` (only if using `DiInstallDriver` later),
`advapi32` (registry), `version`.

**Do not** shell `pnputil /enum-devices` for this data.

## drivers/ — DriverInstaller

| Need | Mechanism | Notes |
|------|-----------|-------|
| Add + install a driver package | `pnputil /add-driver "<path>\driver.inf" /install` | per-INF; capture stdout/stderr/exit code via `QProcess`; per-INF timeout |
| (alt, programmatic) | `DiInstallDriverW` (newdev.h) | consider later; `pnputil` is simpler and logs cleanly |
| List installed OEM packages | `pnputil /enum-drivers` | for reporting only |

Never execute a driver `setup.exe` blindly. Only `.inf`-based install in V1.

## drivers/ — DriverVerifier

Re-run `DeviceEnumerator`; compare `status` / `problemCode` / `driverVersion` before vs
after. `CM_PROB_*` going to 0 = success. `DN_NEED_RESTART` in status ⇒ `RequiresReboot`.

## applications/ — WinGetProvider

| Need | Command | Notes |
|------|---------|-------|
| Is installed | `winget list --id <Id> --exact` (exit code / output) | also accept an MSI/ARP check as fallback |
| Install | `winget install --id <Id> --exact --silent --accept-package-agreements --accept-source-agreements --disable-interactivity` | capture output + exit code |
| Availability | resolve `winget.exe` on PATH; `winget --version` | Win7/8 have no winget → provider reports unavailable |

## system/ — SystemInspector

| Need | API | Notes |
|------|-----|-------|
| Windows version / build | `RtlGetVersion` (ntdll) | `GetVersionEx` is shimmed; prefer `RtlGetVersion`. Also read `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion` (`CurrentBuild`, `DisplayVersion`, `ProductName`, `EditionID`) |
| Architecture | `GetNativeSystemInfo` → `PROCESSOR_ARCHITECTURE_AMD64` / `_INTEL` | reject ARM64 |
| Elevation | `OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY)` + `GetTokenInformation(TokenElevation)` | |
| UEFI vs BIOS | `GetFirmwareEnvironmentVariable(L"", L"{00000000-0000-0000-0000-000000000000}", ...)` → `ERROR_INVALID_FUNCTION` means BIOS | or `Kernel32!GetFirmwareType` (Win8+) |
| Internet connectivity | `INetworkListManager` (COM) or a HEAD request to a known host with timeout | don't hard-fail provisioning on this alone |
| `pnputil` present | `%SystemRoot%\System32\pnputil.exe` exists | present on all targets Vista+ |

Link libraries: `ntdll` (RtlGetVersion), `advapi32`, `ole32`/`oleaut32` (if using COM NLM).

## Elevation model

`scan` / detection: works unelevated (some fields may be reduced).
Driver install / any device mutation: requires elevation. Detect and fail clearly; do not
silently self-elevate in V1. A future version may relaunch with a UAC prompt.

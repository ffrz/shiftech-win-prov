# Milestone 1 — Skeleton + Hardware Detection

**Goal:** a building project with `provisioner.exe scan` that lists every present device and
clearly flags the ones that need a driver, plus passing offline unit tests.

Read first: [../ARCHITECTURE.md](../ARCHITECTURE.md), [../BUILD.md](../BUILD.md),
[../WINDOWS_APIS.md](../WINDOWS_APIS.md), [../TESTING.md](../TESTING.md), and
[../../AGENTS.md](../../AGENTS.md).

Work top to bottom. Report after each task with what changed + build/test result.

---

## Task 1 — Build system skeleton

- [ ] Root `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.24)`, project
      `ShiftechWinProvisioner`, `CMAKE_CXX_STANDARD 20` / `CXX_STANDARD_REQUIRED ON`.
- [ ] Options: `SHIFTECH_BUILD_GUI` (OFF), `SHIFTECH_BUILD_TESTS` (ON),
      `SHIFTECH_INTEGRATION_TESTS` (OFF), `SHIFTECH_WARNINGS_AS_ERRORS` (ON).
- [ ] `find_package(Qt6 REQUIRED COMPONENTS Core Network)` (add `Test` when tests on,
      `Widgets` only when GUI on). `qt_standard_project_setup()`.
- [ ] Warning flags for GCC/MinGW: `-Wall -Wextra -Wpedantic` (+ `-Werror` when the option
      is on). Put shared compile options in `cmake/CompilerWarnings.cmake`.
- [ ] `add_subdirectory(src/core)`, `src/cli`, and `tests` (guarded by the option).
- [ ] `src/core/CMakeLists.txt`: `add_library(shiftech_core STATIC ...)`, link
      `Qt6::Core Qt6::Network` + Windows libs `setupapi cfgmgr32 advapi32 version`.
      `target_include_directories(shiftech_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`.
- [ ] `src/cli/CMakeLists.txt`: `qt_add_executable(provisioner ...)`, link
      `shiftech_core Qt6::Core`. Console subsystem (`WIN32_EXECUTABLE OFF`).
- [ ] Everything compiles with a single stub `.cpp` per target (empty `main` for CLI).
- [ ] `scripts\configure.bat` and `scripts\build.bat` succeed. **Report the exact output.**

**Checkpoint:** clean build of all three targets, no warnings.

---

## Task 2 — `Device` model

- [ ] `src/core/hardware/Device.h` — the struct + `DeviceStatus` enum exactly as in
      [../ARCHITECTURE.md](../ARCHITECTURE.md) §3 (`hardware/`). Plain types only.
- [ ] `bool Device::needsDriver() const` in `Device.cpp`: true when
      `status == NoDriver || status == Unknown`, or `status == Problem` with a problem code
      in the "needs driver" set (28, 1, 3, 24, 31 — comment each).
- [ ] `src/core/hardware/HardwareId.h/.cpp` — pure parsing helpers:
      - `struct PnpId { std::string bus; std::string vendor; std::string device;
        std::string subsys; std::string rev; std::string raw; }`
      - `PnpId parseHardwareId(std::string_view)` handling `PCI\VEN_10EC&DEV_8168&SUBSYS_...&REV_..`,
        `USB\VID_0BDA&PID_8153`, `ACPI\...`, `HDAUDIO\...`, and returning a mostly-empty
        `PnpId` (with `raw` set) for unrecognized input. No exceptions.
      - Keep this file free of any Windows/Qt header so it unit-tests anywhere.

**Checkpoint:** header compiles; no OS calls in `HardwareId.*`.

---

## Task 3 — `DeviceEnumerator` (SetupAPI / CfgMgr32)

- [ ] `src/core/hardware/DeviceEnumerator.h/.cpp`.
      `std::vector<Device> enumerate()` and `std::vector<Device> enumerateNeedingDriver()`.
- [ ] Use `SetupDiGetClassDevs(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT)`
      + `SetupDiEnumDeviceInfo`. RAII wrapper around the `HDEVINFO` (destroy with
      `SetupDiDestroyDeviceInfoList`).
- [ ] Per device, fill from `SetupDiGetDeviceRegistryProperty`:
      `SPDRP_FRIENDLYNAME` (fallback `SPDRP_DEVICEDESC`), `SPDRP_CLASS`, `SPDRP_CLASSGUID`,
      `SPDRP_MFG`, `SPDRP_HARDWAREID` (REG_MULTI_SZ → vector), `SPDRP_COMPATIBLEIDS`.
- [ ] Instance ID via `SetupDiGetDeviceInstanceIdW`.
- [ ] Status + problem code via `CM_Get_DevNode_Status(&status, &problem, devInst, 0)`.
      Map to `DeviceStatus`: `DN_HAS_PROBLEM` + code → `Problem` (or `Unknown` when the
      class GUID is the unknown-device GUID `{4d36e97e-...}` / problem 28 with no driver);
      no driver key + no problem → `NoDriver`; `CM_PROB_DISABLED` (22) → `Disabled`;
      otherwise `Ok`.
- [ ] Driver version/provider/date: read `SPDRP_DRIVER` to get the class-subkey path, open
      `HKLM\SYSTEM\CurrentControlSet\Control\Class\{ClassGUID}\{nnnn}`, read `DriverVersion`,
      `ProviderName`, `DriverDate`. Missing values → empty strings, not errors.
- [ ] All Win32 calls check their return; log a warning (see Task 5 logger or a simple
      `qWarning` for now) and skip a device rather than aborting enumeration.
- [ ] Convert wide strings to UTF-8 for the model.

**Checkpoint:** `enumerate()` returns a non-empty vector on this machine; no crash; no
leaked handles (review RAII).

---

## Task 4 — `SystemInspector` (basic)

- [ ] `src/core/system/SystemInfo.h` — struct: `productName`, `editionId`, `displayVersion`,
      `int buildNumber`, `enum Arch { X86, X64, Unsupported }`, `bool elevated`,
      `bool wingetAvailable`, `bool pnputilAvailable`. (UEFI/BIOS + connectivity are
      Milestone 6 — leave TODO comments.)
- [ ] `src/core/system/SystemInspector.h/.cpp` — `SystemInfo inspect()`.
      - Version: `RtlGetVersion` (via `GetProcAddress` on `ntdll`) for major/minor/build;
        `ProductName` / `EditionID` / `DisplayVersion` from
        `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion`.
      - Arch: `GetNativeSystemInfo` → `PROCESSOR_ARCHITECTURE_AMD64`/`_INTEL`; anything else
        → `Unsupported`.
      - Elevation: `OpenProcessToken` + `GetTokenInformation(TokenElevation)`.
      - `wingetAvailable`: `winget.exe` resolvable on PATH (search `PATH` dirs; do not run it).
      - `pnputilAvailable`: `%SystemRoot%\System32\pnputil.exe` exists.

**Checkpoint:** `inspect()` returns correct values for this machine (sanity-check against
`winver` and `whoami /groups`).

---

## Task 5 — `provisioner.exe scan`

- [ ] Minimal arg handling in `src/cli/main.cpp` (use `QCommandLineParser`): subcommand
      `scan`; global flags `--json`, `--no-color`, `-v/--verbose`. Unknown → usage + exit 3.
- [ ] `src/cli/commands/ScanCommand.h/.cpp` — calls `SystemInspector::inspect()` and
      `DeviceEnumerator::enumerate()`, renders output.
- [ ] Human output (see [../CLI.md](../CLI.md) for the exact shape):
      - `System` block: product name (build), arch, `Elevated: yes/no`,
        `winget: yes/no  pnputil: yes/no`.
      - `Devices needing a driver (N)` — grouped first, each line:
        `[NO DRIVER] / [UNKNOWN] / [PROBLEM nn]  <first hardware id>  <name>`.
      - `All devices (N)` — one line each: `[OK] <class>  <name>  <driverVersion> <provider> <date>`.
- [ ] `--json` output: a single object `{ "system": {...}, "devices": [ ... ],
      "needingDriver": [ ... ] }` built with `QJsonDocument`. `devices` entries carry every
      `Device` field.
- [ ] Exit code: 0 normally; 1 if any device needs a driver (so scripts can detect it);
      2 on a fatal error (e.g. enumeration completely failed); 3 on bad usage.
- [ ] Print a one-line note when not elevated: "Some driver fields may be limited (not
      elevated)."

**Checkpoint:** `scripts\run.bat scan` and `scripts\run.bat scan --json` both work on this
machine. Paste real output.

---

## Task 6 — Unit tests

- [ ] `tests/CMakeLists.txt`: for each test, `qt_add_executable` + `add_test`, link
      `shiftech_core Qt6::Test`. Name tests so `ctest -R hardwareid` works.
- [ ] `tests/unit/test_hardwareid.cpp` (QtTest): parse a real PCI id (VEN/DEV/SUBSYS/REV),
      a USB id (VID/PID), an ACPI id, an HDAUDIO id, empty string, and garbage — assert the
      parsed fields and that garbage yields `raw` set and other fields empty.
- [ ] `tests/unit/test_deviceclassify.cpp` (QtTest): construct `Device` values and assert
      `needsDriver()` for: NoDriver, Unknown, Problem(28), Problem(22 disabled → false),
      Ok → false.
- [ ] Tests must not touch the OS, devices, or network.
- [ ] `scripts\test.bat` green.

**Checkpoint:** `ctest --output-on-failure` all pass.

---

## Exit criteria — DONE (2026-09-03)

- [x] Clean build of `shiftech_core`, `provisioner.exe`, and the test suite, zero warnings
      with `-Wall -Wextra -Wpedantic -Werror` (Qt 6.6.2 / MinGW 11.2.0 / Ninja).
- [x] `provisioner.exe scan` lists 128 devices, flags 2 as needing a driver; `--json`
      valid with all `Device` fields.
- [x] `SystemInspector` matches reality — "Windows 11 Pro (build 22621) x64, Elevated: no,
      winget: yes, pnputil: yes" (registry-ProductName quirk corrected from build number).
- [x] Unit tests pass offline: `test_hardwareid`, `test_deviceclassify` (+ 5 more added
      during hardening).
- [x] `src/core/` has no Qt Widgets/GUI include.

### Deviations / notes
- `tests_stub.cpp` / orphan `shiftech_tests` target removed.
- `DeviceEnumerator` reads driver version/provider/date via `QSettings` on the class
  registry key rather than raw `advapi32` — acceptable, keeps the code short.
- One enumerated device shows `[NO DRIVER]` with an empty hardware-ID list (a software
  device); harmless, but a future pass could suppress ID-less devices from the
  "needing a driver" group.

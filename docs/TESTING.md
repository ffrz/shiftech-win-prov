# Testing

Framework: **QtTest** (bundled with Qt — no extra dependency). Runner: `ctest`.
Target: `shiftech_tests` (built when `SHIFTECH_BUILD_TESTS=ON`, the default).

## Safety rules

- **Never** run destructive driver tests on the main development machine.
- Unit tests must not: touch real devices, hit the network, call `pnputil` / `winget`,
  install anything, or write outside the build/temp dir.
- Integration tests that call `pnputil` / `winget` are compiled only when
  `SHIFTECH_INTEGRATION_TESTS=ON` and must be run on a **VM or dedicated test machine**.
- Use a local fixture HTTP server (QtTest can host one) for downloader tests — no external hosts.

## Unit tests (must exist, platform-independent where possible)

| Area | What to cover |
|------|---------------|
| Hardware ID parsing | split `PCI\VEN_10EC&DEV_8168&SUBSYS_...&REV_..`; extract VEN/DEV/SUBSYS/REV; handle `USB\`, `ACPI\`, `HDAUDIO\`, malformed input |
| Device classification | `status`/`problemCode` → `needsDriver()`; unknown-device GUID; disabled vs problem |
| Driver matching | Hardware ID vs Compatible ID ranking; arch/OS filtering; pick best candidate; no-match reason |
| Profile parsing | valid JSON/YAML; missing required field; unknown key; duplicate id; `required` default |
| Provisioning state | legal transitions; illegal transition rejected; serialize/deserialize round-trip; per-item result accumulation |
| Error handling | one failing item doesn't abort the loop; failures recorded with cause; summary counts correct |
| Cache id | `packageId` is deterministic for the same inputs; changes when version/url changes |
| Report builder | totals match inputs; "SUCCESS" vs "SUCCESS WITH WARNINGS" vs "FAILED" logic; JSON shape stable |

## Integration tests (gated, VM only)

| Area | What to cover |
|------|---------------|
| `pnputil` execution | add a benign signed INF, capture exit code/output, verify via re-enumeration, then remove it |
| `winget` detection | `isInstalled` true/false for a known present/absent package |
| Package installation | install a small package silently, detect it, (optionally) uninstall |
| Driver verification | before/after problem-code diff on a deliberately broken test device (VM) |

## Layout

```
tests/
  CMakeLists.txt
  unit/
    test_hardwareid.cpp
    test_deviceclassify.cpp
    test_drivermatch.cpp
    test_profileloader.cpp
    test_provisioningstate.cpp
    test_errorhandling.cpp
    test_reportbuilder.cpp
  integration/           # compiled only with SHIFTECH_INTEGRATION_TESTS=ON
    test_pnputil.cpp
    test_winget.cpp
  fixtures/
    driver_index.json
    profiles/
    packages/
```

## Running

```bat
scripts\test.bat
:: or
ctest --test-dir build --output-on-failure
ctest --test-dir build -R hardwareid           :: one test
```

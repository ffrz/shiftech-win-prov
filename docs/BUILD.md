# Build

## Toolchain on this machine (detected)

| Tool | Path | Version |
|------|------|---------|
| Qt (default) | `D:\bin\Qt\6.6.2\mingw_64` | 6.6.2 |
| Qt (alt) | `D:\bin\Qt\6.11.1\mingw_64` | 6.11.1 |
| MinGW GCC (default) | `D:\bin\Qt\Tools\mingw1120_64\bin` | 11.2.0 |
| MinGW GCC (alt) | `D:\bin\Qt\Tools\mingw1310_64\bin` | 13.1.0 |
| CMake | `D:\bin\Qt\Tools\CMake_64\bin\cmake.exe` | 3.30.5 |
| Ninja | `D:\bin\Qt\Tools\Ninja\ninja.exe` | bundled |
| Git | `C:\Program Files\Git\cmd\git.exe` | — |
| winget | `%LOCALAPPDATA%\Microsoft\WindowsApps\winget.exe` | — |

**No MSVC / Visual Studio toolchain is installed.** Build with MinGW GCC only.

### Why Qt 6.6.2 + MinGW 11.2.0 is the default kit

- 6.6.2 is the matched pairing for the `mingw1120_64` toolchain that ships with it and is a
  conservative, widely-deployed Qt 6 release — good for the Win7→Win11 target range.
- 6.11.1 + GCC 13.1 is available if a newer Qt feature is needed; switch by changing
  `QT_ROOT` / `MINGW_ROOT` in `scripts/env.bat`. If you switch, note it in
  [DECISIONS.md](DECISIONS.md).
- **Windows 7 caveat:** Qt 6 officially requires Windows 10+. Running on Windows 7/8 is a
  known open risk — see [DECISIONS.md](DECISIONS.md) ADR-0003. Options to evaluate during
  Milestone 6/7: a Qt 5.15 build of the GUI, a static Qt build, or shipping the CLI (which
  needs only Qt Core / can be made Qt-free) as the Win7 story. The **core + CLI** should be
  kept buildable with minimal Qt so this stays open.

## Layout produced by a configure

```
build/                     # gitignored
  shiftech_core.a
  provisioner.exe
  shiftech_gui.exe         # if SHIFTECH_BUILD_GUI=ON
  shiftech_tests.exe       # if SHIFTECH_BUILD_TESTS=ON
```

## CMake options

| Option | Default | Meaning |
|--------|---------|---------|
| `SHIFTECH_BUILD_GUI` | `OFF` | build the Qt Widgets front-end |
| `SHIFTECH_BUILD_TESTS` | `ON` | build the QtTest suite |
| `SHIFTECH_INTEGRATION_TESTS` | `OFF` | enable tests that call `pnputil` / `winget` (VM only) |
| `SHIFTECH_WARNINGS_AS_ERRORS` | `ON` | `-Werror` |

## Commands

Use the wrappers in `scripts/` (they set PATH for Qt + MinGW + Ninja):

```bat
scripts\configure.bat            :: cmake -G Ninja -S . -B build  (Release)
scripts\configure.bat Debug      :: Debug build
scripts\build.bat                :: cmake --build build
scripts\test.bat                 :: ctest --test-dir build --output-on-failure
scripts\run.bat scan             :: build\provisioner.exe scan
```

Manual equivalent:

```bat
set "QT_ROOT=D:\bin\Qt\6.6.2\mingw_64"
set "MINGW=D:\bin\Qt\Tools\mingw1120_64\bin"
set "CMAKE=D:\bin\Qt\Tools\CMake_64\bin\cmake.exe"
set "PATH=%QT_ROOT%\bin;%MINGW%;D:\bin\Qt\Tools\Ninja;%PATH%"

"%CMAKE%" -G Ninja -S . -B build ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH=%QT_ROOT% ^
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++

"%CMAKE%" --build build
ctest --test-dir build --output-on-failure
```

## Deploying the built exe

For a technician machine, run `windeployqt` from the chosen Qt kit against
`provisioner.exe` / `shiftech_gui.exe` to gather Qt DLLs, or link Qt statically (evaluate
in a later milestone). `pnputil` and `winget` are expected to already exist on the target.

## Running

`provisioner.exe` must run **elevated** for driver install. `scan` works unelevated but
some device/driver fields may be limited. The CLI must detect elevation and say so.

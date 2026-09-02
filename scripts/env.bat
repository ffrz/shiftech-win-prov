@echo off
rem Shared environment for build scripts. Edit paths here to switch Qt kit / compiler.
rem Detected on this machine 2026-09-03.

set "QT_ROOT=D:\bin\Qt\6.6.2\mingw_64"
set "MINGW_ROOT=D:\bin\Qt\Tools\mingw1120_64"
set "CMAKE_EXE=D:\bin\Qt\Tools\CMake_64\bin\cmake.exe"
set "NINJA_DIR=D:\bin\Qt\Tools\Ninja"
set "CTEST_EXE=D:\bin\Qt\Tools\CMake_64\bin\ctest.exe"

rem Alternate kit (Qt 6.11.1 + GCC 13.1) — uncomment to use:
rem set "QT_ROOT=D:\bin\Qt\6.11.1\mingw_64"
rem set "MINGW_ROOT=D:\bin\Qt\Tools\mingw1310_64"

set "PATH=%QT_ROOT%\bin;%MINGW_ROOT%\bin;%NINJA_DIR%;%PATH%"

set "REPO_ROOT=%~dp0.."
set "BUILD_DIR=%REPO_ROOT%\build"

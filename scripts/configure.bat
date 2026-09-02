@echo off
setlocal
call "%~dp0env.bat"

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"

"%CMAKE_EXE%" -G Ninja -S "%REPO_ROOT%" -B "%BUILD_DIR%" ^
  -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
  -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
  -DCMAKE_C_COMPILER=gcc ^
  -DCMAKE_CXX_COMPILER=g++ ^
  -DSHIFTECH_BUILD_TESTS=ON ^
  %SHIFTECH_EXTRA_CMAKE_ARGS%

endlocal

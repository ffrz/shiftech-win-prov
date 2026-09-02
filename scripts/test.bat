@echo off
setlocal
call "%~dp0env.bat"
if not exist "%BUILD_DIR%\CMakeCache.txt" call "%~dp0build.bat"
"%CTEST_EXE%" --test-dir "%BUILD_DIR%" --output-on-failure %*
endlocal

@echo off
setlocal
call "%~dp0env.bat"
if not exist "%BUILD_DIR%\CMakeCache.txt" call "%~dp0configure.bat" %*
"%CMAKE_EXE%" --build "%BUILD_DIR%" %SHIFTECH_BUILD_ARGS%
endlocal

@echo off
setlocal
call "%~dp0env.bat"
if not exist "%BUILD_DIR%\provisioner.exe" call "%~dp0build.bat"
"%BUILD_DIR%\provisioner.exe" %*
endlocal

<#
.SYNOPSIS
    Build Shiftech Win Provisioner and stage a portable release folder.

.DESCRIPTION
    Configures with CMake + Ninja (MinGW), builds Release, optionally runs the tests,
    then assembles a self-contained folder under dist\ that can be copied to a USB drive:

        dist\ShiftechWinProvisioner\
            provisioner.exe
            app.exe          (unless -NoGui)
            <Qt runtime DLLs + plugins>   (via windeployqt)
            profiles\*.json
            cache\                    (empty - populated by 'drivers resolve --download')
            logs\                     (empty)
            README-PORTABLE.txt

.PARAMETER Config
    CMAKE_BUILD_TYPE. Default: Release.

.PARAMETER NoGui
    Skip the Qt Widgets GUI (CLI only - smaller, and the Windows 7/8 story per ADR-0003).

.PARAMETER NoTests
    Skip ctest after the build.

.PARAMETER Clean
    Delete the build directory first.

.PARAMETER Zip
    Also produce dist\ShiftechWinProvisioner-<version>.zip.

.EXAMPLE
    .\scripts\build-release.ps1
.EXAMPLE
    .\scripts\build-release.ps1 -Clean -Zip
.EXAMPLE
    .\scripts\build-release.ps1 -NoGui -NoTests
#>
[CmdletBinding()]
param(
    [string]$Config = 'Release',
    [switch]$NoGui,
    [switch]$NoTests,
    [switch]$Clean,
    [switch]$Zip
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# --- toolchain (keep in sync with scripts\env.bat) ---
$QtRoot    = 'D:\bin\Qt\6.6.2\mingw_64'
$MinGWRoot = 'D:\bin\Qt\Tools\mingw1120_64'
$CMake     = 'D:\bin\Qt\Tools\CMake_64\bin\cmake.exe'
$CTest     = 'D:\bin\Qt\Tools\CMake_64\bin\ctest.exe'
$NinjaDir  = 'D:\bin\Qt\Tools\Ninja'

$RepoRoot  = Split-Path -Parent $PSScriptRoot
$BuildDir  = Join-Path $RepoRoot 'build'
$DistRoot  = Join-Path $RepoRoot 'dist'
$AppName   = 'ShiftechWinProvisioner'
$StageDir  = Join-Path $DistRoot $AppName

function Assert-Path([string]$p, [string]$what) {
    if (-not (Test-Path $p)) { throw "$what not found: $p" }
}
Assert-Path $QtRoot    'Qt kit'
Assert-Path $MinGWRoot 'MinGW toolchain'
Assert-Path $CMake     'cmake.exe'

$env:PATH = "$QtRoot\bin;$MinGWRoot\bin;$NinjaDir;$env:PATH"

Write-Host "== Shiftech Win Provisioner - build-release ==" -ForegroundColor Cyan
Write-Host "   config : $Config"
Write-Host "   gui    : $(if ($NoGui) { 'no' } else { 'yes' })"
Write-Host "   qt     : $QtRoot"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "-- cleaning $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

# --- configure ---
$guiFlag = if ($NoGui) { 'OFF' } else { 'ON' }
& $CMake -G Ninja -S $RepoRoot -B $BuildDir `
    "-DCMAKE_BUILD_TYPE=$Config" `
    "-DCMAKE_PREFIX_PATH=$QtRoot" `
    '-DCMAKE_C_COMPILER=gcc' `
    '-DCMAKE_CXX_COMPILER=g++' `
    "-DSHIFTECH_BUILD_GUI=$guiFlag" `
    '-DSHIFTECH_BUILD_TESTS=ON'
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }

# --- build ---
& $CMake --build $BuildDir
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

# --- tests ---
if (-not $NoTests) {
    Write-Host "-- running tests"
    & $CTest --test-dir $BuildDir --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "tests failed ($LASTEXITCODE)" }
} else {
    Write-Host "-- skipping tests (-NoTests)"
}

# --- stage the portable folder ---
Write-Host "-- staging $StageDir"
if (Test-Path $StageDir) { Remove-Item -Recurse -Force $StageDir }
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

$exes = @('provisioner.exe')
if (-not $NoGui) { $exes += 'app.exe' }
foreach ($exe in $exes) {
    $src = Join-Path $BuildDir $exe
    Assert-Path $src "built $exe"
    Copy-Item $src -Destination $StageDir
}

# Qt runtime (DLLs + plugins) next to the exe(s)
$windeployqt = Join-Path $QtRoot 'bin\windeployqt.exe'
Assert-Path $windeployqt 'windeployqt.exe'
foreach ($exe in $exes) {
    Write-Host "   windeployqt $exe"
    & $windeployqt --release --no-translations --compiler-runtime `
        (Join-Path $StageDir $exe)
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed for $exe ($LASTEXITCODE)" }
}

# profiles + app manifests + tools + empty runtime dirs
Copy-Item (Join-Path $RepoRoot 'profiles') -Destination $StageDir -Recurse
if (Test-Path (Join-Path $RepoRoot 'apps')) {
    Copy-Item (Join-Path $RepoRoot 'apps') -Destination $StageDir -Recurse
    # installers/archives already in apps\<id>\ are copied too; add more on the USB drive
}
# 7za.exe for extracting .7z portable apps (see tools\README.md)
$sevenZa = Join-Path $RepoRoot 'tools\7za.exe'
if (Test-Path $sevenZa) {
    Copy-Item $sevenZa -Destination $StageDir
    Write-Host "   bundled 7za.exe"
} else {
    Write-Host "   NOTE: tools\7za.exe not found - .7z portable apps will need 7-Zip on the target"
}
New-Item -ItemType Directory -Force -Path (Join-Path $StageDir 'cache')  | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $StageDir 'logs')   | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $StageDir 'cache\.keep') | Out-Null
New-Item -ItemType File -Force -Path (Join-Path $StageDir 'logs\.keep')  | Out-Null

$version = (Get-Date -Format 'yyyy.MM.dd')
@"
Shiftech Win Provisioner - portable build $version

This folder is self-contained. Copy it anywhere (including a USB drive) and run:

  provisioner.exe scan
  provisioner.exe drivers resolve --download        (pre-populate cache\ on a connected PC)
  provisioner.exe provision --profile standard --dry-run
  provisioner.exe provision --profile standard      (needs Administrator; drops --dry-run)

GUI (Windows 10/11 only - see ADR-0003):

  app.exe

Local app installers: put the .exe/.msi next to its app.json in apps\<id>\
(e.g. apps\winrar\winrar-x64-701.exe). Profiles reference them by folder id.

Config tweaks available:  provisioner.exe config list

Everything is relative to this folder: cache\, logs\, profiles\, apps\.
Driver install, config tweaks and a real provisioning run require running elevated
(Run as administrator).
"@ | Set-Content -Encoding UTF8 (Join-Path $StageDir 'README-PORTABLE.txt')

Write-Host ""
Write-Host "== staged ==" -ForegroundColor Green
Get-ChildItem $StageDir | Select-Object Mode, Length, Name | Format-Table -AutoSize

if ($Zip) {
    $zipPath = Join-Path $DistRoot "$AppName-$version.zip"
    if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
    Compress-Archive -Path $StageDir -DestinationPath $zipPath
    Write-Host "== zipped ==> $zipPath" -ForegroundColor Green
}

Write-Host ""
Write-Host "Portable folder: $StageDir"
Write-Host "Run it with:     .\scripts\run-app.ps1  (or  .\scripts\run-app.ps1 -Portable)"

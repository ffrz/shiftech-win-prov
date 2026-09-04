<#
.SYNOPSIS
    Run Shiftech Win Provisioner (CLI or GUI) from the build tree or a portable folder.

.DESCRIPTION
    Wraps provisioner.exe / app.exe with the right PATH so the Qt DLLs resolve.
    By default it runs the freshly built exe under build\; with -Portable it runs the
    staged copy under dist\ShiftechWinProvisioner\ (which is fully self-contained).

    Anything after the recognised switches is passed straight to provisioner.exe.

.PARAMETER Gui
    Launch app.exe instead of the CLI. Ignores CLI args.

.PARAMETER Portable
    Use dist\ShiftechWinProvisioner\ instead of build\.

.PARAMETER Elevated
    Relaunch the CLI elevated (UAC prompt). Needed for a real 'drivers install' /
    'provision' (without --dry-run).

.PARAMETER Build
    Build first (delegates to build-release.ps1 -NoTests) before running.

.EXAMPLE
    .\scripts\run-app.ps1 scan
.EXAMPLE
    .\scripts\run-app.ps1 provision --profile standard --dry-run
.EXAMPLE
    .\scripts\run-app.ps1 -Gui
.EXAMPLE
    .\scripts\run-app.ps1 -Portable -Elevated provision --profile standard
.EXAMPLE
    .\scripts\run-app.ps1 -Build drivers resolve
#>
[CmdletBinding()]
param(
    [switch]$Gui,
    [switch]$Portable,
    [switch]$Elevated,
    [switch]$Build,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$QtRoot   = 'D:\bin\Qt\6.6.2\mingw_64'
$MinGW    = 'D:\bin\Qt\Tools\mingw1120_64\bin'
$RepoRoot = Split-Path -Parent $PSScriptRoot

if ($Build) {
    Write-Host "-- building first" -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'build-release.ps1') -NoTests
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

$appDir = if ($Portable) {
    Join-Path $RepoRoot 'dist\ShiftechWinProvisioner'
} else {
    Join-Path $RepoRoot 'build'
}
if (-not (Test-Path $appDir)) {
    throw "app dir not found: $appDir  (run with -Build, or .\scripts\build-release.ps1 first)"
}

# Portable folder ships its own Qt DLLs next to the exe; build tree needs Qt on PATH.
if (-not $Portable) {
    $env:PATH = "$QtRoot\bin;$MinGW;$env:PATH"
}

$exeName = if ($Gui) { 'app.exe' } else { 'provisioner.exe' }
$exe = Join-Path $appDir $exeName
if (-not (Test-Path $exe)) {
    throw "$exeName not found in $appDir" + $(if ($Gui) { "  (built with -DSHIFTECH_BUILD_GUI=ON?)" } else { "" })
}

if ($Gui) {
    Write-Host "-- launching $exe" -ForegroundColor Cyan
    if ($Elevated) {
        Start-Process -Verb RunAs -FilePath $exe
    } else {
        Start-Process -FilePath $exe
    }
    return
}

$argList = @($AppArgs)
Write-Host "-- $exeName $($argList -join ' ')" -ForegroundColor Cyan

if ($Elevated) {
    # A new elevated console; keep it open so output is visible.
    $joined = ($argList | ForEach-Object { '"' + $_ + '"' }) -join ' '
    Start-Process -Verb RunAs -FilePath 'powershell.exe' -ArgumentList @(
        '-NoExit', '-Command',
        "& `"$exe`" $joined"
    )
} else {
    & $exe @argList
    exit $LASTEXITCODE
}

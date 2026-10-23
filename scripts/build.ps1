[CmdletBinding()]
param(
    [string]$Preset = "windows-msvc",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",

    [string]$Target,

    [switch]$Clean,

    [switch]$SkipConfigure,

    [switch]$ConfigureOnly,

    [switch]$Parallel
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $RepoRoot

$CMake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $CMake) {
    throw "cmake was not found on PATH. Open a Developer PowerShell for Visual Studio or install CMake."
}

$BuildDir = Join-Path $RepoRoot "out/build/$Preset"
$InstallDir = Join-Path $RepoRoot "out/install/$Preset"

if ($Clean) {
    Write-Host "==> Cleaning $BuildDir"
    if (Test-Path -LiteralPath $BuildDir) {
        Remove-Item -LiteralPath $BuildDir -Recurse -Force
    }

    if (Test-Path -LiteralPath $InstallDir) {
        Remove-Item -LiteralPath $InstallDir -Recurse -Force
    }
}

if (-not $SkipConfigure) {
    Write-Host "==> Configuring preset: $Preset"
    & $CMake.Source --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($ConfigureOnly) {
    Write-Host "==> Configure complete: $Preset"
    exit 0
}

$BuildArgs = @("--build", "--preset", $Preset, "--config", $Configuration)

if ($Target) {
    $BuildArgs += @("--target", $Target)
}

if ($Parallel) {
    $BuildArgs += "--parallel"
}

Write-Host "==> Building preset: $Preset ($Configuration)"
& $CMake.Source @BuildArgs
exit $LASTEXITCODE

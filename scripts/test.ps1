[CmdletBinding()]
param(
    [string]$Preset = "windows-msvc",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",

    [string]$Filter,

    [switch]$Build,

    [switch]$ListOnly
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $RepoRoot

if ($Build) {
    $BuildScript = Join-Path $PSScriptRoot "build.ps1"
    & $BuildScript -Preset $Preset -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$CTest = Get-Command ctest -ErrorAction SilentlyContinue
if (-not $CTest) {
    throw "ctest was not found on PATH. Open a Developer PowerShell for Visual Studio or install CMake."
}

$TestArgs = @("--preset", $Preset, "--output-on-failure", "-C", $Configuration)

if ($Filter) {
    $TestArgs += @("-R", $Filter)
}

if ($ListOnly) {
    $TestArgs += "-N"
}

Write-Host "==> Testing preset: $Preset ($Configuration)"
& $CTest.Source @TestArgs
exit $LASTEXITCODE

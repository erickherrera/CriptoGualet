# CMake Configuration Helper Script
# This script helps configure CMake with the correct environment

$ErrorActionPreference = "Stop"

# Find CMake
$cmakePath = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmakePath)) {
    Write-Host "CMake not found at expected location. Please install CMake or update the path in this script."
    exit 1
}

# Add clang-cl to PATH if needed
$clangPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin"
if (Test-Path $clangPath) {
    $env:PATH = "$clangPath;$env:PATH"
    Write-Host "Added clang-cl to PATH: $clangPath"
}

# Add MSVC tools to PATH
$msvcPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
if (Test-Path $msvcPath) {
    $versions = Get-ChildItem $msvcPath | Sort-Object Name -Descending | Select-Object -First 1
    if ($versions) {
        $msvcBinPath = Join-Path $versions.FullName "bin\Hostx64\x64"
        if (Test-Path $msvcBinPath) {
            $env:PATH = "$msvcBinPath;$env:PATH"
            Write-Host "Added MSVC to PATH: $msvcBinPath"
        }
    }
}

Write-Host "`nAvailable CMake presets:"
& $cmakePath --list-presets

Write-Host "`nSelect a preset to configure:"
Write-Host "1. win-ninja-x64-debug (Best for IntelliSense/clangd)"
Write-Host "2. win-vs2022-x64-debug (Visual Studio IDE)"
Write-Host "3. win-ninja-x64-release"
Write-Host "4. win-vs2022-x64-release"
Write-Host ""
$choice = Read-Host "Enter choice (1-4, default: 1)"

$preset = switch ($choice) {
    "2" { "win-vs2022-x64-debug" }
    "3" { "win-ninja-x64-release" }
    "4" { "win-vs2022-x64-release" }
    default { "win-ninja-x64-debug" }
}

Write-Host "`nConfiguring CMake with preset: $preset"
Write-Host "This may take a while, especially if vcpkg needs to build dependencies..."
Write-Host ""

# Try to configure
try {
    & $cmakePath --preset $preset
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n✓ CMake configuration successful!"
        
        # Copy compile_commands.json to root for clangd
        $buildDir = "out\build\$preset"
        $compileCommands = Join-Path $buildDir "compile_commands.json"
        if (Test-Path $compileCommands) {
            Copy-Item $compileCommands -Destination "compile_commands.json" -Force
            Write-Host "✓ Copied compile_commands.json to root for clangd"
        }
        
        Write-Host "`nTo build the project, run:"
        Write-Host "  cmake --build --preset <build-preset-name>"
        Write-Host "`nOr use:"
        Write-Host "  cmake --build out\build\$preset"
    } else {
        Write-Host "`n✗ CMake configuration failed. Check the error messages above."
        Write-Host "`nNote: If vcpkg failed on libiconv, this is a known issue."
        Write-Host "You may need to update vcpkg or work around the dependency issue."
    }
} catch {
    Write-Host "`n✗ Error during CMake configuration: $_"
    Write-Host "`nTroubleshooting tips:"
    Write-Host "1. Ensure Visual Studio 2022 is installed with C++ workload"
    Write-Host "2. Check that vcpkg is properly configured"
    Write-Host "3. Try updating vcpkg: vcpkg update"
}



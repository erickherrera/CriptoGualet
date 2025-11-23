# Quick script to regenerate compile_commands.json for clangd
# This configures CMake with Ninja generator (best for clangd support)

$ErrorActionPreference = "Stop"

Write-Host "=== Regenerating compile_commands.json for clangd ===" -ForegroundColor Cyan

# Find tools
$cmakePath = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninjaPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
$clangPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin"

# Find MSVC path (get latest version)
$msvcBase = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
if (Test-Path $msvcBase) {
    $msvcVersion = Get-ChildItem $msvcBase | Sort-Object Name -Descending | Select-Object -First 1
    $msvcPath = Join-Path $msvcVersion.FullName "bin\Hostx64\x64"
} else {
    Write-Host "Warning: MSVC path not found, continuing anyway..." -ForegroundColor Yellow
    $msvcPath = ""
}

# Set up environment
$env:PATH = "$ninjaPath;$clangPath;$msvcPath;$env:PATH"

Write-Host "`nConfiguring CMake with Ninja generator..." -ForegroundColor Green

# Configure CMake
$buildDir = "out/build/clangd-ninja"
& $cmakePath -S . -B $buildDir `
    -G "Ninja" `
    -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_C_COMPILER="clang-cl" `
    -DCMAKE_CXX_COMPILER="clang-cl" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
    -DCMAKE_CXX_STANDARD=17 `
    -DCMAKE_CXX_STANDARD_REQUIRED=ON `
    -DCMAKE_PREFIX_PATH="$PWD/vcpkg_installed/x64-windows" `
    -DVCPKG_MANIFEST_MODE=OFF

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✓ CMake configuration successful!" -ForegroundColor Green
    
    # Copy compile_commands.json to root
    $compileCommands = Join-Path $buildDir "compile_commands.json"
    if (Test-Path $compileCommands) {
        Copy-Item $compileCommands -Destination "compile_commands.json" -Force
        $entryCount = (Get-Content "compile_commands.json" | ConvertFrom-Json).Count
        Write-Host "✓ Copied compile_commands.json to root ($entryCount entries)" -ForegroundColor Green
        Write-Host "`nNext steps:" -ForegroundColor Cyan
        Write-Host "1. Restart clangd: Ctrl+Shift+P -> 'clangd: Restart language server'"
        Write-Host "2. Or reload the window: Ctrl+Shift+P -> 'Developer: Reload Window'"
    } else {
        Write-Host "✗ compile_commands.json not found in build directory" -ForegroundColor Red
    }
} else {
    Write-Host "`n✗ CMake configuration failed. Check the error messages above." -ForegroundColor Red
    exit 1
}


# PowerShell script to install vcpkg dependencies
# This script uses the full path to vcpkg.exe so it works even if vcpkg is not in PATH

$vcpkgPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg\vcpkg.exe"

if (-not (Test-Path $vcpkgPath)) {
    Write-Host "Error: vcpkg.exe not found at $vcpkgPath" -ForegroundColor Red
    Write-Host "Please verify your Visual Studio 2022 installation." -ForegroundColor Yellow
    exit 1
}

Write-Host "Installing vcpkg dependencies from vcpkg.json..." -ForegroundColor Green
Write-Host "This may take several minutes..." -ForegroundColor Yellow
Write-Host ""

# In manifest mode, vcpkg reads packages from vcpkg.json
# Just run 'vcpkg install' without package arguments
& $vcpkgPath install --triplet x64-windows

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nDependencies installed successfully!" -ForegroundColor Green
} else {
    Write-Host "`nError: Failed to install dependencies" -ForegroundColor Red
    exit $LASTEXITCODE
}


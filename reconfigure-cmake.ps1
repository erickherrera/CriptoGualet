# Reconfigure CMake with VS2022 Debug preset
# Run this script from PowerShell on Windows

Write-Host "Cleaning old CMake cache..." -ForegroundColor Yellow
Remove-Item -Path "out/build/win-vs2022-x64-debug" -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Configuring CMake with win-vs2022-x64-debug preset..." -ForegroundColor Cyan
cmake --preset win-vs2022-x64-debug

if ($LASTEXITCODE -eq 0) {
    Write-Host "CMake configuration successful!" -ForegroundColor Green
    Write-Host "You can now build the project in Visual Studio or VSCode" -ForegroundColor Green
} else {
    Write-Host "CMake configuration failed. Check the errors above." -ForegroundColor Red
    exit $LASTEXITCODE
}

# CriptoGualet Release Guide

This guide details the steps to build, package, and release CriptoGualet for Windows.

## Prerequisites

Ensure you have the development environment set up as per `SETUP.md`:
*   Visual Studio 2022 with C++ Desktop Development workload.
*   Clang-CL (installed via VS Installer).
*   CMake (3.20+).
*   Qt6 (Installed and `QT_DIR` configured or auto-detected).
*   vcpkg (Environment variable `VCPKG_ROOT` set).

## 1. Clean Build Environment

For a production release, it is CRITICAL to start with a clean build directory. Mismatches in the MSVC runtime library (e.g., `_ITERATOR_DEBUG_LEVEL` errors) are often caused by stale CMake cache from previous Debug builds.

```powershell
# Delete the existing build directory for the Clang preset
Remove-Item -Recurse -Force "out/build/windows-clang" -ErrorAction SilentlyContinue
```

## 2. Configure the Project

Configure the ClangCL preset. The Visual Studio generator is multi-config, so Release is selected at build time.

```powershell
cmake --preset windows-clang
```

## 3. Build the Project

Build the project using the Release configuration.

```powershell
cmake --build --preset windows-clang --config Release
```

## 4. Packaging (Create Installer & Zip)

The project is configured with CPack to generate:
1.  **NSIS Installer** (`.exe`): A standard Windows installer that adds start menu shortcuts.
2.  **ZIP Archive** (`.zip`): A portable version of the application.

Run CPack from the build directory:

```powershell
cd out/build/windows-clang
cpack -C Release
```

**What happens during packaging:**
*   Compiles the application (if not already done).
*   Installs the executable and assets to a staging area.
*   Copies required DLLs (OpenSSL, SQLCipher, etc.) from `vcpkg`.
*   **Runs `windeployqt`**: Automatically deploys all necessary Qt6 DLLs and plugins.
*   Generates the `.exe` installer and `.zip` file.

## 5. Locate Release Artifacts

After `cpack` completes, your release files will be located in:

`out/build/windows-clang/`

Look for files named like:
*   `CriptoGualet-1.0.0-win64.exe` (Installer)
*   `CriptoGualet-1.0.0-win64.zip` (Portable)

## Troubleshooting

### "Qt6 not found"
Ensure your `CMakePresets.json` points to the correct Qt version in `QT_DIR`.
Default is: `C:/Qt/6.9.1/msvc2022_64`

### "windeployqt failed"
Ensure `bin/windeployqt.exe` is in your Qt installation's `bin` folder and accessible. The build script attempts to find it automatically.

### "_ITERATOR_DEBUG_LEVEL mismatch"
If you see an error like `mismatch detected for '_ITERATOR_DEBUG_LEVEL'`, it means a part of the project is trying to use the Debug runtime in a Release build.
*   **Fix:** Ensure you followed Step 1 (Clean Build). The `CMakeLists.txt` has been updated to use generator expressions for the runtime library, but CMake needs a fresh configuration to apply these changes correctly.
*   Check that your `vcpkg` dependencies were installed for the correct triplet (`x64-windows`).

### "Icon missing"
Ensure `resources/icons/wallet.ico` exists. This is required for the NSIS installer.

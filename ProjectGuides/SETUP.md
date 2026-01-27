# CriptoGualet - Windows Setup Guide

This guide will help you set up the project on any Windows machine (Desktop, Laptop, etc.)

## Prerequisites

### Required Software

1. **Visual Studio 2022** (Community, Professional, or Enterprise)
   - Download: https://visualstudio.microsoft.com/downloads/
   - During installation, select:
     - ✓ Desktop development with C++
     - ✓ Clang compiler for Windows (optional but recommended)
     - ✓ C++ CMake tools for Windows

2. **vcpkg** (integrated with Visual Studio 2022)
   - Visual Studio 2022 includes vcpkg integration
   - Default location: `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg`
   - The project is configured to use this automatically

3. **Qt 6** (version 6.9.1 or later recommended)
   - Download: https://www.qt.io/download-qt-installer
   - During installation, select:
     - ✓ Qt 6.9.x → MSVC 2022 64-bit
     - ✓ Qt 6.9.x → Sources (optional)
   - Common install locations (automatically detected):
     - `C:\Program Files\Qt`
     - `C:\Qt`
     - `C:\Program Files (x86)\Qt`

## First-Time Setup

### 1. Clone the Repository

```bash
git clone <your-repo-url>
cd CriptoGualet
```

### 2. Verify vcpkg Integration

The project uses vcpkg to manage dependencies. Visual Studio 2022 includes vcpkg, but you can verify:

```powershell
# Check if VCPKG_ROOT is set (optional - CMake presets set this)
echo $env:VCPKG_ROOT

# If not set, it should be at:
# C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg
```

### 3. Configure the Project

Use the Visual Studio generator:

#### Visual Studio Generator (Recommended - Most Stable)

```bash
# Configure for Debug
cmake --preset=win-vs2022-x64-debug

# Or configure for Release
cmake --preset=win-vs2022-x64-release
```

### 4. Build the Project

#### If using Visual Studio generator:

```bash
# Build Debug
cmake --build out/build/win-vs2022-x64-debug --config Debug

# Build Release
cmake --build out/build/win-vs2022-x64-release --config Release
```

### 5. Run the Application

```bash
# Qt GUI Application (recommended)
./out/build/win-vs2022-x64-debug/bin/Debug/CriptoGualetQt.exe

# Or Win32 GUI Application (if BUILD_GUI_WIN32 is enabled)
./out/build/win-vs2022-x64-debug/bin/Debug/CriptoGualet.exe
```

## Building in Visual Studio IDE

1. Open Visual Studio 2022
2. **File → Open → CMake** → Select `CMakeLists.txt`
3. Visual Studio will automatically configure using the presets
4. Select your build configuration from the toolbar
5. **Build → Build All** (Ctrl+Shift+B)
6. **Debug → Start Debugging** (F5) or **Start Without Debugging** (Ctrl+F5)

## Troubleshooting

### Qt Not Found

If CMake cannot find Qt:

1. Verify Qt is installed in one of these locations:
   - `C:\Program Files\Qt\6.9.x`
   - `C:\Qt\6.9.x`
   - `C:\Program Files (x86)\Qt\6.9.x`

2. Or manually set Qt path:
   ```bash
   cmake --preset=win-vs2022-x64-debug -DCMAKE_PREFIX_PATH="C:/Path/To/Qt/6.9.1/msvc2022_64"
   ```

### vcpkg Dependencies Not Installing

If vcpkg fails to install dependencies:

1. Ensure you have internet connection
2. Check vcpkg location exists:
   ```powershell
   Test-Path "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg"
   ```
3. Manually install dependencies:
   ```bash
   vcpkg install cpr libqrencode nlohmann-json secp256k1 sqlcipher sqlite3 --triplet=x64-windows
   ```

### Compiler Not Found

If CMake cannot find the compiler:

1. Ensure Visual Studio 2022 is installed with C++ development tools
2. The presets automatically use ClangCL or MSVC
3. Visual Studio generator will use the installed toolset automatically

## Development Workflow

### Running Tests

```bash
# Run all tests
cmake --build out/build/win-vs2022-x64-debug --config Debug --target RUN_TESTS

# Or run individual test executables
./out/build/win-vs2022-x64-debug/bin/Debug/test_database.exe
./out/build/win-vs2022-x64-debug/bin/Debug/test_auth_database_integration.exe
```

### Clean Build

```bash
# Remove build directory
Remove-Item -Recurse -Force out/build/win-vs2022-x64-debug

# Reconfigure
cmake --preset=win-vs2022-x64-debug

# Rebuild
cmake --build out/build/win-vs2022-x64-debug --config Debug
```

### Available Build Presets

- `win-vs2022-x64-debug` - Visual Studio 2022, x64, Debug (recommended for IDE)
- `win-vs2022-x64-release` - Visual Studio 2022, x64, Release

## Project Structure

```
CriptoGualet/
├── backend/           # Backend components
│   ├── core/         # Core business logic (Auth, Crypto, WalletAPI)
│   ├── blockchain/   # Blockchain integration (BlockCypher)
│   ├── database/     # Database layer (SQLCipher)
│   ├── repository/   # Data access layer
│   └── utils/        # Utilities (QRGenerator, SharedSymbols)
├── frontend/         # Frontend components
│   ├── qt/          # Qt6 GUI (primary interface)
│   └── win32/       # Win32 GUI (legacy)
├── Tests/           # Unit and integration tests
├── out/             # Build output (git-ignored)
└── CMakeLists.txt   # Root CMake configuration
```

## Dependencies

The project automatically installs these via vcpkg:

- **Qt6** - Modern GUI framework
- **libqrencode** - QR code generation
- **SQLCipher** - Encrypted database
- **nlohmann-json** - JSON parsing
- **CPR** - HTTP client library
- **secp256k1** - Elliptic curve cryptography
- **OpenSSL** - Cryptographic functions

## Notes for Laptop/Desktop Sync

### What Works Automatically:
- ✓ vcpkg dependency management
- ✓ Qt auto-detection
- ✓ Compiler detection
- ✓ CMake presets configuration

### What You Need to Install:
1. Visual Studio 2022 with C++ development
2. Qt 6.9.x (MSVC 2022 64-bit)

### Files to Ignore (Already in .gitignore):
- `out/` - Build output directory
- `vcpkg_installed/` - Installed dependencies
- `.vs/` - Visual Studio cache
- `CMakeSettings.json` - Local VS CMake settings (if it exists)

The project is now fully portable! Just install the prerequisites and run cmake configure.

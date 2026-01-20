# Visual Studio Configuration Guide

## What You're Seeing

When Visual Studio shows only "x64 Debug" and "x86 Debug", it means it's using the default Visual Studio configuration instead of the custom CMake configurations I created.

## How to Fix This

### Method 1: Use CMake Presets (Recommended)

1. **Open Visual Studio**
2. **Click the dropdown** next to the green play button (►)
3. **Select "Manage Configurations..."**
4. **Click "Add Configuration"**
5. **Select "CMake Preset"**
6. **Choose from available presets:**
   - `win-vs2022-x64-debug` - MSVC Debug (Faster builds)
   - `win-vs2022-x64-release` - MSVC Release
   - `win-vs2022-clangcl-debug` - ClangCL Debug
   - `win-vs2022-clangcl-debug-asan` - ClangCL + AddressSanitizer
   - `win-vs2022-clangcl-release-lto` - ClangCL + LTO (Max performance)

### Method 2: Use CMakeSettings.json (Alternative)

1. **Close Visual Studio**
2. **Delete the CMakeCache.txt file** from your project root
3. **Reopen Visual Studio** - it should now show the configurations from CMakeSettings.json:
   - `MSVC Debug (Faster Builds)`
   - `MSVC Release`
   - `ClangCL Debug + AddressSanitizer (Security Testing)`
   - `ClangCL Release + LTO (Maximum Performance)`

### Method 3: Command Line (Most Reliable)

```bash
# Configure each preset first
cmake --preset win-vs2022-x64-debug
cmake --preset win-vs2022-clangcl-debug-asan
cmake --preset win-vs2022-clangcl-release-lto

# Then open Visual Studio - it will detect the configured projects
```

## Available Configurations

| Configuration | Compiler | Purpose | When to Use |
|--------------|------------|----------|--------------|
| **MSVC Debug** | MSVC | Faster development builds | Daily coding, quick iteration |
| **MSVC Release** | MSVC | Standard release | Basic release builds |
| **ClangCL Debug** | ClangCL | Debug with Clang | Testing Clang compatibility |
| **ClangCL Debug + ASan** | ClangCL | Security testing | Memory safety, vulnerability testing |
| **ClangCL Release + LTO** | ClangCL | Optimized release | Maximum performance |

## Troubleshooting

### If you still see only x64/x86 Debug:

1. **Check that CMakePresets.json exists** in project root
2. **Verify the file is valid JSON**
3. **Clear CMake cache**: Delete `CMakeCache.txt` and `out/build/` folder
4. **Restart Visual Studio**
5. **Try Method 1** (Manage Configurations) explicitly

### To verify configurations are working:

1. **Select a configuration** from the dropdown
2. **Check the Output window** - it should show which compiler/toolset is being used
3. **Look for messages like**:
   - "ClangCL mode detected" for Clang configurations
   - "AddressSanitizer enabled" for ASan builds
   - "Link-Time Optimization enabled" for LTO builds

## Quick Start Commands

```bash
# Setup all configurations at once
cmake --preset win-vs2022-x64-debug
cmake --preset win-vs2022-clangcl-debug-asan  
cmake --preset win-vs2022-clangcl-release-lto

# Then open Visual Studio and select from dropdown
```

## Key Differences

- **MSVC Debug**: Fastest compile times, good IntelliSense
- **ClangCL + ASan**: Detects memory errors, 2-3x slower runtime
- **ClangCL + LTO**: Best runtime performance, slower compile time

Choose based on what you're doing: development (MSVC), security testing (ClangCL + ASan), or performance optimization (ClangCL + LTO).
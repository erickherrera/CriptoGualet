# Troubleshooting Guide

## DLL Missing Errors

If you get "The program can't start because [library].dll is missing" errors:

### Quick Fix
Run the batch script:
```
run_app.bat
```

### Manual Fix
1. **Copy vcpkg DLLs:**
   ```cmd
   copy vcpkg_installed\x64-windows\bin\*.dll build\bin\Release\
   ```

2. **Required DLLs for QR functionality:**
   - `qrencode.dll` - Main QR generation library
   - `libpng16.dll` - PNG image support (dependency)
   - `zlib1.dll` - Compression library (dependency)
   - `iconv-2.dll` - Character encoding (dependency)

### Verification
Check that these files exist in `build\bin\Release\`:
```cmd
dir build\bin\Release\*.dll | findstr "qrencode\|png\|zlib\|iconv"
```

## Qt Application Won't Start

### Missing Qt DLLs
The build process should automatically deploy Qt DLLs via `windeployqt`. If missing:

1. **Rebuild with deployment:**
   ```cmd
   cmake --build build --config Release --target CriptoGualetQt
   ```

2. **Manual Qt deployment:**
   ```cmd
   "C:\Program Files\Qt\6.9.1\msvc2022_64\bin\windeployqt.exe" build\bin\Release\CriptoGualetQt.exe
   ```

## QR Code Functionality Issues

### "QR code not available" message
This means libqrencode wasn't detected during build:

1. **Verify libqrencode installation:**
   ```cmd
   vcpkg list | findstr qrencode
   ```

2. **Rebuild with proper detection:**
   ```cmd
   rmdir /s build
   mkdir build
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

3. **Look for this message during CMake configure:**
   ```
   -- libqrencode found - QR code functionality enabled
   -- QRGenerator will be built with libqrencode support
   ```

### Real QR codes vs Fallback patterns
- **Real QR codes**: Scannable with mobile devices, contain actual seed phrase
- **Fallback patterns**: Simple X pattern, indicates libqrencode not available

## Build Configuration Issues

### vcpkg not found
Ensure vcpkg is properly integrated:
```cmd
vcpkg integrate install
```

### Wrong architecture
Ensure you're building for the same architecture as your vcpkg installation:
```cmd
vcpkg install libqrencode:x64-windows
```

## Runtime Environment

### Visual C++ Redistributables
If you get MSVCR/VCRUNTIME errors, install:
- Visual C++ Redistributable for Visual Studio 2022

### Windows Version Compatibility
- Minimum: Windows 10
- Recommended: Windows 10/11 with latest updates

## Testing QR Functionality

1. **Run the application:**
   ```cmd
   build\bin\Release\CriptoGualetQt.exe
   ```

2. **Create a test account:**
   - Use any username/password (minimum 6 characters)
   - Password must have at least one letter and one number

3. **Verify QR generation:**
   - You should see a seed phrase dialog after registration
   - Click "Show QR Code"
   - You should see a real QR code (not an X pattern)
   - The QR code should be scannable with any mobile QR scanner

4. **What to expect:**
   - The QR code contains all 12 seed words separated by spaces
   - Scanning with a mobile device should show the seed phrase text
   - No plain text files should be created on disk

## Success Indicators

✅ **Application starts without DLL errors**
✅ **Registration creates accounts successfully**
✅ **Seed phrase dialog shows formatted words**
✅ **QR code button generates scannable codes**
✅ **No plain text seed files created**
✅ **User confirmation required before proceeding**

If all indicators pass, your secure seed phrase system is working correctly!
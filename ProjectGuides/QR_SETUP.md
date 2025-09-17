# QR Code Library Setup Guide

This project includes QR code functionality for displaying seed phrases. While the application works without libqrencode (using a fallback pattern), installing it provides real QR code generation.

## Option 1: vcpkg (Recommended)

The project includes a `vcpkg.json` manifest that automatically installs libqrencode:

```bash
# Install dependencies from manifest
vcpkg install

# Or specify the triplet explicitly
vcpkg install --triplet x64-windows
```

The vcpkg.json includes:
```json
{
  "name": "criptogualet",
  "version": "1.0.0",
  "builtin-baseline": "4bb07a326d9b9bce3703272a509e5bc25dd9cfd5",
  "dependencies": [
    "libqrencode"
  ]
}
```

## Option 2: Manual Installation

### Windows (vcpkg)
```bash
vcpkg install libqrencode:x64-windows
```

### Ubuntu/Debian
```bash
sudo apt-get install libqrencode-dev
```

### macOS (Homebrew)
```bash
brew install qrencode
```

### Windows (Manual Build)
1. Download libqrencode source from: https://github.com/fukuchi/libqrencode
2. Build with CMake:
   ```bash
   git clone https://github.com/fukuchi/libqrencode.git
   cd libqrencode
   mkdir build && cd build
   cmake .. -DCMAKE_INSTALL_PREFIX=C:/libqrencode
   cmake --build . --config Release
   cmake --install .
   ```

## Option 3: Bundled Libraries

For distribution, you can bundle pre-compiled libraries:

1. Create `libs/libqrencode/` directory
2. Place headers in `libs/libqrencode/include/`
3. Place libraries in `libs/libqrencode/lib/`

The CMake configuration will automatically detect bundled libraries.

## Option 4: System Package Managers

### Chocolatey (Windows)
```powershell
# Note: libqrencode may not be available via Chocolatey
# Use vcpkg instead
```

### Conan (Cross-platform)
Add to `conanfile.txt`:
```
[requires]
libqrencode/4.1.1

[generators]
cmake_find_package
```

## Verification

To verify libqrencode is available, build the project and check the output:

```bash
cmake --build build --config Release
```

Look for these messages:
- ✅ `libqrencode found - QR code functionality enabled`
- ⚠️ `libqrencode not found - QR code functionality will use fallback`

## Runtime Behavior

### With libqrencode
- Real QR codes are generated containing the seed phrase
- Users can scan with mobile devices
- QR codes display properly scaled

### Without libqrencode (Fallback)
- Placeholder pattern is displayed
- Clear message informs user that QR is not available
- Users are guided to manually copy seed phrase
- All other functionality remains intact

## Build Integration

The project automatically detects libqrencode using multiple methods:

1. **pkg-config** (Linux/macOS)
2. **Custom CMake finder** (bundled libraries)
3. **Manual detection** (fallback)

The QRGenerator library is built with or without libqrencode support, ensuring the application always compiles successfully.

## For Developers

To add QR functionality to other parts of the application:

```cpp
#include "include/QRGenerator.h"

QR::QRData qrData;
bool success = QR::GenerateQRCode("your text here", qrData);

if (qrData.width > 0) {
    // Create QImage from qrData for display
    QImage qrImage(qrData.width, qrData.height, QImage::Format_RGB888);
    // ... convert qrData.data to QImage pixels
}
```

The function returns `true` for real QR codes, `false` for fallback patterns, but always provides displayable data when `qrData.width > 0`.
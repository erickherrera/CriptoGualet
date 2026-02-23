# CriptoGualet

CriptoGualet is a **non-custodial, cross-platform cryptocurrency wallet** designed for high-security digital asset management. Built with **C++20** and the **Qt6 Framework**, it combines a professional-grade backend with a modern, theme-aware user interface.

---

## 🚀 Key Features

*   **Multi-Chain Support**: Native support for **Bitcoin (BTC)**, **Ethereum (ETH)**, and **Litecoin (LTC)** with BIP-44 derivation.
*   **Encrypted Storage**: All sensitive data is stored in a locally encrypted **SQLCipher/SQLite3** database using machine-specific hardware identifiers.
*   **BIP-39/BIP-44 Standards**: Industry-standard mnemonic seed phrases and hierarchical deterministic (HD) wallet generation.
*   **Modern UI/UX**:
    *   **Theming**: Multiple built-in themes (Dark Blue, Light Blue, Purple Crypto) with smooth transitions and a loading overlay.
    *   **Responsive Design**: Optimized for various screen sizes, including ultra-wide monitors.
*   **System Diagnostics**: Built-in security verification and cryptographic health checks.
*   **Secure Networking**: Multi-provider support (BlockCypher, Bitcoin RPC) with optional fallback mechanisms.

---

## 🛠 Prerequisites

To build CriptoGualet, ensure you have the following installed:

*   **C++ Compiler**: Clang (v16+) or MSVC (VS 2022 17.10+) supporting **C++20**.
*   **CMake**: Version 3.21 or later.
*   **Qt6**: Version 6.5+ (specifically `Qt6::Widgets`, `Qt6::Gui`, `Qt6::Network`, `Qt6::Concurrent`).
*   **vcpkg**: Microsoft's C++ package manager (used for all backend dependencies).
*   **NSIS**: (Optional) For building the Windows Installer.

---

## 🏗 Building the Project

The project uses **CMake Presets** for a streamlined configuration process.

### 1. Clone the Repository
```bash
git clone https://github.com/erickherrera/CriptoGualet.git
cd CriptoGualet
```

### 2. Install Dependencies
Ensure `VCPKG_ROOT` is set in your environment, then the build system will automatically handle dependencies defined in `vcpkg.json`.

### 3. Configure and Build
We recommend using the **ClangCL** presets on Windows for optimal performance and safety:

**Debug Build (with Address Sanitizer):**
```powershell
cmake --preset w-cl-dbg
cmake --build --preset w-cl-build
```

**Release Build (Optimized):**
```powershell
cmake --preset w-cl-rel
cmake --build --preset w-cl-build-release
```

---

## 📦 Distribution & Packaging

To generate a professional Windows Installer (`.exe`) or a portable archive (`.zip`), run CPack from the release build directory:

```powershell
cd out/build/w-cl-rel
cpack -G NSIS -C Release
```

---

## 🔒 Security Architecture

*   **Memory Safety**: Critical cryptographic secrets (mnemonic, private keys) are securely wiped from memory immediately after use.
*   **Encrypted DB**: The database key is never stored; it is derived at runtime using a combination of user password and hardware-bound secrets.
*   **Two-Factor Authentication**: Support for TOTP (Google Authenticator, Authy) for sensitive operations.
*   **No Centralized Storage**: Your keys never leave your device. CriptoGualet does not have a "cloud" component for your funds.

---

## 🧪 Testing

CriptoGualet includes an extensive test suite for its cryptographic and repository layers:

```powershell
ctest --preset w-cl-test
```

---

## 📂 Project Structure

*   **`backend/`**: Core logic including cryptography (`secp256k1`), blockchain APIs, and the database repository layer.
*   **`frontend/qt/`**: The Qt6 implementation, containing all UI components, custom widgets, and theme management.
*   **`assets/`**: Static resources like the BIP-39 wordlist required for seed generation.
*   **`resources/`**: Icons, logos, and Qt resource files (`.qrc`).
*   **`Tests/`**: Unit and integration tests for cryptographic correctness and repository integrity.
*   **`ProjectGuides/`**: Detailed documentation on architecture, UI design, and development workflows.

---

## 📖 Development Guides

For more in-depth information, please refer to our internal guides:
*   [System Architecture](ProjectGuides/SYSTEM_ARCHITECTURE.md)
*   [Multi-Chain Design](ProjectGuides/MULTI_CHAIN_ARCHITECTURE.md)
*   [UI & Theming Guide](ProjectGuides/QT_UI_ARCHITECTURE.md)
*   [Release & Packaging](ProjectGuides/RELEASE_GUIDE.md)

---

## 📄 License

Copyright (c) 2026 CriptoGualet Team

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.

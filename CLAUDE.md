# CriptoGualet - Cross-Platform Cryptocurrency Wallet

## Project Overview
CriptoGualet is a **non-custodial cryptocurrency and digital assets wallet** with both Windows native (Win32) and modern Qt GUI interfaces. Built with CMake for cross-platform compatibility, it features secure cryptographic operations, QR code generation, and seed phrase management.

## Architecture

The project follows a **backend/frontend separation** for better organization and maintainability.

### Backend Components

#### Core (`backend/core/`)
- **Auth** - Authentication and wallet management
- **Crypto** - Cryptographic functions using Windows CryptoAPI
- **WalletAPI** - High-level wallet operations wrapper

#### Blockchain (`backend/blockchain/`)
- **BlockCypher** - BlockCypher API integration for Bitcoin network interaction

#### Database (`backend/database/`)
- **DatabaseManager** - Encrypted SQLCipher-based storage for wallet data

#### Repository (`backend/repository/`)
- **UserRepository** - User data access layer
- **WalletRepository** - Wallet data access layer
- **TransactionRepository** - Transaction data access layer
- **Logger** - Application logging

#### Utils (`backend/utils/`)
- **QRGenerator** - QR code generation with libqrencode
- **SharedSymbols** - Shared symbols and utilities

### Frontend Components

#### Qt GUI (`frontend/qt/`) - Primary Interface
Modern cross-platform interface using Qt6:
- `CriptoGualetQt.cpp` - Main Qt application
- `QtLoginUI.cpp` - Qt login interface
- `QtWalletUI.cpp` - Qt wallet interface
- `QtThemeManager.cpp` - Theme management
- `QtSeedDisplayDialog.cpp` - Seed phrase display dialog
- `QtExpandableWalletCard.cpp` - Wallet card UI component

#### Win32 GUI (`frontend/win32/`) - Windows Native
- `CriptoGualet.cpp` - Main Win32 application (legacy)

### Test Suite (`Tests/`)
- `test_basic_auth.cpp` - Basic authentication tests
- `test_bip39.cpp` - BIP39 seed phrase tests
- `test_secure_seed.cpp` - Secure seed generation tests
- `test_database.cpp` - Database functionality tests
- `test_repository.cpp` - Repository layer tests
- `test_security_enhancements.cpp` - Security feature tests
- `test_blockcypher_api.cpp` - Blockchain API tests

## Build System

### CMake Configuration
- **Root**: `CMakeLists.txt` - Main build configuration with compiler detection
- **Source**: `src/CMakeLists.txt` - Build orchestration (includes all subdirectories)
- **Backend Modules**:
  - `backend/core/CMakeLists.txt` - Core libraries (Auth, Crypto, WalletAPI)
  - `backend/blockchain/CMakeLists.txt` - Blockchain integration (BlockCypher)
  - `backend/database/CMakeLists.txt` - Database layer (DatabaseManager)
  - `backend/repository/CMakeLists.txt` - Data access layer (Repositories)
  - `backend/utils/CMakeLists.txt` - Utilities (QRGenerator, SharedSymbols)
- **Frontend Modules**:
  - `frontend/qt/CMakeLists.txt` - Qt GUI application
  - `frontend/win32/CMakeLists.txt` - Win32 GUI application
- **Tests**: `Tests/CMakeLists.txt` - Test executables
- **Presets**: `CMakePresets.json` - Build configuration presets
- **Settings**: `CMakeSettings.json` - Visual Studio CMake settings

### Compiler Support
- **Clang** (Primary) - Cross-platform with LLVM
- **Clang-CL** - MSVC compatibility on Windows
- **MSVC** - Native Windows compiler
- **GCC** - Linux/Unix systems

### Dependencies (vcpkg)
- **Qt6** - Modern GUI framework (Core, Widgets, Gui, Svg)
- **libqrencode** - QR code generation library
- **SQLCipher/SQLite3** - Encrypted database storage
- **CPR** - HTTP requests library (for BlockCypher API)
- **nlohmann-json** - JSON parsing library
- **Windows CryptoAPI** - Native cryptographic functions (bcrypt, crypt32)

## Build Commands

### Configure and Build
```bash
# Configure with default preset
cmake --preset=default

# Build debug version
cmake --build out/build/win-clang-x64-debug

# Build release version
cmake --build out/build/win-clang-x64-release
```

### Test Executables
```bash
# Run authentication tests
./out/bin/debug_test

# Run BIP39 tests
./out/bin/test_bip39

# Run secure seed tests
./out/bin/test_secure_seed
```

### Development Tools
```bash
# Format code with clang-format
clang-format -i backend/**/*.cpp backend/**/*.h frontend/**/*.cpp frontend/**/*.h Tests/**/*.cpp

# Lint code with clang-tidy
clang-tidy backend/**/*.cpp frontend/**/*.cpp Tests/**/*.cpp

# Generate compile commands (automatic)
# Output: compile_commands.json
```

## Project Structure
```
CriptoGualet/
├── CMakeLists.txt              # Root build configuration
├── CMakePresets.json           # Build presets
├── CMakeSettings.json          # VS CMake settings
├── vcpkg.json                  # Dependency management
├── .clang-format              # Code formatting rules
├── .clang-tidy                # Static analysis rules
├── backend/                    # Backend components
│   ├── core/                   # Core business logic
│   │   ├── CMakeLists.txt
│   │   ├── Auth.cpp
│   │   ├── Crypto.cpp
│   │   ├── WalletAPI.cpp
│   │   └── include/
│   │       ├── Auth.h
│   │       ├── Crypto.h
│   │       └── WalletAPI.h
│   ├── blockchain/             # Blockchain integration
│   │   ├── CMakeLists.txt
│   │   ├── BlockCypher.cpp
│   │   └── include/
│   │       └── BlockCypher.h
│   ├── database/               # Database layer
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── DatabaseManager.cpp
│   │   │   └── schema_migrations.sql
│   │   └── include/
│   │       └── Database/
│   │           └── DatabaseManager.h
│   ├── repository/             # Data access layer
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── UserRepository.cpp
│   │   │   ├── WalletRepository.cpp
│   │   │   ├── TransactionRepository.cpp
│   │   │   └── Logger.cpp
│   │   └── include/
│   │       └── Repository/
│   │           ├── UserRepository.h
│   │           ├── WalletRepository.h
│   │           ├── TransactionRepository.h
│   │           ├── Logger.h
│   │           └── RepositoryTypes.h
│   └── utils/                  # Utilities
│       ├── CMakeLists.txt
│       ├── QRGenerator.cpp
│       ├── SharedSymbols.cpp
│       └── include/
│           └── QRGenerator.h
├── frontend/                   # Frontend components
│   ├── qt/                     # Qt GUI (primary)
│   │   ├── CMakeLists.txt
│   │   ├── CriptoGualetQt.cpp
│   │   ├── QtLoginUI.cpp
│   │   ├── QtWalletUI.cpp
│   │   ├── QtThemeManager.cpp
│   │   ├── QtSeedDisplayDialog.cpp
│   │   ├── QtExpandableWalletCard.cpp
│   │   ├── assets/             # Qt resources
│   │   └── include/
│   │       ├── CriptoGualetQt.h
│   │       ├── QtLoginUI.h
│   │       ├── QtWalletUI.h
│   │       ├── QtThemeManager.h
│   │       ├── QtSeedDisplayDialog.h
│   │       └── QtExpandableWalletCard.h
│   └── win32/                  # Win32 GUI (legacy)
│       ├── CMakeLists.txt
│       ├── CriptoGualet.cpp
│       └── include/
│           └── CriptoGualet.h
├── Tests/                      # Test suite
│   ├── CMakeLists.txt
│   ├── test_basic_auth.cpp
│   ├── test_bip39.cpp
│   ├── test_secure_seed.cpp
│   ├── test_database.cpp
│   ├── test_repository.cpp
│   ├── test_security_enhancements.cpp
│   └── test_blockcypher_api.cpp
├── src/                        # Build orchestration
│   └── CMakeLists.txt
├── resources/                  # Application resources
│   └── resources.qrc
├── out/                        # Build output directory
│   └── build/
│       └── win-clang-x64-debug/
└── ProjectGuides/              # Documentation
```

## Key Features
- **Non-custodial** - Users control their private keys
- **Cross-platform** - Windows, macOS, Linux support via Qt
- **Secure** - Windows CryptoAPI integration, security compiler flags, encrypted database
- **Modern UI** - Qt6 with theme management
- **QR Codes** - Generate QR codes for addresses and seed phrases
- **BIP39** - Standard seed phrase generation and management
- **Blockchain Integration** - BlockCypher API for Bitcoin network interaction
- **Database Persistence** - SQLCipher encrypted storage for wallet data
- **Modular Architecture** - Separated backend/frontend for maintainability
- **Multiple Builds** - Both Qt and Win32 GUI options

## Development Notes
- **Modular Structure** - Backend and frontend components are cleanly separated
- **Primary startup project**: `CriptoGualetQt` (Qt GUI)
- **Fallback startup project**: `CriptoGualet` (Win32 GUI - legacy)
- **All libraries built as static** for security
- **Automatic Qt DLL deployment** on Windows
- **Cross-platform security flags** enabled
- **Export compile commands** for IntelliSense support
- **CMake modularity** - Each component has its own CMakeLists.txt

## MCP Integration
- GitHub MCP server configured for enhanced context and repository operations
- Environment variable `GITHUB_PAT` stores GitHub Personal Access Token securely
- MCP configuration file `.mcp.json` excluded from version control for security
- Enables direct GitHub API access for issues, PRs, and repository management through Claude Code
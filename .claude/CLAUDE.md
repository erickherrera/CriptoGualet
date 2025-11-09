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
- **PriceService** - Cryptocurrency price service for market data

#### Database (`backend/database/`)
- **DatabaseManager** - Encrypted SQLCipher-based storage for wallet data

#### Repository (`backend/repository/`)
- **UserRepository** - User data access layer
- **WalletRepository** - Wallet data access layer
- **TransactionRepository** - Transaction data access layer with extensions
- **Logger** - Application logging

#### Utils (`backend/utils/`)
- **QRGenerator** - QR code generation with libqrencode
- **SharedSymbols** - Shared symbols and utilities
- **SharedTypes** - Shared type definitions across modules

### Frontend Components

#### Qt GUI (`frontend/qt/`) - Primary Interface
Modern cross-platform interface using Qt6:
- `CriptoGualetQt.cpp` - Main Qt application
- `QtLoginUI.cpp` - Qt login and registration interface
- `QtWalletUI.cpp` - Qt wallet interface
- `QtSidebar.cpp` - Sidebar navigation component
- `QtTopCryptosPage.cpp` - Top cryptocurrencies display page
- `QtSettingsUI.cpp` - Settings interface
- `QtThemeManager.cpp` - Theme management
- `QtSeedDisplayDialog.cpp` - Seed phrase display dialog
- `QtSendDialog.cpp` - Bitcoin transaction send dialog
- `QtExpandableWalletCard.cpp` - Wallet card UI component

### Test Suite (`Tests/`)
- `TestUtils.cpp/h` - Test utilities and helpers
- `test_auth_database_integration.cpp` - Authentication and database integration tests
- `test_bip39.cpp` - BIP39 seed phrase tests
- `test_secure_seed.cpp` - Secure seed generation tests
- `test_database.cpp` - Database functionality tests
- `test_user_repository.cpp` - User repository tests
- `test_wallet_repository.cpp` - Wallet repository tests
- `test_transaction_repository.cpp` - Transaction repository tests
- `test_repository_integration.cpp` - Repository integration tests
- `test_security_enhancements.cpp` - Security feature tests
- `test_password_verification.cpp` - Password verification tests
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
- **secp256k1** - Elliptic curve cryptography library
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
├── CppProperties.json          # IntelliSense configuration
├── launch.vs.json              # Visual Studio launch settings
├── vcpkg.json                  # Dependency management
├── .clang-format              # Code formatting rules
├── .clang-tidy                # Static analysis rules
├── .claude/                    # Claude Code settings
│   ├── settings.local.json
│   ├── CLAUDE.md              # Project documentation
│   └── agents/                # Specialized AI agents
│       ├── architecture-documentation-expert.md
│       ├── database-architect.md
│       ├── debug-specialist.md
│       ├── macos-crypto-wallet-integration.md
│       ├── security-auditor.md
│       ├── system-design-agent.md
│       ├── ui-designer-crypto-wallet.md
│       └── wallet-core-developer.md
├── assets/                     # Root-level assets
│   └── bip39/
│       └── english.txt         # BIP39 English wordlist
├── examples/                   # Example code
│   └── wallet_integration_example.cpp
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
│   │   ├── PriceService.cpp
│   │   └── include/
│   │       ├── BlockCypher.h
│   │       └── PriceService.h
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
│   │   │   ├── TransactionRepositoryExtensions.cpp
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
│           ├── QRGenerator.h
│           └── SharedTypes.h
├── frontend/                   # Frontend components
│   └── qt/                     # Qt GUI (primary)
│       ├── CMakeLists.txt
│       ├── CriptoGualetQt.cpp
│       ├── QtLoginUI.cpp
│       ├── QtWalletUI.cpp
│       ├── QtSidebar.cpp
│       ├── QtTopCryptosPage.cpp
│       ├── QtSettingsUI.cpp
│       ├── QtThemeManager.cpp
│       ├── QtSeedDisplayDialog.cpp
│       ├── QtSendDialog.cpp
│       ├── QtExpandableWalletCard.cpp
│       ├── assets/             # Qt resources
│       │   └── bip39/
│       │       └── english.txt
│       └── include/
│           ├── CriptoGualetQt.h
│           ├── QtLoginUI.h
│           ├── QtWalletUI.h
│           ├── QtSidebar.h
│           ├── QtTopCryptosPage.h
│           ├── QtSettingsUI.h
│           ├── QtThemeManager.h
│           ├── QtSeedDisplayDialog.h
│           ├── QtSendDialog.h
│           └── QtExpandableWalletCard.h
├── Tests/                      # Test suite
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── TestUtils.h
│   ├── src/
│   │   └── TestUtils.cpp
│   ├── test_auth_database_integration.cpp
│   ├── test_bip39.cpp
│   ├── test_secure_seed.cpp
│   ├── test_database.cpp
│   ├── test_user_repository.cpp
│   ├── test_wallet_repository.cpp
│   ├── test_transaction_repository.cpp
│   ├── test_repository_integration.cpp
│   ├── test_security_enhancements.cpp
│   ├── test_password_verification.cpp
│   └── test_blockcypher_api.cpp
├── src/                        # Build orchestration
│   └── CMakeLists.txt
├── resources/                  # Application resources
│   ├── resources.qrc
│   └── icons/
│       ├── chart.svg
│       ├── eye-closed.svg
│       ├── eye-open.svg
│       ├── menu.svg
│       ├── settings.svg
│       └── wallet.svg
├── out/                        # Build output directory
│   └── build/
│       └── win-clang-x64-debug/
└── ProjectGuides/              # Documentation
```

## Key Features
- **Non-custodial** - Users control their private keys
- **Cross-platform** - Windows, macOS, Linux support via Qt6
- **Secure** - Windows CryptoAPI integration, secp256k1 elliptic curve cryptography, security compiler flags, encrypted database
- **Modern UI** - Qt6 with theme management, sidebar navigation, and settings interface
- **Transaction Management** - Send Bitcoin with fee estimation and transaction confirmation dialog
- **Price Service** - Real-time cryptocurrency market data and pricing
- **Top Cryptocurrencies** - View and track top performing cryptocurrencies
- **QR Codes** - Generate QR codes for addresses and seed phrases
- **BIP39** - Standard seed phrase generation and management
- **Blockchain Integration** - BlockCypher API for Bitcoin network interaction
- **Database Persistence** - SQLCipher encrypted storage for wallet data
- **Modular Architecture** - Separated backend/frontend for maintainability
- **Comprehensive Testing** - Individual repository tests and integration tests

## Development Notes
- **Modular Structure** - Backend and frontend components are cleanly separated
- **Startup project**: `CriptoGualetQt` (Qt6 GUI application)
- **All libraries built as static** for security
- **Automatic Qt DLL deployment** on Windows
- **Cross-platform security flags** enabled
- **Export compile commands** for IntelliSense support (CppProperties.json)
- **CMake modularity** - Each component has its own CMakeLists.txt
- **Enhanced UI Components** - Sidebar navigation, settings panel, top cryptocurrencies view, send transaction dialog
- **Test Utilities** - Shared test helpers for consistent testing across modules

## AI Development Assistance
The project includes specialized Claude Code agents in `.claude/agents/` for streamlined development:
- **architecture-documentation-expert** - System architecture documentation and diagrams
- **database-architect** - Database schema design and optimization
- **debug-specialist** - Systematic debugging and issue resolution
- **macos-crypto-wallet-integration** - macOS-specific features and deployment
- **security-auditor** - Security vulnerability analysis and cryptographic review
- **system-design-agent** - System design and architectural planning
- **ui-designer-crypto-wallet** - UI/UX design for cryptocurrency wallet interfaces
- **wallet-core-developer** - Core wallet functionality implementation

## MCP Integration
- GitHub MCP server configured for enhanced context and repository operations
- Environment variable `GITHUB_PAT` stores GitHub Personal Access Token securely
- MCP configuration file `.mcp.json` excluded from version control for security
- Enables direct GitHub API access for issues, PRs, and repository management through Claude Code
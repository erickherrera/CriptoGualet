# CriptoGualet System Architecture

## Table of Contents
1. [Overview](#overview)
2. [C4 Model Diagrams](#c4-model-diagrams)
3. [System Context](#system-context)
4. [Container Architecture](#container-architecture)
5. [Component Architecture](#component-architecture)
6. [Technology Stack](#technology-stack)
7. [Design Principles](#design-principles)
8. [Security Architecture](#security-architecture)

---

## Overview

CriptoGualet is a **non-custodial, cross-platform cryptocurrency wallet** that enables users to securely manage multiple blockchain assets from a single application. The system follows a layered architecture with clear separation between backend business logic, data persistence, blockchain integration, and frontend presentation.

### Key Characteristics

- **Non-custodial**: Users maintain full control of their private keys
- **HD Wallet**: BIP39/BIP32/BIP44 compliant hierarchical deterministic wallet
- **Multi-chain**: Single seed phrase manages Bitcoin, Ethereum, and future blockchains
- **Cross-platform**: Qt6-based UI runs on Windows, macOS, and Linux
- **Secure**: Multi-layer encryption (DPAPI + SQLCipher), on-demand key derivation
- **Modular**: Clean separation of concerns for maintainability and testability

---

## C4 Model Diagrams

### Level 1: System Context Diagram

```mermaid
graph TB
    User[User<br/>Cryptocurrency Holder]

    subgraph "CriptoGualet System"
        App[CriptoGualet<br/>Desktop Wallet Application<br/>Multi-chain HD Wallet]
    end

    BlockCypherAPI[BlockCypher API<br/>Bitcoin Blockchain Service]
    EtherscanAPI[Etherscan API<br/>Ethereum Blockchain Service]
    PriceAPI[Price Service APIs<br/>CoinGecko/CoinMarketCap]
    WindowsDPAPI[Windows DPAPI<br/>OS-Level Encryption]

    User -->|Manages wallets,<br/>sends/receives crypto| App
    App -->|Queries Bitcoin balances,<br/>broadcasts transactions| BlockCypherAPI
    App -->|Queries Ethereum balances,<br/>retrieves transaction history| EtherscanAPI
    App -->|Fetches real-time<br/>cryptocurrency prices| PriceAPI
    App -->|Encrypts/decrypts<br/>seed phrases| WindowsDPAPI

    style App fill:#4A90E2,stroke:#2E5C8A,stroke-width:3px,color:#fff
```

**Key Interactions:**
- **User** interacts with the wallet to create accounts, view balances, send/receive cryptocurrency, and manage settings
- **BlockCypher API** provides Bitcoin testnet/mainnet blockchain data and transaction broadcasting
- **Etherscan API** provides Ethereum mainnet/testnet blockchain data and gas prices
- **Price Service APIs** supply real-time cryptocurrency market data for USD valuations
- **Windows DPAPI** provides machine-bound encryption for local seed storage

---

### Level 2: Container Diagram

```mermaid
graph TB
    subgraph "User's Desktop Machine"
        subgraph "Qt6 Frontend Application"
            QtUI[Qt GUI Container<br/>Technology: Qt6 Widgets<br/>- Login/Registration UI<br/>- Wallet Dashboard<br/>- Transaction Dialogs<br/>- Settings Interface]
        end

        subgraph "Backend Core Logic"
            Auth[Auth Module<br/>Technology: C++<br/>- User authentication<br/>- Seed generation<br/>- Wallet creation]

            Crypto[Crypto Module<br/>Technology: C++, secp256k1<br/>- BIP39 mnemonic generation<br/>- BIP32/BIP44 key derivation<br/>- Bitcoin/Ethereum address generation<br/>- ECDSA signing]

            WalletAPI[WalletAPI Layer<br/>Technology: C++<br/>- SimpleWallet (Bitcoin)<br/>- EthereumWallet (Ethereum)<br/>- Unified wallet operations]
        end

        subgraph "Blockchain Integration"
            BlockCypher[BlockCypher Client<br/>Technology: C++, CPR HTTP<br/>- Bitcoin API wrapper<br/>- UTXO management<br/>- Transaction creation]

            EthereumSvc[Ethereum Service<br/>Technology: C++, CPR HTTP<br/>- Etherscan API wrapper<br/>- Wei/ETH conversion<br/>- Gas price estimation]

            PriceSvc[Price Service<br/>Technology: C++, HTTP<br/>- Multi-source pricing<br/>- USD conversion]
        end

        subgraph "Data Persistence"
            Database[SQLCipher Database<br/>Technology: SQLite + AES-256<br/>- Encrypted seed storage<br/>- User accounts<br/>- Wallet metadata<br/>- Transaction history]

            Repos[Repository Layer<br/>Technology: C++<br/>- UserRepository<br/>- WalletRepository<br/>- TransactionRepository]

            DPAPI[DPAPI Storage<br/>Technology: Windows CryptoAPI<br/>- Machine-bound encryption<br/>- Binary seed files]
        end

        subgraph "Utilities"
            QRGen[QR Generator<br/>Technology: libqrencode<br/>- Seed phrase QR codes<br/>- Address QR codes]

            Logger[Logger<br/>Technology: C++<br/>- Application logging<br/>- Debug traces]
        end
    end

    subgraph "External Services"
        BlockCypherAPI[(BlockCypher API<br/>Bitcoin Blockchain)]
        EtherscanAPI[(Etherscan API<br/>Ethereum Blockchain)]
        PriceAPIs[(CoinGecko/CMC<br/>Price Data)]
    end

    QtUI -->|User actions| Auth
    QtUI -->|Wallet operations| WalletAPI
    QtUI -->|QR code generation| QRGen

    Auth -->|Cryptographic operations| Crypto
    Auth -->|Store/retrieve users| Repos
    Auth -->|Encrypt seeds| DPAPI

    WalletAPI -->|Bitcoin operations| BlockCypher
    WalletAPI -->|Ethereum operations| EthereumSvc
    WalletAPI -->|Price data| PriceSvc

    BlockCypher -->|HTTP requests| BlockCypherAPI
    EthereumSvc -->|HTTP requests| EtherscanAPI
    PriceSvc -->|HTTP requests| PriceAPIs

    Repos -->|Database queries| Database
    Repos -->|Encryption/decryption| Crypto

    Crypto -->|Seed encryption| Database

    style QtUI fill:#4A90E2,stroke:#2E5C8A,stroke-width:2px,color:#fff
    style Auth fill:#50C878,stroke:#2E7D4E,stroke-width:2px,color:#fff
    style Crypto fill:#50C878,stroke:#2E7D4E,stroke-width:2px,color:#fff
    style WalletAPI fill:#50C878,stroke:#2E7D4E,stroke-width:2px,color:#fff
    style Database fill:#F4A460,stroke:#C17D3A,stroke-width:2px,color:#fff
    style Repos fill:#F4A460,stroke:#C17D3A,stroke-width:2px,color:#fff
    style DPAPI fill:#F4A460,stroke:#C17D3A,stroke-width:2px,color:#fff
    style BlockCypher fill:#9B59B6,stroke:#6C3483,stroke-width:2px,color:#fff
    style EthereumSvc fill:#9B59B6,stroke:#6C3483,stroke-width:2px,color:#fff
    style PriceSvc fill:#9B59B6,stroke:#6C3483,stroke-width:2px,color:#fff
```

**Container Responsibilities:**

| Container | Technology | Responsibility |
|-----------|-----------|----------------|
| **Qt GUI Container** | Qt6 Widgets | User interaction, visual presentation, input validation |
| **Auth Module** | C++ | User registration/login, seed generation, wallet initialization |
| **Crypto Module** | C++, secp256k1 | BIP39/BIP32/BIP44 implementation, cryptographic operations |
| **WalletAPI Layer** | C++ | High-level wallet abstractions, blockchain-agnostic operations |
| **BlockCypher Client** | C++, CPR | Bitcoin blockchain interaction, transaction broadcasting |
| **Ethereum Service** | C++, CPR | Ethereum blockchain interaction, balance queries |
| **Price Service** | C++, HTTP | Cryptocurrency price aggregation |
| **SQLCipher Database** | SQLite + AES-256 | Persistent encrypted storage |
| **Repository Layer** | C++ | Data access abstraction, ORM-like patterns |
| **DPAPI Storage** | Windows CryptoAPI | OS-level encrypted seed storage |
| **QR Generator** | libqrencode | QR code generation for seeds and addresses |
| **Logger** | C++ | Application-wide logging and debugging |

---

### Level 3: Component Diagram - Backend Core

```mermaid
graph TB
    subgraph "Qt Frontend"
        QtLoginUI[QtLoginUI]
        QtWalletUI[QtWalletUI]
        QtSendDialog[QtSendDialog]
    end

    subgraph "Auth Module"
        RegisterUser[RegisterUserWithMnemonic]
        LoginUser[LoginUser]
        SeedGenerator[GenerateAndActivateSeedForUser]
        WalletCreator[CreateMultiChainWallets]
    end

    subgraph "Crypto Module"
        BIP39[BIP39 Functions<br/>- GenerateEntropy<br/>- MnemonicFromEntropy<br/>- SeedFromMnemonic]

        BIP32[BIP32 Functions<br/>- MasterKeyFromSeed<br/>- DeriveChild<br/>- DerivePath]

        BIP44[BIP44 Functions<br/>- GetAddress (Bitcoin)<br/>- GetEthereumAddress<br/>- DeriveChainAddress]

        AddressGen[Address Generators<br/>- GetBitcoinAddress<br/>- GetEthereumAddress<br/>- Keccak256 (ETH)]

        Signing[Signature Functions<br/>- SignHash<br/>- ECDSA with secp256k1]

        Encryption[Encryption Functions<br/>- EncryptDBData (AES-GCM)<br/>- DecryptDBData<br/>- DeriveDBEncryptionKey]
    end

    subgraph "WalletAPI Layer"
        SimpleWallet[SimpleWallet<br/>Bitcoin]
        EthereumWallet[EthereumWallet<br/>Ethereum]
        WalletOps[Common Operations<br/>- GetBalance<br/>- GetTransactionHistory<br/>- SendFunds<br/>- ValidateAddress]
    end

    subgraph "Repository Layer"
        UserRepo[UserRepository<br/>- createUser<br/>- authenticateUser<br/>- getUserByUsername]

        WalletRepo[WalletRepository<br/>- createWallet<br/>- getWalletsByUserId<br/>- storeEncryptedSeed<br/>- retrieveDecryptedSeed]

        TxRepo[TransactionRepository<br/>- createTransaction<br/>- getTransactionsByWallet<br/>- updateTransactionStatus]
    end

    QtLoginUI --> RegisterUser
    QtLoginUI --> LoginUser
    QtWalletUI --> SimpleWallet
    QtWalletUI --> EthereumWallet
    QtSendDialog --> SimpleWallet

    RegisterUser --> SeedGenerator
    RegisterUser --> WalletCreator
    LoginUser --> UserRepo

    SeedGenerator --> BIP39
    WalletCreator --> BIP44
    WalletCreator --> AddressGen

    BIP39 --> BIP32
    BIP32 --> BIP44
    BIP44 --> AddressGen

    SimpleWallet --> WalletOps
    EthereumWallet --> WalletOps
    WalletOps --> Signing

    RegisterUser --> UserRepo
    RegisterUser --> WalletRepo
    WalletCreator --> WalletRepo

    WalletRepo --> Encryption

    style QtLoginUI fill:#4A90E2,stroke:#2E5C8A,stroke-width:2px,color:#fff
    style QtWalletUI fill:#4A90E2,stroke:#2E5C8A,stroke-width:2px,color:#fff
    style QtSendDialog fill:#4A90E2,stroke:#2E5C8A,stroke-width:2px,color:#fff
```

---

### Level 3: Component Diagram - Frontend Qt UI

```mermaid
graph TB
    subgraph "Main Application"
        CriptoGualetQt[CriptoGualetQt<br/>Main Window]
        ThemeManager[QtThemeManager<br/>Theme System]
    end

    subgraph "Authentication Views"
        QtLoginUI[QtLoginUI<br/>Login/Registration]
        SeedDialog[QtSeedDisplayDialog<br/>Seed Phrase Display]
    end

    subgraph "Wallet Views"
        QtWalletUI[QtWalletUI<br/>Main Wallet Dashboard]
        QtSidebar[QtSidebar<br/>Navigation Sidebar]
        QtTopCryptos[QtTopCryptosPage<br/>Market Overview]
        QtSettings[QtSettingsUI<br/>Settings Panel]
    end

    subgraph "Wallet Components"
        BTCCard[QtExpandableWalletCard<br/>Bitcoin Wallet]
        ETHCard[QtExpandableWalletCard<br/>Ethereum Wallet]
        SendDialog[QtSendDialog<br/>Transaction Send Dialog]
    end

    subgraph "Backend Integration"
        Auth[Auth Module]
        SimpleWallet[SimpleWallet]
        EthereumWallet[EthereumWallet]
        PriceService[PriceService]
        QRGenerator[QRGenerator]
    end

    CriptoGualetQt --> QtLoginUI
    CriptoGualetQt --> QtWalletUI
    CriptoGualetQt --> ThemeManager

    QtLoginUI --> SeedDialog
    QtLoginUI --> Auth

    QtWalletUI --> QtSidebar
    QtWalletUI --> QtTopCryptos
    QtWalletUI --> QtSettings
    QtWalletUI --> BTCCard
    QtWalletUI --> ETHCard
    QtWalletUI --> SendDialog

    BTCCard -->|sendRequested signal| SendDialog
    ETHCard -->|receiveRequested signal| QtWalletUI

    SendDialog --> SimpleWallet
    QtWalletUI --> SimpleWallet
    QtWalletUI --> EthereumWallet
    QtTopCryptos --> PriceService
    SeedDialog --> QRGenerator

    ThemeManager -->|applies theme| QtLoginUI
    ThemeManager -->|applies theme| QtWalletUI
    ThemeManager -->|applies theme| BTCCard
    ThemeManager -->|applies theme| ETHCard

    style CriptoGualetQt fill:#4A90E2,stroke:#2E5C8A,stroke-width:3px,color:#fff
    style ThemeManager fill:#E74C3C,stroke:#A93226,stroke-width:2px,color:#fff
```

---

## System Context

### User Personas

**Primary User: Cryptocurrency Holder**
- Wants to manage multiple blockchain assets from one application
- Needs to send and receive cryptocurrency securely
- Values privacy and control over private keys
- Expects user-friendly interface with clear feedback

### External Systems

1. **BlockCypher API**
   - Provides Bitcoin blockchain data (testnet3 and mainnet)
   - Supports balance queries, transaction history, UTXO retrieval
   - Handles transaction creation and broadcasting
   - Free tier: 200 requests/hour

2. **Etherscan API**
   - Provides Ethereum blockchain data (mainnet, sepolia, goerli)
   - Supports balance queries, transaction history, gas price estimation
   - Read-only operations (no transaction broadcasting yet)
   - Free tier: 5 calls/second

3. **Price Service APIs**
   - CoinGecko, CoinMarketCap for real-time prices
   - Supports Bitcoin, Ethereum, and top cryptocurrencies
   - Used for USD balance calculations

4. **Windows DPAPI**
   - Operating system-level encryption service
   - Machine and user-bound encryption
   - Used for local seed phrase storage

---

## Container Architecture

### Frontend Layer: Qt6 GUI

**Responsibilities:**
- User interaction and input validation
- Visual presentation and theming
- Navigation between views
- Event handling and signal/slot connections

**Key Components:**
- **CriptoGualetQt**: Main application window, view orchestration
- **QtLoginUI**: Registration and login interface
- **QtWalletUI**: Primary wallet dashboard with multi-chain support
- **QtSidebar**: Navigation sidebar with route management
- **QtExpandableWalletCard**: Reusable wallet display component
- **QtSendDialog**: Transaction creation dialog
- **QtThemeManager**: Centralized theme management (4 built-in themes)

**Technology:**
- Qt6 Widgets framework
- Signal/slot architecture for loose coupling
- Qt StyleSheets for theming
- QImage for QR code display

---

### Backend Layer: Core Business Logic

**Responsibilities:**
- User authentication and session management
- Cryptographic operations (BIP39/BIP32/BIP44)
- Wallet address generation (Bitcoin, Ethereum)
- Transaction signing with ECDSA
- Data encryption/decryption

**Key Modules:**

#### Auth Module
- User registration with mnemonic seed generation
- Login with password verification
- Session management
- Multi-chain wallet initialization

#### Crypto Module
- BIP39: Entropy generation, mnemonic encoding/decoding
- BIP32: Hierarchical deterministic key derivation
- BIP44: Multi-account hierarchy for different blockchains
- Address generation: Bitcoin (P2PKH), Ethereum (Keccak256)
- Signing: ECDSA signatures with secp256k1 curve
- Encryption: AES-256-GCM for database, DPAPI for local storage

#### WalletAPI Layer
- **SimpleWallet**: Bitcoin wallet abstraction
  - Balance queries, transaction history
  - UTXO management, fee estimation
  - Transaction creation and broadcasting
- **EthereumWallet**: Ethereum wallet abstraction
  - Balance queries (Wei to ETH conversion)
  - Transaction history, gas price estimation
  - Address validation

**Technology:**
- Modern C++17/20
- secp256k1 library for elliptic curve cryptography
- Windows CryptoAPI (bcrypt, crypt32)
- Keccak256 for Ethereum addresses

---

### Blockchain Integration Layer

**Responsibilities:**
- Interface with external blockchain APIs
- HTTP request/response handling
- Data format conversion (JSON parsing)
- Error handling and retry logic

**Key Services:**

#### BlockCypher Client
- Bitcoin testnet3 and mainnet support
- REST API integration via CPR library
- UTXO retrieval for transaction building
- Transaction broadcasting
- Fee estimation

#### Ethereum Service
- Etherscan API integration
- Multiple network support (mainnet, sepolia)
- Wei/Gwei/ETH unit conversions
- Transaction history parsing
- Gas price oracle

#### Price Service
- Multi-source price aggregation
- Top cryptocurrencies market data
- USD valuation calculations
- Rate limiting and caching

**Technology:**
- CPR (C++ HTTP client)
- nlohmann-json for JSON parsing
- HTTPS for secure communication

---

### Data Persistence Layer

**Responsibilities:**
- Secure storage of encrypted seeds
- User account management
- Wallet metadata storage
- Transaction history persistence

**Key Components:**

#### SQLCipher Database
- Encrypted SQLite database (AES-256-CBC)
- Tables: users, wallets, addresses, encrypted_seeds, transactions
- ACID compliance
- Schema migrations

#### Repository Layer
- **UserRepository**: User CRUD operations, authentication
- **WalletRepository**: Wallet management, seed storage/retrieval
- **TransactionRepository**: Transaction history tracking

#### DPAPI Storage
- Windows-specific machine-bound encryption
- Binary seed files in `seed_vault/` directory
- Fast access for frequent operations

**Technology:**
- SQLCipher for encrypted database
- Windows DPAPI for OS-level encryption
- C++ filesystem API

---

### Utility Layer

**QR Generator**
- libqrencode integration
- Fallback pattern when library unavailable
- Seed phrase QR codes
- Address QR codes

**Logger**
- Application-wide logging
- Debug trace output
- Error reporting

---

## Technology Stack

### Core Technologies

| Layer | Technology | Version | Purpose |
|-------|-----------|---------|---------|
| **Frontend** | Qt6 | 6.8+ | Cross-platform GUI framework |
| **Language** | C++ | C++17/20 | System programming language |
| **Build System** | CMake | 3.20+ | Build configuration and management |
| **Package Manager** | vcpkg | Latest | Dependency management |

### Cryptography Libraries

| Library | Purpose |
|---------|---------|
| **secp256k1** | Elliptic curve operations, ECDSA signatures |
| **Keccak256** | Ethereum address generation |
| **Windows CryptoAPI** | PBKDF2, AES-GCM, DPAPI encryption |

### Database & Storage

| Technology | Purpose |
|-----------|---------|
| **SQLCipher** | Encrypted SQLite database (AES-256) |
| **Windows DPAPI** | Machine-bound seed encryption |

### Networking & APIs

| Library | Purpose |
|---------|---------|
| **CPR** | HTTP client for REST APIs |
| **nlohmann-json** | JSON parsing and serialization |

### Additional Libraries

| Library | Purpose |
|---------|---------|
| **libqrencode** | QR code generation |
| **Qt SVG** | SVG icon rendering |

---

## Design Principles

### 1. Separation of Concerns

**Backend/Frontend Separation:**
- Backend modules (core, blockchain, database, repository) are completely independent of UI
- Frontend communicates through well-defined APIs
- Enables testing backend without GUI

**Layered Architecture:**
```
┌─────────────────────────────────────────┐
│          Presentation Layer (Qt UI)     │
├─────────────────────────────────────────┤
│      Application Layer (WalletAPI)      │
├─────────────────────────────────────────┤
│     Domain Layer (Auth, Crypto)         │
├─────────────────────────────────────────┤
│  Infrastructure Layer (Database, APIs)  │
└─────────────────────────────────────────┘
```

### 2. Security by Design

**Defense in Depth:**
- Multi-layer encryption (DPAPI + SQLCipher)
- On-demand key derivation (keys never cached)
- Password-based encryption with PBKDF2 (100,000+ iterations)
- Secure memory wiping after cryptographic operations

**Principle of Least Privilege:**
- Private keys only exist in memory during transaction signing
- Seeds encrypted at rest with multiple layers
- No logging of sensitive data

### 3. Modularity

**Single Responsibility:**
- Each module has one clear purpose
- Easy to test in isolation
- Minimal coupling between modules

**Dependency Injection:**
- Repositories injected into services
- Crypto module injected where needed
- Facilitates mocking for tests

### 4. Standards Compliance

**BIP Standards:**
- BIP39: Mnemonic seed phrases (industry standard)
- BIP32: Hierarchical deterministic wallets
- BIP44: Multi-account hierarchy for multi-currency

**Wallet Compatibility:**
- Seeds compatible with Ledger, Trezor, MetaMask
- Standard derivation paths
- Portable across wallet applications

### 5. Extensibility

**Multi-Chain Support:**
- New blockchains added with minimal code changes
- Unified WalletAPI interface
- Chain-specific services pluggable

**Theme System:**
- Centralized theme management
- Easy to add new color schemes
- Consistent styling across all components

### 6. Testability

**Comprehensive Test Suite:**
- Unit tests for each module
- Integration tests for workflows
- Test utilities and mocking infrastructure

---

## Security Architecture

### Threat Model

**Protected Assets:**
- BIP39 seed phrases (12-word mnemonics)
- Private keys derived from seeds
- User passwords
- Transaction data

**Threat Actors:**
- Malware on user's machine
- Network attackers (MITM)
- Physical attackers (stolen device)
- Insider threats (compromised APIs)

### Security Mechanisms

#### 1. Seed Phrase Protection

**Dual-Layer Encryption:**

```mermaid
graph TB
    Plaintext[Plaintext Seed Phrase<br/>12 words]

    subgraph "Encryption Layer 1: DPAPI"
        DPAPI[Windows DPAPI Encryption<br/>Machine + User Bound]
        DPAPIFile[seed_vault/username.bin<br/>Encrypted Binary]
    end

    subgraph "Encryption Layer 2: Database"
        PasswordKey[Password-Derived Key<br/>PBKDF2-HMAC-SHA256<br/>100,000 iterations]
        AESGCM[AES-256-GCM Encryption]
        DBBlob[encrypted_seeds table<br/>Encrypted Blob]
    end

    Plaintext --> DPAPI
    DPAPI --> DPAPIFile

    Plaintext --> PasswordKey
    PasswordKey --> AESGCM
    AESGCM --> DBBlob

    DPAPIFile -.Attacker needs<br/>user account + machine.-> Attack1[X]
    DBBlob -.Attacker needs<br/>user password.-> Attack2[X]
```

**Security Properties:**
- **DPAPI**: Protects against file theft (requires user account access)
- **Database**: Protects against unauthorized device access (requires password)
- **Combined**: Attacker needs both device access AND password

#### 2. Private Key Management

**On-Demand Derivation:**
1. User enters password for transaction
2. Seed decrypted from database
3. Private key derived via BIP32/BIP44
4. Transaction signed
5. Private key wiped from memory immediately

**No Caching:**
- Keys never stored in memory longer than necessary
- Reduces attack surface for memory dumps

#### 3. Password Security

**Storage:**
- PBKDF2-HMAC-SHA256 with random salt
- 100,000+ iterations (configurable)
- Constant-time comparison to prevent timing attacks

**Verification:**
- Rate limiting: 5 attempts, then 10-minute lockout
- Prevents brute force attacks

#### 4. Network Security

**API Communication:**
- HTTPS for all external API calls
- API tokens stored securely (not in source code)
- Rate limiting respected
- Timeout handling

#### 5. Input Validation

**Address Validation:**
- Checksum verification for Bitcoin addresses
- Format validation for Ethereum addresses
- Prevents sending to invalid addresses

**Transaction Validation:**
- Amount validation (sufficient balance, positive values)
- Fee validation (reasonable fee estimates)
- User confirmation required

### Security Best Practices

1. **Regular Security Audits**: Code reviews for cryptographic operations
2. **Dependency Updates**: Keep libraries updated for security patches
3. **Secure Coding**: No buffer overflows, proper error handling
4. **Minimal Logging**: Never log seeds, private keys, or passwords
5. **User Education**: Clear warnings about seed phrase backup importance

---

## Deployment Architecture

```mermaid
graph TB
    subgraph "User's Desktop Machine"
        subgraph "Application Bundle"
            QtApp[CriptoGualetQt.exe<br/>Qt GUI Application]
            BackendLibs[Backend Static Libraries<br/>Auth, Crypto, WalletAPI]
            QtDLLs[Qt6 Runtime DLLs<br/>Core, Widgets, Gui, Svg]
            VcpkgDLLs[Dependency DLLs<br/>secp256k1, SQLCipher, CPR]
        end

        subgraph "User Data Directory"
            Database[wallet.db<br/>SQLCipher Encrypted Database]
            SeedVault[seed_vault/<br/>DPAPI Encrypted Seeds]
            Logs[logs/<br/>Application Logs]
        end

        subgraph "Application Resources"
            WordList[assets/bip39/english.txt<br/>BIP39 Wordlist]
            Icons[resources/icons/<br/>SVG Icons]
        end

        subgraph "Windows OS Services"
            DPAPI[Windows DPAPI Service]
            CryptoAPI[Windows CryptoAPI<br/>bcrypt.dll, crypt32.dll]
        end
    end

    QtApp --> BackendLibs
    QtApp --> QtDLLs
    QtApp --> VcpkgDLLs

    BackendLibs --> Database
    BackendLibs --> SeedVault
    BackendLibs --> WordList

    BackendLibs --> DPAPI
    BackendLibs --> CryptoAPI

    QtApp --> Icons
    QtApp --> Logs

    style QtApp fill:#4A90E2,stroke:#2E5C8A,stroke-width:3px,color:#fff
    style Database fill:#F4A460,stroke:#C17D3A,stroke-width:2px,color:#000
    style SeedVault fill:#F4A460,stroke:#C17D3A,stroke-width:2px,color:#000
```

### Deployment Targets

- **Windows 10/11**: Primary platform with full DPAPI support
- **macOS**: Cross-platform via Qt6 (Keychain instead of DPAPI)
- **Linux**: Cross-platform via Qt6 (Keyring instead of DPAPI)

---

**Document Version:** 1.0
**Last Updated:** 2025-11-16
**Author:** Claude (Architecture Documentation Expert)
**Project:** CriptoGualet - Cross-Platform Cryptocurrency Wallet

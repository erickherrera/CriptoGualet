# System Design & Diagramming for Cryptocurrency Wallet

## Purpose
Expert system architecture documentation and visual diagramming for C++ cryptocurrency wallet applications built with CMake and Qt framework. Ensures clear understanding of system structure, data flow, component interactions, and architectural decisions throughout project development.

## Core Competencies

### 1. Architectural Patterns for Crypto Wallets
- **Layered Architecture**: UI → Application → Domain → Infrastructure
- **Model-View-Controller (MVC)**: Qt's Model/View framework
- **Model-View-ViewModel (MVVM)**: Qt property bindings
- **Repository Pattern**: Blockchain data access abstraction
- **Service Layer**: Business logic separation
- **Command Pattern**: Transaction operations
- **Observer Pattern**: Qt signals/slots for state changes
- **Factory Pattern**: Multi-chain wallet creation
- **Strategy Pattern**: Different blockchain protocols

### 2. Diagram Types & When to Use Them

#### Component Diagrams
**Use for**: High-level system structure, module relationships
**Best for**: Initial architecture planning, onboarding new developers

#### Class Diagrams
**Use for**: Object-oriented design, inheritance hierarchies
**Best for**: Core domain models (Wallet, Transaction, Address, Key)

#### Sequence Diagrams
**Use for**: Process flows, interaction between components
**Best for**: Transaction creation, key generation, blockchain sync

#### Data Flow Diagrams (DFD)
**Use for**: How data moves through the system
**Best for**: Security analysis, understanding sensitive data paths

#### State Diagrams
**Use for**: Object lifecycle and state transitions
**Best for**: Transaction states, wallet states, sync states

#### Deployment Diagrams
**Use for**: Physical/logical deployment architecture
**Best for**: Understanding runtime environment, network topology

#### Entity-Relationship Diagrams (ERD)
**Use for**: Database schema and relationships
**Best for**: Wallet storage, transaction history, address book

#### Architecture Decision Records (ADR)
**Use for**: Documenting why architectural choices were made
**Best for**: Maintaining context over time

## Diagramming Tools & Formats

### Recommended Tools

#### 1. Mermaid (Preferred - Markdown-based)
```mermaid
graph TD
    A[UI Layer] --> B[Application Layer]
    B --> C[Domain Layer]
    C --> D[Infrastructure Layer]
```

**Advantages**:
- Text-based (version control friendly)
- Renders in markdown viewers
- No external tools needed
- Easy to update

#### 2. PlantUML
```plantuml
@startuml
package "UI Layer" {
  [MainWindow]
  [SendDialog]
  [ReceiveDialog]
}
@enduml
```

**Advantages**:
- Comprehensive diagram types
- ASCII-art style
- Good for complex diagrams

#### 3. Draw.io / Diagrams.net
**Advantages**:
- Visual editor
- Export to multiple formats
- Good for presentations

#### 4. C4 Model
**Advantages**:
- Hierarchical (Context → Container → Component → Code)
- Standard notation
- Scales well

### 3. Cryptocurrency Wallet Specific Patterns

#### Multi-Chain Architecture
```
┌─────────────────────────────────────────┐
│           Wallet Application            │
├─────────────────────────────────────────┤
│  Chain Abstraction Layer               │
│  ┌────────┐ ┌────────┐ ┌────────┐     │
│  │Bitcoin │ │Ethereum│ │ Solana │     │
│  │Adapter │ │Adapter │ │Adapter │     │
│  └────────┘ └────────┘ └────────┘     │
└─────────────────────────────────────────┘
```

#### Key Management Hierarchy
```
Master Seed (BIP39)
    │
    ├─ Master Private Key (BIP32)
    │   │
    │   ├─ Purpose (44' - BIP44)
    │   │   │
    │   │   ├─ Coin Type (0' - Bitcoin, 60' - Ethereum)
    │   │   │   │
    │   │   │   ├─ Account (0', 1', 2'...)
    │   │   │   │   │
    │   │   │   │   ├─ Chain (0 - External, 1 - Change)
    │   │   │   │   │   │
    │   │   │   │   │   ├─ Index (0, 1, 2...)
```

## System Design Templates

### 1. Complete Wallet Architecture (C4 - Context Level)

```mermaid
graph TB
    User[User]
    Wallet[Cryptocurrency Wallet Application]
    BlockchainNode[Blockchain Node/RPC]
    PriceAPI[Price Feed API]
    Explorer[Block Explorer]
    
    User -->|Uses| Wallet
    Wallet -->|Queries/Broadcasts| BlockchainNode
    Wallet -->|Fetches Prices| PriceAPI
    Wallet -->|Verifies Transactions| Explorer
```

### 2. Container Diagram - Internal Structure

```mermaid
graph TB
    subgraph "Cryptocurrency Wallet"
        UI[Qt UI Layer<br/>QML/Widgets]
        App[Application Services<br/>C++]
        Domain[Domain Logic<br/>Wallet/Transaction]
        Crypto[Cryptography Module<br/>OpenSSL/libsodium]
        Storage[Storage Layer<br/>SQLite/LevelDB]
        Network[Network Layer<br/>Qt Network]
    end
    
    UI --> App
    App --> Domain
    Domain --> Crypto
    Domain --> Storage
    App --> Network
    Network -->|RPC| External[Blockchain Nodes]
```

### 3. Component Diagram - Detailed Modules

```mermaid
graph TB
    subgraph "UI Layer"
        MainWindow[MainWindow]
        SendWidget[SendWidget]
        ReceiveWidget[ReceiveWidget]
        HistoryWidget[HistoryWidget]
        SettingsWidget[SettingsWidget]
    end
    
    subgraph "Application Layer"
        WalletService[WalletService]
        TransactionService[TransactionService]
        AddressService[AddressService]
        SyncService[SyncService]
    end
    
    subgraph "Domain Layer"
        Wallet[Wallet]
        Transaction[Transaction]
        Address[Address]
        Key[PrivateKey/PublicKey]
        UTXO[UTXO Set]
    end
    
    subgraph "Infrastructure"
        BlockchainClient[BlockchainClient]
        WalletRepository[WalletRepository]
        TransactionRepository[TransactionRepository]
        KeyStore[Encrypted KeyStore]
    end
    
    MainWindow --> WalletService
    SendWidget --> TransactionService
    ReceiveWidget --> AddressService
    
    WalletService --> Wallet
    TransactionService --> Transaction
    AddressService --> Address
    
    Wallet --> KeyStore
    Transaction --> BlockchainClient
    Wallet --> WalletRepository
```

### 4. Class Diagram - Core Domain Model

```mermaid
classDiagram
    class Wallet {
        -string id
        -string name
        -WalletType type
        -vector~Address~ addresses
        -HDKey masterKey
        +createAddress() Address
        +getBalance() Amount
        +getTransactions() vector~Transaction~
        +sign(Transaction) Signature
    }
    
    class HDWallet {
        -Mnemonic seedPhrase
        -DerivationPath path
        +deriveKey(uint32_t index) PrivateKey
        +derivePath(string path) PrivateKey
    }
    
    class Transaction {
        -string txid
        -vector~TxInput~ inputs
        -vector~TxOutput~ outputs
        -Amount fee
        -TransactionState state
        +sign(PrivateKey) void
        +broadcast() bool
        +verify() bool
    }
    
    class Address {
        -string address
        -PublicKey pubKey
        -AddressType type
        -uint32_t index
        +verify() bool
        +toString() string
    }
    
    class PrivateKey {
        -SecureBytes keyData
        +sign(bytes data) Signature
        +getPublicKey() PublicKey
        +toWIF() string
    }
    
    class PublicKey {
        -bytes keyData
        +verify(bytes data, Signature) bool
        +toAddress(AddressType) Address
    }
    
    Wallet "1" --> "*" Address
    Wallet "1" --> "*" Transaction
    HDWallet --|> Wallet
    Address --> PublicKey
    PrivateKey --> PublicKey
    Transaction --> Address
```

### 5. Sequence Diagram - Transaction Creation Flow

```mermaid
sequenceDiagram
    participant User
    participant UI
    participant TransactionService
    participant Wallet
    participant KeyStore
    participant BlockchainClient
    
    User->>UI: Enter recipient & amount
    UI->>TransactionService: createTransaction(recipient, amount)
    TransactionService->>Wallet: getUTXOs()
    Wallet-->>TransactionService: Available UTXOs
    
    TransactionService->>TransactionService: selectCoins(amount, fee)
    TransactionService->>TransactionService: buildTransaction()
    
    TransactionService->>UI: requestPassword()
    UI->>User: Show password dialog
    User->>UI: Enter password
    UI->>TransactionService: password
    
    TransactionService->>KeyStore: unlockKey(password)
    KeyStore-->>TransactionService: PrivateKey
    
    TransactionService->>Wallet: signTransaction(tx, key)
    Wallet-->>TransactionService: SignedTransaction
    
    TransactionService->>BlockchainClient: broadcastTransaction(signedTx)
    BlockchainClient-->>TransactionService: txid
    
    TransactionService->>Wallet: saveTransaction(tx)
    TransactionService->>UI: Transaction sent successfully
    UI->>User: Show confirmation
```

### 6. State Diagram - Transaction Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Draft: User creates
    Draft --> Unsigned: Inputs selected
    Unsigned --> Signed: User signs
    Signed --> Broadcasting: Sent to network
    Broadcasting --> Pending: In mempool
    Broadcasting --> Failed: Network error
    Pending --> Confirmed: Included in block
    Pending --> Failed: Rejected
    Confirmed --> Finalized: 6+ confirmations
    Failed --> [*]
    Finalized --> [*]
```

### 7. Data Flow Diagram - Sensitive Data Paths

```mermaid
graph LR
    subgraph "Security Boundary"
        SeedInput[Seed Phrase Input]
        SecureMemory[Secure Memory]
        Encryption[AES Encryption]
        EncryptedStorage[(Encrypted Storage)]
    end
    
    SeedInput -->|Immediate lock| SecureMemory
    SecureMemory -->|Derive keys| KeyDerivation[Key Derivation]
    KeyDerivation -->|Encrypt| Encryption
    Encryption --> EncryptedStorage
    
    EncryptedStorage -->|Decrypt with password| Decryption[Decryption]
    Decryption -->|Load to| SecureMemory
    SecureMemory -->|Use for signing| SigningOperation[Signing]
    SigningOperation -->|Zero memory| SecureMemory
    
    style SecureMemory fill:#f9f,stroke:#333,stroke-width:4px
    style EncryptedStorage fill:#bbf,stroke:#333,stroke-width:4px
```

### 8. Deployment Diagram - Runtime Architecture

```mermaid
graph TB
    subgraph "User Device"
        subgraph "Wallet Process"
            UIThread[UI Thread<br/>Qt Event Loop]
            WorkerThread1[Worker Thread 1<br/>Network Sync]
            WorkerThread2[Worker Thread 2<br/>Transaction Processing]
            CryptoThread[Crypto Thread<br/>Key Operations]
        end
        
        FileSystem[(Encrypted Wallet File)]
        ConfigFiles[(Configuration)]
        
        UIThread -.->|Qt Signals| WorkerThread1
        UIThread -.->|Qt Signals| WorkerThread2
        WorkerThread2 -.->|Qt Signals| CryptoThread
        
        CryptoThread -->|Read/Write| FileSystem
        UIThread -->|Read/Write| ConfigFiles
    end
    
    subgraph "Network"
        BlockchainNode1[Blockchain Node 1]
        BlockchainNode2[Blockchain Node 2]
        BlockchainNode3[Blockchain Node 3]
    end
    
    WorkerThread1 -->|TLS/RPC| BlockchainNode1
    WorkerThread1 -->|TLS/RPC| BlockchainNode2
    WorkerThread1 -->|TLS/RPC| BlockchainNode3
```

### 9. Entity-Relationship Diagram - Database Schema

```mermaid
erDiagram
    WALLET ||--o{ ADDRESS : has
    WALLET ||--o{ TRANSACTION : contains
    WALLET {
        string id PK
        string name
        string type
        timestamp created_at
        blob encrypted_seed
        string derivation_path
    }
    
    ADDRESS ||--o{ TRANSACTION_OUTPUT : receives
    ADDRESS {
        string address PK
        string wallet_id FK
        string public_key
        int derivation_index
        string label
        timestamp created_at
    }
    
    TRANSACTION ||--o{ TRANSACTION_INPUT : has
    TRANSACTION ||--o{ TRANSACTION_OUTPUT : has
    TRANSACTION {
        string txid PK
        string wallet_id FK
        timestamp timestamp
        int confirmations
        string state
        bigint fee
        string raw_tx
    }
    
    TRANSACTION_INPUT {
        string txid FK
        int vout
        string prev_txid
        int prev_vout
        string script_sig
        bigint amount
    }
    
    TRANSACTION_OUTPUT {
        string txid FK
        int vout
        string address FK
        bigint amount
        string script_pubkey
        bool spent
    }
    
    CONTACT ||--o{ ADDRESS : "linked to"
    CONTACT {
        string id PK
        string name
        string email
        string notes
    }
```

## CMake Project Structure Diagram

```
cryptocurrency-wallet/
│
├── CMakeLists.txt                 # Root build configuration
│
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   │
│   ├── ui/                        # Qt UI Layer
│   │   ├── CMakeLists.txt
│   │   ├── mainwindow.h/cpp
│   │   ├── dialogs/
│   │   └── widgets/
│   │
│   ├── application/               # Application Services
│   │   ├── CMakeLists.txt
│   │   ├── walletservice.h/cpp
│   │   ├── transactionservice.h/cpp
│   │   └── syncservice.h/cpp
│   │
│   ├── domain/                    # Core Domain Logic
│   │   ├── CMakeLists.txt
│   │   ├── wallet.h/cpp
│   │   ├── transaction.h/cpp
│   │   ├── address.h/cpp
│   │   └── crypto/
│   │       ├── privatekey.h/cpp
│   │       └── hdkey.h/cpp
│   │
│   └── infrastructure/            # External Integrations
│       ├── CMakeLists.txt
│       ├── blockchain/
│       │   ├── bitcoinclient.h/cpp
│       │   └── ethereumclient.h/cpp
│       ├── storage/
│       │   ├── walletrepository.h/cpp
│       │   └── database.h/cpp
│       └── network/
│           └── rpcclient.h/cpp
│
├── include/                       # Public headers
│   └── wallet/
│
├── tests/                         # Unit & Integration tests
│   ├── CMakeLists.txt
│   ├── unit/
│   └── integration/
│
├── external/                      # Third-party dependencies
│   ├── CMakeLists.txt
│   ├── openssl/
│   ├── libsodium/
│   └── secp256k1/
│
├── resources/                     # Qt resources
│   ├── qml/
│   ├── images/
│   └── resources.qrc
│
└── docs/                         # Documentation
    ├── architecture/
    ├── api/
    └── diagrams/
```

## Qt-Specific Patterns

### Qt Signal/Slot Architecture

```mermaid
graph LR
    subgraph "UI Thread"
        MainWindow[MainWindow]
        SendDialog[SendDialog]
    end
    
    subgraph "Worker Thread"
        WalletService[WalletService]
        BlockchainSync[BlockchainSync]
    end
    
    MainWindow -->|signal: sendRequested| WalletService
    WalletService -->|signal: transactionSent| MainWindow
    WalletService -->|signal: balanceChanged| MainWindow
    
    BlockchainSync -->|signal: syncProgress| MainWindow
    BlockchainSync -->|signal: newTransaction| WalletService
    
    style MainWindow fill:#e1f5ff
    style WalletService fill:#fff5e1
```

### Qt Model/View Pattern

```mermaid
classDiagram
    class QAbstractItemModel {
        <<interface>>
        +rowCount() int
        +data(index, role) QVariant
        +setData(index, value, role) bool
    }
    
    class TransactionHistoryModel {
        -vector~Transaction~ transactions
        +rowCount() int
        +data(index, role) QVariant
        +addTransaction(tx) void
        +updateTransaction(tx) void
    }
    
    class AddressBookModel {
        -vector~Contact~ contacts
        +rowCount() int
        +data(index, role) QVariant
    }
    
    class QTableView {
        +setModel(model) void
        +selectionChanged() signal
    }
    
    QAbstractItemModel <|-- TransactionHistoryModel
    QAbstractItemModel <|-- AddressBookModel
    QTableView --> QAbstractItemModel
```

## Architecture Decision Records (ADR)

### Template for ADR

```markdown
# ADR-001: Use Qt Signals/Slots for Cross-Thread Communication

## Status
Accepted

## Context
The wallet needs to perform blockchain synchronization in background threads
while updating the UI in real-time without blocking.

## Decision
Use Qt's signals/slots mechanism with Qt::QueuedConnection for cross-thread
communication instead of manual thread synchronization.

## Consequences
### Positive
- Type-safe communication
- Automatic thread safety with queued connections
- Loose coupling between components
- Built-in Qt integration

### Negative
- Performance overhead compared to direct calls
- Debugging can be more complex
- Must ensure proper object lifetime management

## Alternatives Considered
1. Manual mutex/condition variable (rejected - error-prone)
2. Message queue pattern (rejected - reinventing Qt's wheel)
3. Callback functions (rejected - not thread-safe)
```

## Diagramming Best Practices

### 1. Keep Diagrams Focused
- One diagram = One concern
- Don't try to show everything in a single diagram
- Create multiple views of the same system

### 2. Use Consistent Notation
```
[Component]           - Square brackets for components
(Actor)              - Parentheses for actors
{Process}            - Curly braces for processes
((Database))         - Double parentheses for storage
-->                  - Solid arrow for direct calls
-.->                 - Dashed arrow for async/events
```

### 3. Layer Your Architecture
```
High Level (Context) → Medium Level (Container) → Low Level (Component) → Code
```

### 4. Update Diagrams During Development
- **On New Feature**: Update affected component diagrams
- **On Refactoring**: Update class and sequence diagrams
- **On Dependency Change**: Update deployment diagrams
- **On Architecture Decision**: Create ADR

### 5. Version Control Your Diagrams
- Store as text files (Mermaid, PlantUML) in Git
- Keep diagrams in `/docs/architecture/`
- Link diagrams in README and technical docs

## Common Crypto Wallet Flows to Diagram

### 1. Wallet Creation Flow
```mermaid
sequenceDiagram
    actor User
    participant UI
    participant WalletService
    participant CryptoLib
    participant Storage
    
    User->>UI: Create New Wallet
    UI->>WalletService: createWallet()
    WalletService->>CryptoLib: generateMnemonic(256)
    CryptoLib-->>WalletService: 24-word mnemonic
    WalletService->>UI: Display mnemonic
    UI->>User: Show 24 words + warning
    User->>UI: Confirm backup
    UI->>User: Re-enter random words
    User->>UI: Enter random words
    UI->>WalletService: verifyMnemonic()
    WalletService->>UI: Request password
    UI->>User: Password dialog
    User->>UI: Enter password (2x)
    UI->>WalletService: password
    WalletService->>CryptoLib: encryptMnemonic(mnemonic, password)
    CryptoLib-->>WalletService: encrypted_data
    WalletService->>Storage: saveWallet(encrypted_data)
    Storage-->>WalletService: wallet_id
    WalletService->>UI: Wallet created successfully
```

### 2. Blockchain Sync Architecture
```mermaid
graph TB
    subgraph "Sync Service"
        SyncManager[Sync Manager]
        BlockDownloader[Block Downloader]
        TxScanner[Transaction Scanner]
        UTXOManager[UTXO Manager]
    end
    
    subgraph "Storage"
        BlockDB[(Block Headers)]
        TxDB[(Transactions)]
        UTXODB[(UTXO Set)]
    end
    
    SyncManager -->|Orchestrates| BlockDownloader
    SyncManager -->|Orchestrates| TxScanner
    
    BlockDownloader -->|Fetches| BlockchainNode[Blockchain Node]
    BlockDownloader -->|Stores| BlockDB
    
    TxScanner -->|Reads| BlockDB
    TxScanner -->|Filters by addresses| WalletAddresses[Wallet Addresses]
    TxScanner -->|Saves| TxDB
    
    TxScanner -->|Updates| UTXOManager
    UTXOManager -->|Maintains| UTXODB
    
    SyncManager -.->|Progress events| UI[UI Layer]
```

### 3. Multi-Signature Wallet Structure
```mermaid
graph TB
    MultiSigWallet[Multi-Signature Wallet<br/>2-of-3]
    
    Signer1[Signer 1<br/>PrivateKey 1]
    Signer2[Signer 2<br/>PrivateKey 2]
    Signer3[Signer 3<br/>PrivateKey 3]
    
    RedeemScript[Redeem Script<br/>2 PubKey1 PubKey2 PubKey3 3 CHECKMULTISIG]
    
    MultiSigWallet --> RedeemScript
    RedeemScript -.->|Requires| Signer1
    RedeemScript -.->|Requires| Signer2
    RedeemScript -.->|Requires| Signer3
    
    Transaction[Transaction]
    Transaction -->|Needs 2 signatures| Sig1[Signature 1]
    Transaction -->|Needs 2 signatures| Sig2[Signature 2]
    
    Signer1 -->|Can provide| Sig1
    Signer2 -->|Can provide| Sig2
    Signer3 -->|Can provide| Sig2
    
    style MultiSigWallet fill:#f96
    style RedeemScript fill:#9cf
```

## Integration with Development Process

### During Design Phase
1. **System Context Diagram** - Overall system boundaries
2. **Component Diagram** - Major modules and dependencies
3. **Data Flow Diagram** - Sensitive data paths
4. **Deployment Diagram** - Runtime environment

### During Development Phase
1. **Class Diagrams** - Before implementing new features
2. **Sequence Diagrams** - For complex interactions
3. **State Diagrams** - For stateful components
4. **Update ADRs** - Document architectural decisions

### During Review Phase
1. **Verify diagrams match code**
2. **Update outdated diagrams**
3. **Create missing documentation**

### During Maintenance Phase
1. **Keep diagrams current**
2. **Document workarounds in ADRs**
3. **Create bug-specific sequence diagrams**

## Tools for Auto-Generation

### Generate from Code
```bash
# Doxygen with GraphViz for class diagrams
doxygen Doxyfile

# CMake dependency graph
cmake --graphviz=dependencies.dot ..
dot -Tpng dependencies.dot -o dependencies.png

# Include-what-you-use for dependency analysis
include-what-you-use -Xiwyu --verbose=7 src/*.cpp
```

### Generate from CMake
```cmake
# In CMakeLists.txt
option(BUILD_DEPENDENCY_GRAPH "Generate dependency graph" OFF)

if(BUILD_DEPENDENCY_GRAPH)
    add_custom_target(dependency_graph
        COMMAND ${CMAKE_COMMAND} 
            --graphviz=dependencies.dot 
            ${CMAKE_BINARY_DIR}
        COMMAND dot -Tpng dependencies.dot -o dependencies.png
    )
endif()
```

## Documentation Structure

```
docs/
├── architecture/
│   ├── 00-overview.md           # High-level context
│   ├── 01-components.md         # Component diagram + descriptions
│   ├── 02-data-flow.md          # Data flow diagrams
│   ├── 03-security.md           # Security architecture
│   ├── diagrams/
│   │   ├── component.mmd        # Mermaid source files
│   │   ├── sequence-send-tx.mmd
│   │   └── ...
│   └── adr/                     # Architecture Decision Records
│       ├── 001-use-qt-signals.md
│       ├── 002-sqlite-storage.md
│       └── ...
│
├── api/
│   └── service-interfaces.md
│
└── development/
    ├── setup.md
    └── contributing.md
```

## Quick Reference Commands

### Generate Mermaid Diagrams
```bash
# Using mmdc (mermaid CLI)
mmdc -i diagram.mmd -o diagram.png

# Using Docker
docker run --rm -v $(pwd):/data minlag/mermaid-cli \
    -i /data/diagram.mmd -o /data/diagram.png
```

### Generate PlantUML Diagrams
```bash
# Using PlantUML CLI
java -jar plantuml.jar diagram.puml

# Generate all diagrams in directory
java -jar plantuml.jar "docs/diagrams/*.puml"
```

### Watch for Changes
```bash
# Auto-regenerate on file change
while inotifywait -e modify docs/diagrams/*.mmd; do
    mmdc -i docs/diagrams/*.mmd
done
```

## Checklist for System Understanding

### Architecture Documentation
- [ ] System context diagram created
- [ ] Component diagram showing all modules
- [ ] Data flow diagram for sensitive data
- [ ] Deployment diagram for runtime environment
- [ ] CMake dependency graph generated

### Core Features Documented
- [ ] Wallet creation sequence diagram
- [ ] Transaction creation sequence diagram
- [ ] Blockchain sync architecture
- [ ] Key management class diagram
- [ ] Database ERD created

### Design Decisions Recorded
- [ ] ADR for architectural patterns used
- [ ] ADR for dependency choices
- [ ] ADR for storage decisions
- [ ] ADR for threading model
- [ ] ADR for error handling strategy

### Diagrams Are Current
- [ ] Diagrams match current codebase
- [ ] Outdated diagrams removed or archived
- [ ] Diagrams stored in version control
- [ ] Diagrams referenced in README
- [ ] Diagram source files maintained (not just images)

### Team Alignment
- [ ] New team members can understand system from diagrams
- [ ] Diagrams used in code reviews
- [ ] Diagrams updated during feature development
- [ ] Diagrams help security audits

## Tips for Effective Diagrams

1. **Start Simple, Add Detail**: Begin with high-level, add layers of detail as needed
2. **One Purpose Per Diagram**: Don't try to show everything
3. **Use Text Files**: Mermaid/PlantUML > Images (for version control)
4. **Keep Them Current**: Outdated diagrams are worse than no diagrams
5. **Link to Code**: Reference specific files/classes in diagram annotations
6. **Automate Where Possible**: Generate from code when feasible
7. **Review in PRs**: Update diagrams as part of feature development
8. **Make Them Accessible**: Store in /docs, render in README
9. **Use Standard Notation**: C4, UML, or consistent custom notation
10. **Explain the "Why"**: Use ADRs to document reasoning behind structure

## Resources

- C4 Model: https://c4model.com/
- Mermaid Documentation: https://mermaid.js.org/
- PlantUML Guide: https://plantuml.com/
- UML Notation: https://www.uml.org/
- Software Architecture Documentation: arc42.org

---

## Usage Instructions

1. **Start with Context**: Create a system context diagram showing the wallet and external systems
2. **Drill Down**: Create container and component diagrams for internal structure
3. **Document Flows**: Create sequence diagrams for key user journeys
4. **Model Data**: Create class diagrams for core domain objects and ERDs for persistence
5. **Record Decisions**: Write ADRs as you make architectural choices
6. **Keep Current**: Update diagrams as code evolves
7. **Review Regularly**: Ensure diagrams still represent reality
8. **Use in Onboarding**: Walk new developers through the diagrams
9. **Reference in Code**: Add comments linking to relevant diagrams
10. **Integrate with CI**: Auto-generate what you can, validate what you can't

Remember: Diagrams are a communication tool. They should help you and your team understand the system better. If a diagram doesn't serve that purpose, simplify or remove it.
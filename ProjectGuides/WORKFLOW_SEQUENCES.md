# CriptoGualet Workflow Sequence Diagrams

## Table of Contents
1. [User Registration](#user-registration)
2. [User Login](#user-login)
3. [Wallet Creation](#wallet-creation)
4. [Balance Fetching](#balance-fetching)
5. [Send Transaction](#send-transaction)
6. [Receive Transaction](#receive-transaction)
7. [Seed Phrase Display](#seed-phrase-display)
8. [Address Generation](#address-generation)
9. [Theme Switching](#theme-switching)
10. [Top Cryptocurrencies View](#top-cryptocurrencies-view)

---

## User Registration

Complete workflow for new user registration with multi-chain wallet creation.

```mermaid
sequenceDiagram
    actor User
    participant QtLoginUI
    participant Auth
    participant UserRepo
    participant Crypto
    participant WalletRepo
    participant DPAPI
    participant Database
    participant QtSeedDialog

    User->>QtLoginUI: Enter username, email, password
    QtLoginUI->>QtLoginUI: Validate input<br/>(username length, password strength)

    QtLoginUI->>Auth: RegisterUserWithMnemonic(username, email, password)

    Auth->>UserRepo: getUserByUsername(username)
    UserRepo-->>Auth: null (user doesn't exist)

    Auth->>Crypto: GenerateEntropy(128 bits)
    Crypto-->>Auth: 16-byte random entropy

    Auth->>Crypto: LoadBIP39Wordlist()
    Crypto-->>Auth: 2048 English words

    Auth->>Crypto: MnemonicFromEntropy(entropy, wordlist)
    Crypto->>Crypto: Calculate checksum<br/>Convert to 11-bit indices
    Crypto-->>Auth: 12-word mnemonic

    Auth->>Crypto: BIP39_SeedFromMnemonic(mnemonic, passphrase="")
    Crypto->>Crypto: PBKDF2-HMAC-SHA512<br/>(2048 iterations)
    Crypto-->>Auth: 64-byte seed

    Auth->>Crypto: DerivePasswordHash(password)
    Crypto->>Crypto: PBKDF2-HMAC-SHA256<br/>(100,000 iterations)
    Crypto-->>Auth: password_hash, salt

    Auth->>UserRepo: createUser(username, email, password_hash, salt)
    UserRepo->>Database: INSERT INTO users(...)
    Database-->>UserRepo: user_id
    UserRepo-->>Auth: user_id

    Auth->>DPAPI: StoreUserSeedDPAPI(username, seed)
    DPAPI->>DPAPI: Encrypt with Windows DPAPI
    DPAPI->>DPAPI: Write to seed_vault/{username}.bin
    DPAPI-->>Auth: Success

    Auth->>WalletRepo: storeEncryptedSeed(user_id, password, mnemonic)
    WalletRepo->>Crypto: DeriveDBEncryptionKey(password, salt)
    Crypto-->>WalletRepo: 32-byte AES key
    WalletRepo->>Crypto: EncryptDBData(key, mnemonic)
    Crypto-->>WalletRepo: encrypted_blob
    WalletRepo->>Database: INSERT INTO encrypted_seeds(...)
    Database-->>WalletRepo: Success
    WalletRepo-->>Auth: Success

    Auth->>Crypto: BIP32_MasterKeyFromSeed(seed)
    Crypto->>Crypto: HMAC-SHA512("Bitcoin seed", seed)
    Crypto-->>Auth: master_extended_key

    par Bitcoin Wallet Creation
        Auth->>Crypto: BIP44_GetAddress(master, m/44'/1'/0'/0/0, testnet=true)
        Crypto->>Crypto: Derive child keys<br/>Generate P2PKH address
        Crypto-->>Auth: btc_address

        Auth->>WalletRepo: createWallet(user_id, "Bitcoin Wallet", "bitcoin", "m/44'/1'/0'")
        WalletRepo->>Database: INSERT INTO wallets(...)
        Database-->>WalletRepo: wallet_id
        WalletRepo-->>Auth: btc_wallet_id

        Auth->>WalletRepo: createAddress(btc_wallet_id, user_id, btc_address, 0, false)
        WalletRepo->>Database: INSERT INTO addresses(...)
        Database-->>WalletRepo: Success
        WalletRepo-->>Auth: Success
    and Ethereum Wallet Creation
        Auth->>Crypto: BIP44_GetEthereumAddress(master, m/44'/60'/0'/0/0)
        Crypto->>Crypto: Derive child keys<br/>Keccak256(pubkey)
        Crypto-->>Auth: eth_address

        Auth->>WalletRepo: createWallet(user_id, "Ethereum Wallet", "ethereum", "m/44'/60'/0'")
        WalletRepo->>Database: INSERT INTO wallets(...)
        Database-->>WalletRepo: wallet_id
        WalletRepo-->>Auth: eth_wallet_id

        Auth->>WalletRepo: createAddress(eth_wallet_id, user_id, eth_address, 0, false)
        WalletRepo->>Database: INSERT INTO addresses(...)
        Database-->>WalletRepo: Success
        WalletRepo-->>Auth: Success
    end

    Auth-->>QtLoginUI: RegistrationResult(success, mnemonic)

    QtLoginUI->>QtSeedDialog: Display seed phrase dialog
    QtSeedDialog->>User: Show 12-word mnemonic<br/>with QR code<br/>"Write down and store securely"
    User->>QtSeedDialog: Confirm backup checkbox
    QtSeedDialog-->>QtLoginUI: User confirmed

    QtLoginUI->>QtLoginUI: Navigate to WalletUI
    QtLoginUI-->>User: Registration complete
```

**Key Steps:**
1. User input validation
2. Entropy generation (128 bits = 12 words)
3. BIP39 mnemonic creation
4. Seed derivation via PBKDF2
5. Password hashing with salt
6. User creation in database
7. Dual seed storage (DPAPI + Database)
8. BIP32 master key derivation
9. Multi-chain wallet creation (Bitcoin, Ethereum)
10. Address generation and storage
11. Seed phrase display to user

---

## User Login

Authentication flow with rate limiting and seed retrieval.

```mermaid
sequenceDiagram
    actor User
    participant QtLoginUI
    participant Auth
    participant UserRepo
    participant Crypto
    participant DPAPI
    participant Database
    participant QtWalletUI

    User->>QtLoginUI: Enter username, password
    QtLoginUI->>Auth: LoginUser(username, password)

    Auth->>UserRepo: getUserByUsername(username)
    UserRepo->>Database: SELECT * FROM users WHERE username=?
    Database-->>UserRepo: User record
    UserRepo-->>Auth: User object

    Auth->>Auth: Check lockout<br/>(lockout_until > now?)
    alt Account Locked
        Auth-->>QtLoginUI: Error: Account locked
        QtLoginUI-->>User: "Too many failed attempts.<br/>Try again in 10 minutes."
    end

    Auth->>Crypto: DerivePasswordHash(password, user.salt)
    Crypto->>Crypto: PBKDF2-HMAC-SHA256<br/>(100,000 iterations, same salt)
    Crypto-->>Auth: computed_hash

    Auth->>Auth: Compare hashes<br/>(constant-time)

    alt Password Incorrect
        Auth->>UserRepo: incrementFailedLoginAttempts(user_id)
        UserRepo->>Database: UPDATE users SET failed_login_attempts=+1
        Database-->>UserRepo: Success

        alt Failed Attempts >= 5
            Auth->>UserRepo: setAccountLockout(user_id, now + 10 minutes)
            UserRepo->>Database: UPDATE users SET lockout_until=?
            Database-->>UserRepo: Success
            Auth-->>QtLoginUI: Error: Account locked
            QtLoginUI-->>User: "Account locked for 10 minutes"
        else Failed Attempts < 5
            Auth-->>QtLoginUI: Error: Invalid password
            QtLoginUI-->>User: "Invalid username or password"
        end
    end

    Auth->>UserRepo: resetFailedLoginAttempts(user_id)
    UserRepo->>Database: UPDATE users SET failed_login_attempts=0
    Database-->>UserRepo: Success

    Auth->>DPAPI: RetrieveUserSeedDPAPI(username)
    DPAPI->>DPAPI: Read seed_vault/{username}.bin
    DPAPI->>DPAPI: DPAPI_Unprotect(encrypted_bytes)

    alt DPAPI Success
        DPAPI-->>Auth: 64-byte seed
    else DPAPI Failure (machine/user changed)
        Auth->>WalletRepo: retrieveDecryptedSeed(user_id, password)
        WalletRepo->>Database: SELECT encrypted_seed FROM encrypted_seeds WHERE user_id=?
        Database-->>WalletRepo: encrypted_blob, salt
        WalletRepo->>Crypto: DeriveDBEncryptionKey(password, salt)
        Crypto-->>WalletRepo: 32-byte AES key
        WalletRepo->>Crypto: DecryptDBData(key, encrypted_blob)
        Crypto-->>WalletRepo: mnemonic words
        WalletRepo->>Crypto: BIP39_SeedFromMnemonic(mnemonic, "")
        Crypto-->>WalletRepo: 64-byte seed
        WalletRepo-->>Auth: seed
    end

    Auth->>Crypto: BIP32_MasterKeyFromSeed(seed)
    Crypto-->>Auth: master_key

    Auth->>Crypto: BIP44_GetAddress(master, m/44'/1'/0'/0/0, testnet=true)
    Crypto-->>Auth: btc_address

    Auth->>Crypto: BIP44_GetEthereumAddress(master, m/44'/60'/0'/0/0)
    Crypto-->>Auth: eth_address

    Auth-->>QtLoginUI: LoginResult(success, user_id, btc_address, eth_address)

    QtLoginUI->>QtWalletUI: setUserInfo(username, btc_address, eth_address)
    QtWalletUI->>QtWalletUI: Initialize wallet clients<br/>SimpleWallet, EthereumWallet
    QtWalletUI->>QtWalletUI: Fetch initial balances
    QtWalletUI-->>User: Display wallet dashboard
```

**Security Features:**
- Rate limiting (5 attempts, 10-minute lockout)
- Constant-time password comparison
- Dual seed retrieval (DPAPI primary, database fallback)
- Password never logged or stored in plaintext

---

## Wallet Creation

Multi-chain wallet initialization during registration.

```mermaid
sequenceDiagram
    participant Auth
    participant Crypto
    participant WalletRepo
    participant Database

    Auth->>Crypto: BIP32_MasterKeyFromSeed(seed)
    Crypto->>Crypto: HMAC-SHA512("Bitcoin seed", seed)
    Crypto-->>Auth: master_extended_key {<br/>  key: 32 bytes,<br/>  chainCode: 32 bytes,<br/>  depth: 0<br/>}

    loop For each blockchain (Bitcoin, Ethereum, ...)
        alt Bitcoin Testnet
            Auth->>Crypto: BIP44_GetAddress(master, 0, false, 0, testnet=true)
            Crypto->>Crypto: DerivePath(master, "m/44'/1'/0'/0/0")
            Crypto->>Crypto: Generate P2PKH address<br/>(testnet version byte 0x6F)
            Crypto-->>Auth: btc_address (starts with 'm' or 'n')

            Auth->>WalletRepo: createWallet(user_id, "Bitcoin Wallet", "bitcoin", "m/44'/1'/0'")
            WalletRepo->>Database: INSERT INTO wallets(...)<br/>VALUES(user_id, "Bitcoin Wallet", "bitcoin", "m/44'/1'/0'")
            Database-->>WalletRepo: wallet_id
            WalletRepo-->>Auth: wallet_id

            Auth->>WalletRepo: createAddress(wallet_id, user_id, btc_address, 0, false)
            WalletRepo->>Database: INSERT INTO addresses(...)<br/>VALUES(wallet_id, user_id, btc_address, 0, 0)
            Database-->>WalletRepo: address_id
            WalletRepo-->>Auth: address_id
        else Ethereum Mainnet
            Auth->>Crypto: BIP44_GetEthereumAddress(master, 0, false, 0)
            Crypto->>Crypto: DerivePath(master, "m/44'/60'/0'/0/0")
            Crypto->>Crypto: Generate Ethereum address<br/>(Keccak256(pubkey), take last 20 bytes)
            Crypto-->>Auth: eth_address (0x...)

            Auth->>WalletRepo: createWallet(user_id, "Ethereum Wallet", "ethereum", "m/44'/60'/0'")
            WalletRepo->>Database: INSERT INTO wallets(...)<br/>VALUES(user_id, "Ethereum Wallet", "ethereum", "m/44'/60'/0'")
            Database-->>WalletRepo: wallet_id
            WalletRepo-->>Auth: wallet_id

            Auth->>WalletRepo: createAddress(wallet_id, user_id, eth_address, 0, false)
            WalletRepo->>Database: INSERT INTO addresses(...)<br/>VALUES(wallet_id, user_id, eth_address, 0, 0)
            Database-->>WalletRepo: address_id
            WalletRepo-->>Auth: address_id
        end
    end

    Auth-->>Auth: Wallets created for all chains
```

**BIP44 Paths Used:**
- Bitcoin Testnet: `m/44'/1'/0'/0/0`
- Bitcoin Mainnet: `m/44'/0'/0'/0/0`
- Ethereum: `m/44'/60'/0'/0/0`
- Future chains follow BIP44 standard coin types

---

## Balance Fetching

Periodic balance updates from blockchain APIs.

```mermaid
sequenceDiagram
    participant Timer
    participant QtWalletUI
    participant SimpleWallet
    participant BlockCypher
    participant EthereumWallet
    participant Etherscan
    participant PriceService
    participant QtCard

    Timer->>QtWalletUI: Timer fired (every 30 seconds)

    QtWalletUI->>QtWalletUI: Check mode (mockMode?)
    alt Mock Mode
        QtWalletUI-->>QtCard: Display mock balance
    end

    par Bitcoin Balance
        QtWalletUI->>SimpleWallet: GetBalance(btc_address)
        SimpleWallet->>BlockCypher: GET /addrs/{address}/balance
        BlockCypher-->>SimpleWallet: {"balance": satoshis,<br/>"unconfirmed_balance": satoshis}
        SimpleWallet->>SimpleWallet: ConvertSatoshisToBTC(satoshis)
        SimpleWallet-->>QtWalletUI: balance_btc

        QtWalletUI->>SimpleWallet: GetTransactionHistory(btc_address, limit=10)
        SimpleWallet->>BlockCypher: GET /addrs/{address}/txs?limit=10
        BlockCypher-->>SimpleWallet: [transactions array]
        SimpleWallet-->>QtWalletUI: recent_transactions

        QtWalletUI->>QtCard: m_bitcoinWalletCard->setBalance("{balance_btc} BTC")
        QtWalletUI->>QtCard: m_bitcoinWalletCard->setTransactionHistory(html)
    and Ethereum Balance
        QtWalletUI->>EthereumWallet: GetBalance(eth_address)
        EthereumWallet->>Etherscan: GET /api?module=account&action=balance&address={address}
        Etherscan-->>EthereumWallet: {"result": "wei_balance"}
        EthereumWallet->>EthereumWallet: WeiToEth(wei_balance)
        EthereumWallet-->>QtWalletUI: balance_eth

        QtWalletUI->>EthereumWallet: GetTransactionHistory(eth_address, limit=10)
        EthereumWallet->>Etherscan: GET /api?module=account&action=txlist&address={address}&limit=10
        Etherscan-->>EthereumWallet: {"result": [transactions]}
        EthereumWallet-->>QtWalletUI: recent_transactions

        QtWalletUI->>QtCard: m_ethereumWalletCard->setBalance("{balance_eth} ETH")
        QtWalletUI->>QtCard: m_ethereumWalletCard->setTransactionHistory(html)
    and Price Fetching
        QtWalletUI->>PriceService: GetBitcoinPrice()
        PriceService->>PriceService: HTTP GET CoinGecko/CMC API
        PriceService-->>QtWalletUI: btc_price_usd

        QtWalletUI->>PriceService: GetEthereumPrice()
        PriceService->>PriceService: HTTP GET CoinGecko/CMC API
        PriceService-->>QtWalletUI: eth_price_usd
    end

    QtWalletUI->>QtWalletUI: Calculate total USD<br/>= (btc_balance * btc_price) + (eth_balance * eth_price)
    QtWalletUI->>QtWalletUI: Update m_balanceLabel<br/>("${total_usd} USD")
    QtWalletUI-->>Timer: Balance refresh complete
```

**Refresh Frequency:**
- Auto-refresh every 30 seconds
- Manual refresh on user request
- Exponential backoff on API errors

---

## Send Transaction

Bitcoin transaction creation and broadcasting workflow.

```mermaid
sequenceDiagram
    actor User
    participant QtWalletUI
    participant QtSendDialog
    participant WalletRepo
    participant Crypto
    participant SimpleWallet
    participant BlockCypher
    participant TxRepo
    participant Database

    User->>QtWalletUI: Click "Send" on Bitcoin card
    QtWalletUI->>QtSendDialog: new QtSendDialog(currentBalance, btcPrice)
    QtSendDialog->>User: Display send dialog<br/>(recipient, amount, fee)

    User->>QtSendDialog: Enter recipient address
    QtSendDialog->>QtSendDialog: Validate address<br/>(checksum, format)

    User->>QtSendDialog: Enter amount (BTC or USD)
    QtSendDialog->>QtSendDialog: Convert USD to BTC if needed<br/>Check sufficient balance

    User->>QtSendDialog: Enter password
    User->>QtSendDialog: Click "Send"

    QtSendDialog->>QtSendDialog: getTransactionData()
    QtSendDialog-->>QtWalletUI: TransactionData(recipient, amount, fee, password)

    QtWalletUI->>WalletRepo: retrieveDecryptedSeed(user_id, password)
    WalletRepo->>Database: SELECT encrypted_seed FROM encrypted_seeds WHERE user_id=?
    Database-->>WalletRepo: encrypted_blob, salt
    WalletRepo->>Crypto: DeriveDBEncryptionKey(password, salt)
    Crypto-->>WalletRepo: 32-byte AES key
    WalletRepo->>Crypto: DecryptDBData(key, encrypted_blob)
    Crypto-->>WalletRepo: mnemonic words

    alt Invalid Password
        WalletRepo-->>QtWalletUI: Error: Decryption failed
        QtWalletUI-->>User: "Invalid password"
    end

    WalletRepo-->>QtWalletUI: mnemonic

    QtWalletUI->>Crypto: BIP39_SeedFromMnemonic(mnemonic, "")
    Crypto-->>QtWalletUI: 64-byte seed

    QtWalletUI->>Crypto: BIP32_MasterKeyFromSeed(seed)
    Crypto-->>QtWalletUI: master_key

    QtWalletUI->>Crypto: BIP44_DeriveAddressKey(master, 0, false, 0, testnet=true)
    Crypto-->>QtWalletUI: address_key {<br/>  private_key: 32 bytes,<br/>  public_key: 33 bytes<br/>}

    QtWalletUI->>SimpleWallet: SendFunds(<br/>  from_addresses=[current_address],<br/>  to_address=recipient,<br/>  amount_satoshis=amount,<br/>  private_keys={address: privkey},<br/>  fee_satoshis=fee<br/>)

    SimpleWallet->>BlockCypher: CreateTransaction({<br/>  inputs: [from_addresses],<br/>  outputs: [{address: to, value: amount}]<br/>})

    BlockCypher->>BlockCypher: Find UTXOs for from_addresses<br/>Build unsigned transaction
    BlockCypher-->>SimpleWallet: {<br/>  tosign: [hashes to sign],<br/>  signatures: [],<br/>  pubkeys: []<br/>}

    loop For each tosign hash
        SimpleWallet->>Crypto: SignHash(private_key, hash)
        Crypto->>Crypto: secp256k1 ECDSA signing
        Crypto-->>SimpleWallet: DER-encoded signature
    end

    SimpleWallet->>SimpleWallet: Add signatures and pubkeys to transaction

    SimpleWallet->>BlockCypher: SendSignedTransaction({<br/>  tx: signed_transaction,<br/>  tosign: [hashes],<br/>  signatures: [sigs],<br/>  pubkeys: [pubs]<br/>})

    BlockCypher->>BlockCypher: Verify signatures<br/>Broadcast to Bitcoin network
    BlockCypher-->>SimpleWallet: {<br/>  tx_hash: "abc123...",<br/>  received: timestamp<br/>}

    SimpleWallet-->>QtWalletUI: SendTransactionResult(success, tx_hash)

    QtWalletUI->>TxRepo: createTransaction(<br/>  wallet_id, tx_hash,<br/>  from_address, to_address,<br/>  amount_satoshis, "send"<br/>)
    TxRepo->>Database: INSERT INTO transactions(...)
    Database-->>TxRepo: transaction_id
    TxRepo-->>QtWalletUI: Success

    QtWalletUI->>Crypto: SecureWipeMemory(private_key)
    Crypto->>Crypto: Overwrite memory with zeros

    QtWalletUI->>QtWalletUI: fetchRealBalance()<br/>(update balance)
    QtWalletUI-->>User: "Transaction sent!<br/>Hash: {tx_hash}"
```

**Security Measures:**
- Password re-entry required for each transaction
- Private key derived on-demand
- Private key wiped from memory immediately after signing
- Transaction confirmation dialog with all details

---

## Receive Transaction

QR code display for receiving cryptocurrency.

```mermaid
sequenceDiagram
    actor User
    participant QtWalletUI
    participant QtCard
    participant QRGenerator
    participant QtReceiveDialog

    User->>QtCard: Click "Receive" on Bitcoin card
    QtCard->>QtWalletUI: receiveRequested signal

    QtWalletUI->>QtWalletUI: Get current_address

    QtWalletUI->>QRGenerator: GenerateQRCode(current_address)
    QRGenerator->>QRGenerator: Check if libqrencode available

    alt libqrencode available
        QRGenerator->>QRGenerator: QRcode_encodeString(address, ...)
        QRGenerator->>QRGenerator: Convert to QRData format
        QRGenerator-->>QtWalletUI: QRData(width, height, data)
    else libqrencode not available
        QRGenerator->>QRGenerator: Generate fallback pattern
        QRGenerator-->>QtWalletUI: QRData(fallback pattern)
    end

    QtWalletUI->>QtReceiveDialog: new QtReceiveDialog(address, qrData)
    QtReceiveDialog->>QtReceiveDialog: Create QImage from QRData
    QtReceiveDialog->>QtReceiveDialog: Display QR code + address

    QtReceiveDialog-->>User: Show receive dialog<br/>"Scan QR or copy address"

    User->>QtReceiveDialog: Click "Copy Address"
    QtReceiveDialog->>QtReceiveDialog: QClipboard::setText(address)
    QtReceiveDialog-->>User: "Address copied to clipboard"

    User->>QtReceiveDialog: Close dialog
    QtReceiveDialog-->>QtWalletUI: Dialog closed
```

**Use Cases:**
- Display QR code for mobile wallet scanning
- Copy address to clipboard
- Share address via other applications

---

## Seed Phrase Display

Secure display of BIP39 mnemonic with QR code.

```mermaid
sequenceDiagram
    actor User
    participant Auth
    participant QtSeedDialog
    participant QRGenerator

    Auth->>QtSeedDialog: new QtSeedDisplayDialog(mnemonic)
    QtSeedDialog->>QtSeedDialog: Create grid of word labels<br/>(12 words in 3 columns)

    QtSeedDialog->>QRGenerator: GenerateQRCode(mnemonic_string)
    QRGenerator-->>QtSeedDialog: QRData

    QtSeedDialog->>QtSeedDialog: Create QImage from QRData
    QtSeedDialog->>QtSeedDialog: Display QR code

    QtSeedDialog->>QtSeedDialog: Add warning text<br/>"Write down and store securely.<br/>Anyone with this phrase can access your funds."

    QtSeedDialog->>QtSeedDialog: Add backup confirmation checkbox<br/>"I have written down my seed phrase"

    QtSeedDialog-->>User: Display seed dialog

    User->>User: Write down seed phrase<br/>(or scan QR code)

    User->>QtSeedDialog: Check backup confirmation
    QtSeedDialog->>QtSeedDialog: Enable "Close" button

    User->>QtSeedDialog: Click "Close"
    QtSeedDialog-->>Auth: User confirmed backup
```

**Security Notes:**
- Seed phrase displayed only during registration
- Cannot be viewed again without password
- Clear warning about seed phrase importance
- QR code for easy backup (optional)

---

## Address Generation

BIP44 hierarchical address derivation.

```mermaid
sequenceDiagram
    participant QtWalletUI
    participant Crypto
    participant secp256k1

    QtWalletUI->>Crypto: BIP44_GetAddress(master, account=0, change=false, index=0, testnet=true)

    Crypto->>Crypto: Build derivation path<br/>"m/44'/1'/0'/0/0"

    Crypto->>Crypto: Parse path segments<br/>["44'", "1'", "0'", "0", "0"]

    Crypto->>Crypto: current_key = master_key

    loop For each path segment
        Crypto->>Crypto: Extract index<br/>(add 0x80000000 if hardened)

        alt Hardened derivation (44', 1', 0')
            Crypto->>Crypto: data = 0x00 || parent_privkey || index
        else Non-hardened (0, 0)
            Crypto->>Crypto: data = parent_pubkey || index
        end

        Crypto->>Crypto: hmac_output = HMAC-SHA512(parent_chaincode, data)
        Crypto->>Crypto: tweak = hmac_output[0:32]<br/>new_chaincode = hmac_output[32:64]

        Crypto->>secp256k1: privkey_tweak_add(parent_privkey, tweak)
        secp256k1-->>Crypto: child_privkey

        Crypto->>Crypto: current_key = {<br/>  key: child_privkey,<br/>  chaincode: new_chaincode,<br/>  depth: parent.depth + 1<br/>}
    end

    Crypto->>Crypto: derived_key = current_key

    Crypto->>secp256k1: pubkey_create(derived_key.privkey)
    secp256k1-->>Crypto: 33-byte compressed pubkey

    Crypto->>Crypto: SHA256(pubkey)
    Crypto->>Crypto: RIPEMD160(sha256_hash)
    Crypto->>Crypto: Add version byte (0x6F for testnet)
    Crypto->>Crypto: Calculate checksum (double SHA256)
    Crypto->>Crypto: Base58Check encode

    Crypto-->>QtWalletUI: Bitcoin address (e.g., "mz8KW1p4...")
```

**Derivation Properties:**
- Deterministic: Same seed always produces same addresses
- Hierarchical: Parent key derives infinite children
- Secure: Hardened derivation prevents sibling key compromise

---

## Theme Switching

Dynamic theme application without restart.

```mermaid
sequenceDiagram
    actor User
    participant QtSettingsUI
    participant QtThemeManager
    participant QtWalletUI
    participant QtCard
    participant QtSidebar

    User->>QtSettingsUI: Select theme from dropdown<br/>(Crypto Dark, Crypto Light, etc.)

    QtSettingsUI->>QtThemeManager: setTheme(ThemeType)

    QtThemeManager->>QtThemeManager: Switch theme type<br/>Update color palette

    alt Crypto Dark
        QtThemeManager->>QtThemeManager: setupCryptoDark()<br/>background=#1a1a2e<br/>primary=#0f3460<br/>accent=#e94560
    else Crypto Light
        QtThemeManager->>QtThemeManager: setupCryptoLight()<br/>background=#f0f0f0<br/>primary=#4A90E2<br/>accent=#e94560
    else Standard Dark
        QtThemeManager->>QtThemeManager: setupStandardDark()<br/>background=#2b2b2b<br/>primary=#3c3c3c<br/>accent=#007acc
    else Standard Light
        QtThemeManager->>QtThemeManager: setupStandardLight()<br/>background=#ffffff<br/>primary=#f3f3f3<br/>accent=#0078d4
    end

    QtThemeManager->>QtThemeManager: Emit themeChanged signal

    par Update all components
        QtThemeManager->>QtWalletUI: themeChanged signal
        QtWalletUI->>QtWalletUI: applyTheme()<br/>Update styleSheet
        QtWalletUI->>QtCard: m_bitcoinWalletCard->applyTheme()
        QtCard->>QtCard: Update colors from ThemeManager
        QtWalletUI->>QtCard: m_ethereumWalletCard->applyTheme()
        QtCard->>QtCard: Update colors from ThemeManager
        QtWalletUI->>QtSidebar: m_sidebar->applyTheme()
        QtSidebar->>QtSidebar: Update colors from ThemeManager
    end

    QtSettingsUI->>QtSettingsUI: applyTheme()<br/>Update own styleSheet

    QtSettingsUI-->>User: Theme applied instantly
```

**Theme Components Updated:**
- Background colors
- Primary/secondary surface colors
- Text colors (primary, subtitle)
- Accent colors
- Button styles
- Card borders and shadows

---

## Top Cryptocurrencies View

Market data fetching and display.

```mermaid
sequenceDiagram
    actor User
    participant QtSidebar
    participant QtWalletUI
    participant QtTopCryptosPage
    participant PriceService
    participant CoinGecko

    User->>QtSidebar: Click "Top Cryptocurrencies"
    QtSidebar->>QtWalletUI: navigateToPage("top_cryptos")

    QtWalletUI->>QtTopCryptosPage: showPage()

    QtTopCryptosPage->>PriceService: GetTopCryptocurrencies(limit=100)

    PriceService->>CoinGecko: GET /api/v3/coins/markets?<br/>vs_currency=usd&<br/>order=market_cap_desc&<br/>per_page=100

    CoinGecko-->>PriceService: [<br/>  {id, symbol, name, current_price, market_cap, price_change_24h, ...},<br/>  ...<br/>]

    PriceService->>PriceService: Parse JSON response<br/>Filter out stablecoins (optional)

    PriceService-->>QtTopCryptosPage: CryptoDataList

    QtTopCryptosPage->>QtTopCryptosPage: Clear existing table rows

    loop For each cryptocurrency
        QtTopCryptosPage->>QtTopCryptosPage: Create table row<br/>(rank, icon, name, price, 24h change, market cap)

        alt Price increased
            QtTopCryptosPage->>QtTopCryptosPage: Color: green
        else Price decreased
            QtTopCryptosPage->>QtTopCryptosPage: Color: red
        end

        QtTopCryptosPage->>QtTopCryptosPage: Add row to table
    end

    QtTopCryptosPage-->>User: Display top 100 cryptocurrencies
```

**Features:**
- Real-time price data
- 24-hour price change indicators
- Market cap sorting
- Color-coded price changes (green/red)
- Automatic refresh (configurable interval)

---

**Document Version:** 1.0
**Last Updated:** 2025-11-16
**Author:** Claude (Architecture Documentation Expert)
**Project:** CriptoGualet - Cross-Platform Cryptocurrency Wallet

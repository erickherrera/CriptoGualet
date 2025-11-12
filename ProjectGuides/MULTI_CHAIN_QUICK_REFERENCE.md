# Multi-Chain Wallet - Quick Reference Guide

## BIP44 Derivation Paths

### Standard Format
```
m / purpose' / coin_type' / account' / change / address_index
```

### Active Chains

| Chain | Path | Address Example | Coin Type |
|-------|------|----------------|-----------|
| Bitcoin Testnet | `m/44'/1'/0'/0/0` | `mz8KW1p4xyDJpBheqcR8h...` | 1 |
| Ethereum Mainnet | `m/44'/60'/0'/0/0` | `0xAbC1234567890aBcDe...` | 60 |
| Ethereum Sepolia | `m/44'/60'/0'/0/0` | `0x1234567890aBcDeF12...` | 60 |

### Planned Chains

| Chain | Path | Coin Type | Same Address as ETH? |
|-------|------|-----------|---------------------|
| Bitcoin Mainnet | `m/44'/0'/0'/0/0` | 0 | No (P2PKH) |
| Arbitrum | `m/44'/60'/0'/0/0` | 60 | ✅ Yes |
| Optimism | `m/44'/60'/0'/0/0` | 60 | ✅ Yes |
| Base | `m/44'/60'/0'/0/0` | 60 | ✅ Yes |
| BNB Chain | `m/44'/714'/0'/0/0` | 714 | No (EVM but different path) |
| Polygon | `m/44'/966'/0'/0/0` | 966 | No (EVM but different path) |
| Avalanche | `m/44'/9000'/0'/0/0` | 9000 | No (EVM but different path) |

---

## Component Quick Reference

### File Locations

```
backend/
├── core/
│   ├── Crypto.h/cpp         # BIP39, BIP32, BIP44, address generation
│   ├── Auth.cpp             # Seed generation, wallet creation
│   └── WalletAPI.h/cpp      # SimpleWallet (BTC), EthereumWallet (ETH)
├── blockchain/
│   ├── BlockCypher.h/cpp    # Bitcoin API integration
│   └── EthereumService.h/cpp # Ethereum API integration
└── database/
    └── DatabaseManager.h/cpp # SQLCipher encrypted storage

frontend/qt/
└── QtWalletUI.h/cpp         # Multi-chain wallet UI
```

---

## Key Functions Cheat Sheet

### Seed Generation (Registration)
```cpp
// 1. Generate entropy
std::vector<uint8_t> entropy;
Crypto::GenerateEntropy(128, entropy); // 128 bits = 12 words

// 2. Convert to mnemonic
std::vector<std::string> mnemonic;
Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

// 3. Derive seed
std::array<uint8_t, 64> seed;
Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed); // Empty passphrase
```

### Master Key Derivation
```cpp
Crypto::BIP32ExtendedKey masterKey;
Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);
```

### Bitcoin Address Generation
```cpp
std::string btcAddress;
Crypto::BIP44_GetAddress(masterKey,
    0,      // account
    false,  // change (false=receiving, true=change)
    0,      // address_index
    btcAddress,
    true    // testnet
);
// Result: mz8KW1p4xyDJpBheqcR8hVD9FwAv9wEgZ6
```

### Ethereum Address Generation
```cpp
std::string ethAddress;
Crypto::BIP44_GetEthereumAddress(masterKey,
    0,      // account
    false,  // change
    0,      // address_index
    ethAddress
);
// Result: 0xAbC1234567890aBcDeF1234567890AbCdEf1234
```

### Balance Fetching

**Bitcoin:**
```cpp
SimpleWallet btcWallet("btc/test3");
uint64_t balanceSatoshis = btcWallet.GetBalance(btcAddress);
double balanceBTC = btcWallet.ConvertSatoshisToBTC(balanceSatoshis);
```

**Ethereum:**
```cpp
EthereumWallet ethWallet("mainnet");
double balanceETH = ethWallet.GetBalance(ethAddress);
```

---

## Address Format Quick Reference

### Bitcoin (P2PKH)
- **Mainnet:** Starts with `1` or `3`
- **Testnet:** Starts with `m` or `n`
- **Format:** Base58Check encoding
- **Length:** 26-35 characters
- **Example:** `mz8KW1p4xyDJpBheqcR8hVD9FwAv9wEgZ6`

### Ethereum (All EVM Chains)
- **Format:** Hexadecimal with `0x` prefix
- **Length:** 42 characters (0x + 40 hex digits)
- **Case:** Case-insensitive (but EIP-55 checksum available)
- **Example:** `0xAbC1234567890aBcDeF1234567890AbCdEf1234`
- **Same address on:** Ethereum, Arbitrum, Optimism, Base, Avalanche (all EVM)

---

## Security Checklist

### Registration
- ✅ Generate 128-bit entropy using Windows BCrypt
- ✅ Create 12-word BIP39 mnemonic
- ✅ Display seed in QtSeedDisplayDialog (user confirms backup)
- ✅ Encrypt seed with DPAPI (machine-bound)
- ✅ Encrypt seed in SQLCipher database (password-bound)
- ✅ Derive addresses for Bitcoin and Ethereum
- ✅ Store wallet records in database

### Login
- ✅ Rate limiting (5 attempts, 10-minute lockout)
- ✅ Retrieve encrypted seed from DPAPI or database
- ✅ Decrypt seed with user password
- ✅ Re-derive addresses from seed
- ✅ Verify addresses match stored addresses

### Transaction Signing
- ✅ User enters password to decrypt seed
- ✅ Derive private key on-demand (not cached)
- ✅ Sign transaction with ECDSA (secp256k1)
- ✅ Broadcast to blockchain
- ✅ Wipe private key from memory immediately

---

## API Endpoints

### BlockCypher (Bitcoin Testnet)
- **Base URL:** `https://api.blockcypher.com/v1/btc/test3`
- **Balance:** `GET /addrs/{address}/balance`
- **Transactions:** `GET /addrs/{address}/txs`
- **Create TX:** `POST /txs/new`
- **Send TX:** `POST /txs/send`
- **Rate Limit:** 200 requests/hour (free tier)

### Etherscan (Ethereum Mainnet)
- **Base URL:** `https://api.etherscan.io/api`
- **Balance:** `GET ?module=account&action=balance&address={address}`
- **Transactions:** `GET ?module=account&action=txlist&address={address}`
- **Gas Price:** `GET ?module=gastracker&action=gasoracle`
- **Rate Limit:** 5 calls/second, 100,000 calls/day (free tier)

---

## Unit Conversions

### Bitcoin
```cpp
// Satoshis to BTC
double btc = satoshis / 100000000.0;

// BTC to Satoshis
uint64_t satoshis = btc * 100000000.0;
```

### Ethereum
```cpp
// Wei to ETH
double eth = std::stod(wei_str) / 1e18;

// ETH to Wei
std::string wei = std::to_string(eth * 1e18);

// Gwei to Wei (for gas prices)
std::string wei = std::to_string(gwei * 1e9);
```

---

## Database Schema (Quick)

### users
- `id` (INTEGER PRIMARY KEY)
- `username` (TEXT UNIQUE)
- `password_hash` (TEXT) - PBKDF2-HMAC-SHA256

### wallets
- `id` (INTEGER PRIMARY KEY)
- `user_id` (INTEGER FK)
- `wallet_name` (TEXT)
- `wallet_type` (TEXT) - "bitcoin", "ethereum", etc.
- `derivation_path` (TEXT) - "m/44'/1'/0'", "m/44'/60'/0'"

### addresses
- `id` (INTEGER PRIMARY KEY)
- `wallet_id` (INTEGER FK)
- `address` (TEXT UNIQUE)
- `address_index` (INTEGER)
- `balance_satoshis` (INTEGER)

### encrypted_seeds
- `id` (INTEGER PRIMARY KEY)
- `user_id` (INTEGER UNIQUE FK)
- `encrypted_seed` (BLOB) - AES-GCM encrypted mnemonic
- `encryption_salt` (BLOB)
- `verification_hash` (BLOB)

---

## Common Operations

### Add New User
```cpp
Auth::RegisterUserWithMnemonic(username, email, password, outMnemonic);
// Creates user, generates seed, derives addresses, stores encrypted seed
```

### Login User
```cpp
Auth::LoginUser(username, password);
// Authenticates, retrieves seed, re-derives addresses, populates g_users cache
```

### Send Bitcoin Transaction
```cpp
// 1. Get transaction details from UI
QtSendDialog dialog(currentBalance, btcPrice, parent);
auto txData = dialog.getTransactionData();

// 2. Decrypt seed with password
auto seedResult = walletRepo->retrieveDecryptedSeed(userId, password);

// 3. Derive private key
BIP39_SeedFromMnemonic(seedResult.data, "", seed);
BIP32_MasterKeyFromSeed(seed, masterKey);
BIP44_DeriveAddressKey(masterKey, 0, false, 0, addressKey, true);

// 4. Send transaction
wallet->SendFunds(fromAddresses, toAddress, amountSatoshis, privateKeys, feeSatoshis);
```

### Fetch Balance
```cpp
// Fetch Bitcoin balance
uint64_t satoshis = wallet->GetBalance(btcAddress);
double btc = wallet->ConvertSatoshisToBTC(satoshis);

// Fetch Ethereum balance
double eth = ethWallet->GetBalance(ethAddress);

// Calculate USD total
double totalUSD = (btc * btcPrice) + (eth * ethPrice);
```

---

## Debugging Tips

### Enable Debug Logging
```cpp
// Auth.cpp
#define ENABLE_AUTH_DEBUG_LOGGING 1
// Writes to: registration_debug.log
```

### Check Seed Storage
```
DPAPI file: seed_vault/{username}.bin
Database: wallet.db → encrypted_seeds table
```

### Verify Derivation
```cpp
// Bitcoin: m/44'/1'/0'/0/0
// Check: Address starts with 'm' or 'n' (testnet)

// Ethereum: m/44'/60'/0'/0/0
// Check: Address starts with '0x', 42 chars total
```

### Test Network Connectivity
```cpp
// Bitcoin
SimpleWallet wallet("btc/test3");
std::string networkInfo = wallet.GetNetworkInfo();
// Should output: "Connected to BlockCypher API - Network: btc/test3"

// Ethereum
EthereumWallet ethWallet("mainnet");
std::string networkInfo = ethWallet.GetNetworkInfo();
// Should output: "Connected to Etherscan API - Network: mainnet"
```

---

## Quick Architecture Diagram

```
┌─────────────────────────────────────────────────┐
│                Frontend (Qt UI)                 │
│  ┌────────────────┐    ┌─────────────────────┐ │
│  │ Bitcoin Card   │    │ Ethereum Card       │ │
│  │ (₿ BTC)        │    │ (Ξ ETH)             │ │
│  └────────────────┘    └─────────────────────┘ │
└──────────────┬───────────────────┬──────────────┘
               │                   │
┌──────────────▼───────┐ ┌─────────▼──────────────┐
│  SimpleWallet (BTC)  │ │ EthereumWallet (ETH)   │
│  WalletAPI Layer     │ │ WalletAPI Layer        │
└──────────────┬───────┘ └─────────┬──────────────┘
               │                   │
┌──────────────▼───────┐ ┌─────────▼──────────────┐
│ BlockCypher Service  │ │ EthereumService        │
│ (Bitcoin API)        │ │ (Etherscan API)        │
└──────────────────────┘ └────────────────────────┘
               │                   │
               └─────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │   Crypto Module  │
                │ ┌──────────────┐ │
                │ │ BIP39 Seed   │ │
                │ │ BIP32 HD Key │ │
                │ │ BIP44 Paths  │ │
                │ │ secp256k1    │ │
                │ │ Keccak256    │ │
                │ └──────────────┘ │
                └──────────────────┘
                         │
        ┌────────────────┴────────────────┐
        │                                 │
┌───────▼────────┐              ┌─────────▼────────┐
│ DPAPI Storage  │              │ SQLCipher DB     │
│ seed_vault/    │              │ encrypted_seeds  │
│ {user}.bin     │              │ table            │
└────────────────┘              └──────────────────┘
```

---

## Testing Checklist

### Unit Tests
- [ ] `test_bip39.cpp` - Mnemonic generation and validation
- [ ] `test_secure_seed.cpp` - Seed encryption/decryption
- [ ] `test_database.cpp` - Database operations
- [ ] `test_wallet_repository.cpp` - Wallet CRUD
- [ ] `test_blockcypher_api.cpp` - Bitcoin API integration

### Integration Tests
- [ ] Register new user → Verify seed stored in DPAPI + DB
- [ ] Login user → Verify addresses re-derived correctly
- [ ] Fetch Bitcoin balance → Verify BlockCypher API call
- [ ] Fetch Ethereum balance → Verify Etherscan API call
- [ ] Send Bitcoin transaction → Verify signature and broadcast

### Manual Testing
1. **Registration:**
   - Register new user
   - Verify seed phrase displayed in dialog
   - Check seed_vault/{username}.bin created
   - Check encrypted_seeds table has entry

2. **Login:**
   - Login with correct password
   - Verify Bitcoin and Ethereum addresses displayed
   - Check balances fetched from APIs

3. **Balance Updates:**
   - Wait 30 seconds
   - Verify balances refresh automatically

4. **Send Transaction (Bitcoin Testnet):**
   - Get test BTC from faucet
   - Send small amount to another testnet address
   - Verify transaction appears in BlockCypher explorer

---

## Troubleshooting

### Issue: "Failed to load BIP39 wordlist"
**Solution:** Check that `assets/bip39/english.txt` exists. Set environment variable:
```bash
set BIP39_WORDLIST=C:\path\to\english.txt
```

### Issue: "Database not initialized"
**Solution:** Ensure `InitializeDatabase()` is called before using repositories.

### Issue: "DPAPI decryption failed"
**Cause:** Seed encrypted on different user account or machine.
**Solution:** Use database decryption fallback (requires user password).

### Issue: "Invalid Bitcoin address"
**Check:** Testnet addresses start with 'm' or 'n', mainnet with '1' or '3'.

### Issue: "Ethereum balance shows 0"
**Check:**
1. Etherscan API token configured?
2. Network set to "mainnet" (not "sepolia")?
3. Address has ETH? Check on etherscan.io

---

**Last Updated:** 2025-11-10
**Quick Reference Version:** 1.0

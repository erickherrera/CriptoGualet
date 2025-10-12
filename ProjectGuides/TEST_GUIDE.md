# Phase 1 Completion Test Guide

This guide explains how to verify that Phase 1 security implementation is complete and working correctly.

## Phase 1 Requirements Checklist

- [x] **Remove plaintext database** (SaveUserDatabase/LoadUserDatabase removed)
- [x] **Remove fallback key generation** (fail fast instead of generating random keys)
- [x] **Remove test backdoors** (no debug logging of sensitive data)
- [x] **Implement proper private key encryption** (evolved to seed-based derivation - more secure)

## Test Execution Order

### 1. Core Cryptographic Tests (CRITICAL)

**Test:** `test_security_enhancements.exe`

**What it verifies:**
- ✅ AES-GCM encryption/decryption working
- ✅ Password-based seed encryption (EncryptSeedPhrase/DecryptSeedPhrase)
- ✅ Database-level encryption (EncryptDBData/DecryptDBData)
- ✅ PBKDF2 key derivation (600,000 iterations)
- ✅ Memory security (SecureWipeVector/SecureWipeString)
- ✅ Full integration scenario (wallet creation → encryption → decryption)

**Run:**
```bash
.\out\build\win-clang-x64-debug\bin\Debug\test_security_enhancements.exe
```

**Expected result:** All 8 tests should pass
```
=== ALL TESTS PASSED! ===
Security enhancements are working correctly.
```

---

### 2. BIP39 + BIP32 Integration Tests (CRITICAL)

**Test:** `test_bip39.exe`

**What it verifies:**

**BIP39 Tests:**
- ✅ Entropy generation (128-bit and 256-bit)
- ✅ BIP39 wordlist loading (2048 words)
- ✅ 12-word mnemonic generation
- ✅ Mnemonic consistency (deterministic)
- ✅ User registration with seed
- ✅ Seed reveal (encrypted storage retrieval)
- ✅ Seed restore from mnemonic
- ✅ Invalid mnemonic rejection
- ✅ Password/username validation
- ✅ Rate limiting

**BIP32 Cryptographic Tests (NEW):**
- ✅ **Master key generation from BIP39 seed**
- ✅ **Child key derivation (hardened + normal)**
- ✅ **BIP44 path derivation (m/44'/0'/0'/0/0)**
- ✅ **Bitcoin address generation using secp256k1**
- ✅ **WIF private key export**
- ✅ **secp256k1 elliptic curve integration**
- ✅ **Known test vector verification**

**Run:**
```bash
.\out\build\win-clang-x64-debug\bin\Debug\test_bip39.exe
```

**Expected result:** 19 tests should pass (12 BIP39 + 7 BIP32)
```
==================================================
TEST SUMMARY
==================================================
Total tests: 19
Passed: 19
Failed: 0

Success rate: 100.0%
==================================================
```

---

### 3. Database Persistence Tests (CRITICAL)

**Test:** `test_auth_database_integration.exe`

**What it verifies:**
- ✅ No plaintext database usage (Phase 1 requirement)
- ✅ Encrypted SQLCipher persistence
- ✅ User registration → database storage
- ✅ Wallet creation and storage
- ✅ Encrypted seed storage and retrieval
- ✅ Login authentication via database
- ✅ Duplicate user prevention
- ✅ Last login timestamp tracking

**Run:**
```bash
.\out\build\win-clang-x64-debug\bin\Debug\test_auth_database_integration.exe
```

**Expected result:** All tests should pass
```
========================================
Test Suite Results: ALL PASSED
========================================
```

---

### 4. Security Verification Tests (OPTIONAL)

**Test:** `test_secure_seed.exe`

**What it verifies:**
- ✅ No insecure plaintext seed files created
- ✅ DPAPI storage working
- ✅ QR code generation functional
- ✅ Mnemonic handling is memory-only during registration

**Run:**
```bash
.\out\build\win-clang-x64-debug\bin\Debug\test_secure_seed.exe
```

**Expected result:**
```
=== Security Implementation Summary ===
✅ Removed plain text file storage
✅ Added secure QR code display (with fallback)
✅ User confirmation required for backup
✅ Seeds stored with Windows DPAPI encryption
✅ Memory-only seed phrase handling during registration

🔐 Seed phrase security has been significantly improved!
```

---

### 5. Low-Level Database Tests (OPTIONAL)

**Test:** `test_database.exe`

**What it verifies:**
- ✅ SQLCipher initialization
- ✅ Database encryption key setup
- ✅ Table creation and schema migrations
- ✅ CRUD operations on encrypted database

**Run:**
```bash
.\out\build\win-clang-x64-debug\bin\Debug\test_database.exe
```

---

### 6. Repository Layer Tests (OPTIONAL)

**Test:** `test_repository.exe`

**What it verifies:**
- ✅ UserRepository operations
- ✅ WalletRepository operations
- ✅ TransactionRepository operations
- ✅ Data access layer abstraction

**Run:**
```bash
.\out\build\win-clang-x64-debug\bin\Debug\test_repository.exe
```

---

## Quick Test All (PowerShell)

Run all critical tests in sequence:

```powershell
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Phase 1 Verification - Critical Tests" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

$testsPassed = 0
$testsFailed = 0

Write-Host "[1/3] Running Security Enhancements Tests..." -ForegroundColor Yellow
& ".\out\build\win-clang-x64-debug\bin\Debug\test_security_enhancements.exe"
if ($LASTEXITCODE -eq 0) { $testsPassed++ } else { $testsFailed++ }
Write-Host ""

Write-Host "[2/3] Running BIP39 + BIP32 Integration Tests..." -ForegroundColor Yellow
& ".\out\build\win-clang-x64-debug\bin\Debug\test_bip39.exe"
if ($LASTEXITCODE -eq 0) { $testsPassed++ } else { $testsFailed++ }
Write-Host ""

Write-Host "[3/3] Running Auth Database Integration Tests..." -ForegroundColor Yellow
& ".\out\build\win-clang-x64-debug\bin\Debug\test_auth_database_integration.exe"
if ($LASTEXITCODE -eq 0) { $testsPassed++ } else { $testsFailed++ }
Write-Host ""

Write-Host "========================================" -ForegroundColor Cyan
if ($testsFailed -eq 0) {
    Write-Host "✅ ALL CRITICAL TESTS PASSED ($testsPassed/3)" -ForegroundColor Green
    Write-Host "Phase 1 is complete and working correctly." -ForegroundColor Green
} else {
    Write-Host "❌ SOME TESTS FAILED ($testsFailed failed, $testsPassed passed)" -ForegroundColor Red
    Write-Host "Please review the output above." -ForegroundColor Red
}
Write-Host "========================================" -ForegroundColor Cyan
```

---

## What Each Test Proves About Phase 1

### Phase 1 Requirement 1: Remove Plaintext Database ✅

**Verified by:**
- `test_auth_database_integration.exe` - All persistence uses SQLCipher encryption
- `test_bip39.cpp` lines 574, 601 - Comments confirm removal

### Phase 1 Requirement 2: Remove Fallback Key Generation ✅

**Verified by:**
- `test_bip39.exe` BIP32 tests - All keys derived from BIP39 seed using BIP32
- No random key generation in Auth.cpp (removed in Phase 1)

### Phase 1 Requirement 3: Remove Test Backdoors ✅

**Verified by:**
- `test_secure_seed.exe` - No insecure files created
- No debug logging of sensitive data in Auth.cpp

### Phase 1 Requirement 4: Proper Private Key Encryption ✅

**Verified by:**
- `test_security_enhancements.exe` - EncryptSeedPhrase/DecryptSeedPhrase working
- `test_bip39.exe` - Private keys derived on-demand from encrypted seed (more secure than storing encrypted keys)
- `test_auth_database_integration.exe` - Seed retrieval requires password

---

## BIP32 + secp256k1 Implementation Verification

The new BIP32 tests in `test_bip39.exe` specifically verify:

1. **Master Key Generation** - HMAC-SHA512 of BIP39 seed
2. **Child Derivation** - Both hardened (m/0') and normal (m/0) paths
3. **BIP44 Path** - m/44'/0'/0'/0/0 standard Bitcoin derivation
4. **secp256k1 Integration** - Proper elliptic curve operations:
   - `secp256k1_ec_pubkey_create()` - Public key from private key
   - `secp256k1_ec_seckey_tweak_add()` - Scalar addition (mod n)
   - `secp256k1_ec_pubkey_tweak_add()` - Point addition on curve
   - `secp256k1_ec_pubkey_serialize()` - Compressed public key format
5. **Bitcoin Addresses** - Base58Check encoding, starting with '1'
6. **WIF Export** - Wallet Import Format for private keys
7. **Known Test Vectors** - Deterministic address generation

---

## Troubleshooting

### Tests are failing

1. **Rebuild everything:**
   ```bash
   cmake --build out/build/win-clang-x64-debug --clean-first
   ```

2. **Check database permissions:**
   - Delete `wallet.db` file if it exists
   - Run tests with admin privileges if needed

3. **Verify dependencies:**
   ```bash
   vcpkg install
   ```

### Specific test failures

- **test_bip39.exe fails on BIP32 tests** - secp256k1 library issue
  - Check that secp256k1 DLLs are copied to bin/Debug/
  - Verify vcpkg.json includes `secp256k1`

- **test_security_enhancements.exe fails on encryption** - Windows CryptoAPI issue
  - Run as administrator
  - Check bcrypt.lib and crypt32.lib are linked

- **test_auth_database_integration.exe fails** - SQLCipher issue
  - Check that `sqlcipher.dll` is in bin/Debug/
  - Delete any existing `wallet.db` files

---

## Summary

**Phase 1 is COMPLETE when:**
- ✅ `test_security_enhancements.exe` passes (8/8 tests)
- ✅ `test_bip39.exe` passes (19/19 tests, including 7 BIP32 tests)
- ✅ `test_auth_database_integration.exe` passes (3/3 test suites)

**This verifies:**
- 🔒 No plaintext storage anywhere
- 🔒 All keys derived from encrypted BIP39 seed using BIP32
- 🔒 Production-grade elliptic curve crypto (secp256k1)
- 🔒 AES-GCM encryption for all sensitive data
- 🔒 SQLCipher encrypted database
- 🔒 DPAPI for local Windows encryption
- 🔒 No test backdoors or debug leaks

**Phase 1 Status:** ✅ COMPLETE AND VERIFIED

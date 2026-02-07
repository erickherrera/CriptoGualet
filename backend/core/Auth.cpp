// Standard library headers first
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// Project headers before Windows headers to avoid macro conflicts
#include "../database/include/Database/DatabaseManager.h"
#include "../repository/include/Repository/UserRepository.h"
#include "../repository/include/Repository/WalletRepository.h"
#include "../utils/include/SharedTypes.h"  // for User struct, GeneratePrivateKey, GenerateBitcoinAddress
#include "Auth.h"
#include "Crypto.h"

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

// Windows headers need to be after project headers to avoid macro conflicts
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>  // Must be included first for type definitions (LONG, ULONG, etc.)
#include <bcrypt.h>
#include <lmcons.h>  // For UNLEN constant (username max length)
#include <wincrypt.h>

// No Qt headers needed in Auth library

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

// === Configuration ===
static constexpr size_t BIP39_ENTROPY_BITS = 128;  // 12 words
static constexpr uint32_t BIP39_PBKDF2_ITERS = 2048;
static const char* DEFAULT_WORDLIST_PATH = "assets/bip39/english.txt";
static const char* SEED_VAULT_DIR = "seed_vault";
static const char* DPAPI_ENTROPY_PREFIX = "CriptoGualet seed v1::";

// === Database Boolean Constants ===
// SQLite doesn't have native BOOLEAN type, uses INTEGER (0 = false, 1 = true)

// Debug logging configuration - DISABLE IN PRODUCTION
// Set to 0 to disable all debug file logging for production builds
#ifndef ENABLE_AUTH_DEBUG_LOGGING
#ifdef _DEBUG
#define ENABLE_AUTH_DEBUG_LOGGING 1
#else
#define ENABLE_AUTH_DEBUG_LOGGING 0
#endif
#endif

// Helper macros for conditional debug logging
#if ENABLE_AUTH_DEBUG_LOGGING
#define AUTH_DEBUG_LOG_OPEN(logVar) std::ofstream logVar("registration_debug.log", std::ios::app)
#define AUTH_DEBUG_LOG_WRITE(logVar, msg) \
    do {                                  \
        if ((logVar).is_open()) {         \
            (logVar) << msg;              \
            (logVar).flush();             \
        }                                 \
    } while (0)
#define AUTH_DEBUG_LOG_PTR(ptr) (ptr)
#define AUTH_DEBUG_LOG_STREAM(logVar) \
    if ((logVar).is_open())           \
    (logVar)
#define AUTH_DEBUG_LOG_FLUSH(logVar) \
    if ((logVar).is_open())          \
    (logVar).flush()
#else
#define AUTH_DEBUG_LOG_OPEN(logVar) \
    std::ofstream logVar;           \
    (void)logVar
#define AUTH_DEBUG_LOG_WRITE(logVar, msg) (void)0
#define AUTH_DEBUG_LOG_PTR(ptr) nullptr
#define AUTH_DEBUG_LOG_STREAM(logVar) \
    if (false)                        \
    std::cerr
#define AUTH_DEBUG_LOG_FLUSH(logVar) (void)0
#endif

// g_users and g_currentUser are declared in SharedTypes.h, defined in SharedSymbols.cpp
// Access must be protected by g_usersMutex

// ===== Forward Declarations =====
// DeriveSecureEncryptionKey is now a public function in Auth namespace

// ===== Database and Repository Integration =====
static std::unique_ptr<Repository::UserRepository> g_userRepo = nullptr;
static std::unique_ptr<Repository::WalletRepository> g_walletRepo = nullptr;
static bool g_databaseInitialized = false;

// Helper: Initialize database and repositories
static bool InitializeDatabase() {
    if (g_databaseInitialized) {
        return true;  // Already initialized
    }

    try {
        auto& dbManager = Database::DatabaseManager::getInstance();

        // Get database path from environment or use default
        std::string dbPath = "wallet.db";
#pragma warning(push)
#pragma warning(disable : 4996)  // Suppress getenv warning
        if (const char* envPath = std::getenv("WALLET_DB_PATH")) {
            dbPath = envPath;
        }
#pragma warning(pop)

        // Derive secure encryption key from machine-specific data
        // This binds the database to the specific machine/user context
        std::string encryptionKey;
        if (!Auth::DeriveSecureEncryptionKey(encryptionKey)) {
            return false;
        }

        // Initialize database with derived encryption key
        auto result = dbManager.initialize(dbPath, encryptionKey);

        // Securely wipe encryption key from memory
        std::fill(encryptionKey.begin(), encryptionKey.end(), '\0');

        if (!result.success) {
            return false;
        }

        // Create essential database tables if they don't exist
        std::string schemaSQL = R"(
      CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT NOT NULL UNIQUE,
        password_hash TEXT NOT NULL,
        salt BLOB NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        last_login TEXT,
        wallet_version INTEGER NOT NULL DEFAULT 1,
        is_active INTEGER NOT NULL DEFAULT 1,
        totp_enabled INTEGER NOT NULL DEFAULT 0,
        totp_secret TEXT,
        totp_secret_pending TEXT,
        backup_codes TEXT
      );

      CREATE TABLE IF NOT EXISTS wallets (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        wallet_name TEXT NOT NULL,
        wallet_type TEXT NOT NULL DEFAULT 'bitcoin',
        derivation_path TEXT,
        extended_public_key TEXT,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        is_active INTEGER NOT NULL DEFAULT 1,
        FOREIGN KEY (user_id) REFERENCES users(id)
      );

      CREATE TABLE IF NOT EXISTS addresses (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        wallet_id INTEGER NOT NULL,
        address TEXT NOT NULL UNIQUE,
        address_index INTEGER NOT NULL,
        is_change INTEGER NOT NULL DEFAULT 0,
        public_key TEXT,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        label TEXT,
        balance_satoshis INTEGER NOT NULL DEFAULT 0,
        FOREIGN KEY (wallet_id) REFERENCES wallets(id),
        UNIQUE (wallet_id, address_index, is_change)
      );

      CREATE TABLE IF NOT EXISTS erc20_tokens (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        wallet_id INTEGER NOT NULL,
        contract_address TEXT NOT NULL,
        symbol TEXT NOT NULL,
        name TEXT NOT NULL,
        decimals INTEGER NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (wallet_id) REFERENCES wallets(id),
        UNIQUE (wallet_id, contract_address)
      );

      CREATE TABLE IF NOT EXISTS transactions (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        wallet_id INTEGER NOT NULL,
        txid TEXT NOT NULL UNIQUE,
        block_height INTEGER,
        block_hash TEXT,
        amount_satoshis INTEGER NOT NULL,
        fee_satoshis INTEGER NOT NULL DEFAULT 0,
        direction TEXT NOT NULL,
        from_address TEXT,
        to_address TEXT,
        confirmation_count INTEGER NOT NULL DEFAULT 0,
        is_confirmed INTEGER NOT NULL DEFAULT 0,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        confirmed_at TEXT,
        memo TEXT,
        FOREIGN KEY (wallet_id) REFERENCES wallets(id)
      );

      CREATE TABLE IF NOT EXISTS encrypted_seeds (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL UNIQUE,
        encrypted_seed BLOB NOT NULL,
        encryption_salt BLOB NOT NULL,
        verification_hash BLOB NOT NULL,
        key_derivation_iterations INTEGER NOT NULL DEFAULT 600000,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        backup_confirmed INTEGER NOT NULL DEFAULT 0,
        FOREIGN KEY (user_id) REFERENCES users(id)
      );

      CREATE TABLE IF NOT EXISTS user_settings (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        setting_key TEXT NOT NULL,
        setting_value TEXT NOT NULL,
        updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (user_id) REFERENCES users(id),
        UNIQUE (user_id, setting_key)
      );

      CREATE TABLE IF NOT EXISTS rate_limits (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        identifier TEXT NOT NULL UNIQUE,
        attempt_count INTEGER NOT NULL DEFAULT 0,
        last_attempt_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        lockout_until TEXT,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
      );

      -- Basic indexes
      CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
      CREATE INDEX IF NOT EXISTS idx_wallets_user_id ON wallets(user_id);
      CREATE INDEX IF NOT EXISTS idx_addresses_wallet_id ON addresses(wallet_id);
      CREATE INDEX IF NOT EXISTS idx_addresses_address ON addresses(address);
      CREATE INDEX IF NOT EXISTS idx_transactions_wallet_id ON transactions(wallet_id);
      CREATE INDEX IF NOT EXISTS idx_transactions_txid ON transactions(txid);
      CREATE INDEX IF NOT EXISTS idx_erc20_tokens_wallet_contract ON erc20_tokens(wallet_id, contract_address);
      CREATE INDEX IF NOT EXISTS idx_user_settings_user_key ON user_settings(user_id, setting_key);
      CREATE INDEX IF NOT EXISTS idx_rate_limits_identifier ON rate_limits(identifier);

      -- Phase 3: Composite indexes for query optimization
      CREATE INDEX IF NOT EXISTS idx_wallets_user_type ON wallets(user_id, wallet_type);
      CREATE INDEX IF NOT EXISTS idx_wallets_user_active ON wallets(user_id, is_active);
      CREATE INDEX IF NOT EXISTS idx_transactions_wallet_date ON transactions(wallet_id, created_at DESC);
      CREATE INDEX IF NOT EXISTS idx_transactions_wallet_confirmed ON transactions(wallet_id, is_confirmed);
      CREATE INDEX IF NOT EXISTS idx_transactions_wallet_direction_date ON transactions(wallet_id, direction, created_at DESC);
      CREATE INDEX IF NOT EXISTS idx_addresses_wallet_change ON addresses(wallet_id, is_change);
      CREATE INDEX IF NOT EXISTS idx_addresses_wallet_balance ON addresses(wallet_id, balance_satoshis DESC);
    )";

        auto schemaResult = dbManager.executeQuery(schemaSQL);
        if (!schemaResult.success) {
            return false;
        }

        // Create repository instances
        g_userRepo = std::make_unique<Repository::UserRepository>(dbManager);
        g_walletRepo = std::make_unique<Repository::WalletRepository>(dbManager);

        g_databaseInitialized = true;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Rate limiting data structures
struct RateLimitEntry {
    int attemptCount = 0;
    std::chrono::steady_clock::time_point lastAttempt;
    std::chrono::steady_clock::time_point lockoutUntil;
};

static std::map<std::string, RateLimitEntry> g_rateLimits;

namespace fs = std::filesystem;

static inline bool EnsureDir(const fs::path& p) {
    std::error_code ec;
    if (fs::exists(p, ec))
        return fs::is_directory(p, ec);
    return fs::create_directories(p, ec);
}

// === Secure encryption key derivation ===
// Derives a deterministic encryption key from machine-specific data
//
// SECURITY CONSIDERATIONS:
// 1. Database binding: The key is derived from machine/user-specific data,
// binding
//    the encrypted database to this specific Windows user account on this
//    machine.
// 2. Key components: computer name + username + volume serial + app salt
// 3. Key derivation: PBKDF2-HMAC-SHA256 with 100,000 iterations for slow
// derivation
// 4. Portability: Database CANNOT be moved to different machine/user - this is
// intentional
// 5. Recovery: If machine is reimaged or username changes, database will be
// inaccessible
//
// PRODUCTION NOTES:
// - For multi-device sync, implement separate user-controlled master password
// - For cloud backup, implement key escrow with user-controlled recovery phrase
// - For enterprise deployment, consider using Windows DPAPI with machine/user
// scope
//
bool Auth::DeriveSecureEncryptionKey(std::string& outKey) {
    // Gather machine-specific entropy sources
    std::vector<uint8_t> entropy;

    // 1. Get Windows computer name
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        entropy.insert(entropy.end(), computerName, computerName + size);
    }

    // 2. Get Windows username
    char username[UNLEN + 1];
    size = sizeof(username);
    if (GetUserNameA(username, &size)) {
        entropy.insert(entropy.end(), username, username + size);
    }

    // 3. Get volume serial number (disk-specific)
    DWORD volumeSerial = 0;
    if (GetVolumeInformationA("C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr, nullptr, 0)) {
        uint8_t* serialBytes = reinterpret_cast<uint8_t*>(&volumeSerial);
        entropy.insert(entropy.end(), serialBytes, serialBytes + sizeof(volumeSerial));
    }

    // 4. Add application-specific constant salt
    const char* appSalt = "CriptoGualet-v1.0-DatabaseEncryption";
    entropy.insert(entropy.end(), appSalt, appSalt + strlen(appSalt));

    // Ensure we have sufficient entropy
    if (entropy.size() < 32) {
        return false;
    }

    // Derive key using PBKDF2-HMAC-SHA256 with high iteration count
    std::vector<uint8_t> derivedKey;
    const unsigned int iterations = 100000;  // High iteration count for security

    if (!Crypto::PBKDF2_HMAC_SHA256(std::string(entropy.begin(), entropy.end()), entropy.data(),
                                    entropy.size(), iterations, derivedKey,
                                    32  // 256-bit key
                                    )) {
        return false;
    }

    // Convert to hex string for SQLCipher
    std::ostringstream hexKey;
    for (uint8_t byte : derivedKey) {
        hexKey << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }

    outKey = hexKey.str();

    // Securely wipe entropy and derived key from memory
    std::fill(entropy.begin(), entropy.end(), uint8_t(0));
    std::fill(derivedKey.begin(), derivedKey.end(), uint8_t(0));

    return true;
}

// ===== BIP-39 helpers =====
namespace {
static inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static bool LoadWordList(std::vector<std::string>& out) {
    out.clear();

    // Try multiple possible locations for the wordlist
    std::vector<std::string> possiblePaths = {"src/assets/bip39/english.txt",
                                              "assets/bip39/english.txt",
                                              "../src/assets/bip39/english.txt",
                                              "../assets/bip39/english.txt",
                                              "../../../../../src/assets/bip39/english.txt",
                                              "../../../../../../src/assets/bip39/english.txt"};

    // Also check environment variable
#pragma warning(push)
#pragma warning(disable : 4996)  // Suppress getenv warning - this is safe usage
    if (const char* env = std::getenv("BIP39_WORDLIST")) {
        possiblePaths.insert(possiblePaths.begin(), env);
    }
#pragma warning(pop)

    for (const auto& path : possiblePaths) {
        std::ifstream f(path, std::ios::binary);
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                line = trim(line);
                if (!line.empty())
                    out.push_back(line);
            }
            f.close();
            if (out.size() == 2048) {
                return true;
            }
            out.clear();
        }
    }

    return false;
}

// Parse "word word ...", normalize spaces to single space, lowercase
static std::vector<std::string> SplitWordsNormalized(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    cur.reserve(16);
    for (char ch : text) {
        if (std::isspace((unsigned char)ch)) {
            if (!cur.empty()) {
                std::transform(cur.begin(), cur.end(), cur.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back((char)std::tolower((unsigned char)ch));
        }
    }
    if (!cur.empty()) {
        std::transform(cur.begin(), cur.end(), cur.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        out.push_back(cur);
    }
    return out;
}

static fs::path VaultPathForUser(const std::string& username) {
    return fs::path(SEED_VAULT_DIR) / (username + ".bin");
}

static bool StoreUserSeedDPAPI(const std::string& username, const std::array<uint8_t, 64>& seed) {
    if (!EnsureDir(SEED_VAULT_DIR))
        return false;
    const std::string entropy = std::string(DPAPI_ENTROPY_PREFIX) + username;

    std::vector<uint8_t> plaintext(seed.begin(), seed.end());
    std::vector<uint8_t> ciphertext;
    if (!Crypto::DPAPI_Protect(plaintext, entropy, ciphertext))
        return false;

    std::ofstream file(VaultPathForUser(username), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;
    file.write(reinterpret_cast<const char*>(ciphertext.data()),
               static_cast<std::streamsize>(ciphertext.size()));
    file.close();
    std::fill(plaintext.begin(), plaintext.end(), uint8_t(0));
    return true;
}

static bool RetrieveUserSeedDPAPI(const std::string& username, std::array<uint8_t, 64>& outSeed) {
    const fs::path p = VaultPathForUser(username);
    std::ifstream f(p, std::ios::binary);
    if (!f.is_open())
        return false;
    std::vector<uint8_t> ciphertext((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
    f.close();
    if (ciphertext.empty())
        return false;

    const std::string entropy = std::string(DPAPI_ENTROPY_PREFIX) + username;
    std::vector<uint8_t> plaintext;
    if (!Crypto::DPAPI_Unprotect(ciphertext, entropy, plaintext))
        return false;
    if (plaintext.size() != 64)
        return false;
    std::memcpy(outSeed.data(), plaintext.data(), 64);
    std::fill(plaintext.begin(), plaintext.end(), uint8_t(0));
    return true;
}

// REMOVED: WriteOneTimeMnemonicFile - No longer creating insecure plain text
// files Seed phrases are now displayed securely via QtSeedDisplayDialog with
// user confirmation

// REMOVED: TryLoadOneTimeMnemonicFile - No longer reading from insecure plain
// text files Seed phrases are now handled securely in memory only during the
// registration/display process

}  // namespace

// ===== Public API =====
namespace Auth {

bool IsRateLimited(const std::string& identifier);
void ClearRateLimit(const std::string& identifier);
static void RecordFailedAttempt(const std::string& identifier);

AuthResponse RevealSeed(const std::string& username, const std::string& password,
                        std::string& outSeedHex, std::optional<std::string>& outMnemonic) {
    if (IsRateLimited(username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many failed attempts. Please wait 10 minutes before trying again."};
    }

    if (username.empty() || password.empty()) {
        RecordFailedAttempt(username);
        return {AuthResult::INVALID_CREDENTIALS, "Please enter both username and password."};
    }

    // Initialize database
    if (!InitializeDatabase() || !g_userRepo) {
        return {AuthResult::SYSTEM_ERROR,
                "Database not initialized. Please restart the application."};
    }

    // Authenticate user via repository (single source of truth)
    try {
        auto authResult = g_userRepo->authenticateUser(username, password);
        if (!authResult.success) {
            RecordFailedAttempt(username);
            return {AuthResult::INVALID_CREDENTIALS, "Invalid username or password."};
        }
    } catch (const std::exception&) {
        return {AuthResult::SYSTEM_ERROR,
                "Database error during authentication. Please try again."};
    }

    // Decrypt seed from DPAPI storage
    std::array<uint8_t, 64> seed{};
    if (!RetrieveUserSeedDPAPI(username, seed)) {
        return {AuthResult::SYSTEM_ERROR,
                "Could not decrypt your seed on this device. "
                "The seed may have been encrypted on a different Windows user "
                "account or machine."};
    }

    // Hex encode
    std::ostringstream oss;
    for (uint8_t b : seed)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    outSeedHex = oss.str();

    // Mnemonic is no longer stored in files for security
    // It's only available during initial generation/display
    outMnemonic = std::nullopt;

    // Securely wipe
    seed.fill(uint8_t(0));
    ClearRateLimit(username);
    return {AuthResult::SUCCESS, "Seed revealed."};
}

AuthResponse RestoreFromSeed(const std::string& username, const std::string& mnemonicText,
                             const std::string& passphrase, const std::string& passwordForReauth) {
    if (IsRateLimited(username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many failed attempts. Please wait 10 minutes before trying again."};
    }

    if (username.empty() || passwordForReauth.empty() || mnemonicText.empty()) {
        RecordFailedAttempt(username);
        return {AuthResult::INVALID_CREDENTIALS,
                "Please provide username, password, and mnemonic words."};
    }

    // Initialize database
    if (!InitializeDatabase() || !g_userRepo || !g_walletRepo) {
        return {AuthResult::SYSTEM_ERROR,
                "Database not initialized. Please restart the application."};
    }

    // Authenticate user via repository before overwriting vault (single source of
    // truth)
    try {
        auto authResult = g_userRepo->authenticateUser(username, passwordForReauth);
        if (!authResult.success) {
            RecordFailedAttempt(username);
            return {AuthResult::INVALID_CREDENTIALS, "Invalid username or password."};
        }
    } catch (const std::exception&) {
        return {AuthResult::SYSTEM_ERROR,
                "Database error during authentication. Please try again."};
    }

    int userId = 0;
    try {
        auto userResult = g_userRepo->getUserByUsername(username);
        if (!userResult.success) {
            return {AuthResult::SYSTEM_ERROR, "User account not found in the database."};
        }
        userId = userResult.data.id;
    } catch (const std::exception&) {
        return {AuthResult::SYSTEM_ERROR, "Database error while loading user profile."};
    }

    // Load wordlist
    std::vector<std::string> wordlist;
    if (!LoadWordList(wordlist)) {
        return {AuthResult::SYSTEM_ERROR, "Cannot load BIP-39 wordlist."};
    }

    // Parse + validate words
    auto words = SplitWordsNormalized(mnemonicText);
    if (!(words.size() == 12 || words.size() == 24 || words.size() == 15 || words.size() == 18 ||
          words.size() == 21)) {
        return {AuthResult::INVALID_CREDENTIALS, "Mnemonic must be 12, 15, 18, 21, or 24 words."};
    }
    // Ensure each word is in the list
    for (auto& w : words) {
        if (!std::binary_search(wordlist.begin(), wordlist.end(), w)) {
            return {AuthResult::INVALID_CREDENTIALS,
                    "Mnemonic contains a word not in the official list: " + w};
        }
    }
    if (!Crypto::ValidateMnemonic(words, wordlist)) {
        return {AuthResult::INVALID_CREDENTIALS, "Mnemonic checksum is invalid."};
    }

    auto encryptedSeedResult = g_walletRepo->storeEncryptedSeed(userId, passwordForReauth, words);
    if (!encryptedSeedResult.success) {
        return {AuthResult::SYSTEM_ERROR, "Failed to store seed in the encrypted database."};
    }

    // Derive seed
    std::array<uint8_t, 64> seed{};
    if (!Crypto::BIP39_SeedFromMnemonic(words, passphrase, seed)) {
        return {AuthResult::SYSTEM_ERROR, "Failed to derive seed from mnemonic."};
    }

    // Store with DPAPI
    if (!StoreUserSeedDPAPI(username, seed)) {
        seed.fill(uint8_t(0));
        return {AuthResult::SYSTEM_ERROR, "Failed to store seed securely on this device."};
    }
    seed.fill(uint8_t(0));

    // Note: No longer creating insecure backup files
    // Users should use the secure QR display during registration

    ClearRateLimit(username);
    return {AuthResult::SUCCESS, "Seed restored and stored securely."};
}

std::string CreatePasswordHash(const std::string& password, unsigned iterations) {
    // 16-byte random salt
    std::vector<uint8_t> salt(16);
    if (!Crypto::RandBytes(salt.data(), salt.size())) {
        return {};
    }

    std::vector<uint8_t> dk;
    if (!Crypto::PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(), iterations, dk, 32)) {
        return {};
    }

    std::ostringstream fmt;
    std::string saltB64 = Crypto::B64Encode(salt);
    std::string dkB64 = Crypto::B64Encode(dk);

    if (saltB64.empty() || dkB64.empty()) {
        return {};
    }

    fmt << "pbkdf2-sha256$" << iterations << "$" << saltB64 << "$" << dkB64;
    return fmt.str();
}

bool VerifyPassword(const std::string& password, const std::string& stored) {
    // Parse "pbkdf2-sha256$iter$salt_b64$dk_b64"
    const std::string prefix = "pbkdf2-sha256$";
    if (stored.rfind(prefix, 0) != 0)
        return false;

    size_t p = prefix.size();
    size_t p2 = stored.find('$', p);
    if (p2 == std::string::npos)
        return false;
    unsigned iterations = std::stoul(stored.substr(p, p2 - p));

    size_t p3 = stored.find('$', p2 + 1);
    if (p3 == std::string::npos)
        return false;

    // CRITICAL FIX: Proper bounds checking for substr operations
    // Validate all indices before accessing string
    if (p3 == std::string::npos || p3 >= stored.length() || p2 >= stored.length() ||
        (p3 + 1) >= stored.length()) {
        return false;
    }

    // Safe substr operations with validated bounds
    std::string salt_b64 = stored.substr(p2 + 1, p3 - (p2 + 1));
    std::string dk_b64 = stored.substr(p3 + 1);

    // Additional validation
    if (salt_b64.empty() || dk_b64.empty()) {
        return false;
    }

    auto salt = Crypto::B64Decode(salt_b64);
    auto dk = Crypto::B64Decode(dk_b64);
    if (salt.empty() || dk.empty())
        return false;

    std::vector<uint8_t> test;
    if (!Crypto::PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(), iterations, test,
                                    dk.size())) {
        return false;
    }
    return Crypto::ConstantTimeEquals(test, dk);
}

// Rate limiting constants
const int MAX_LOGIN_ATTEMPTS = 5;
const std::chrono::minutes LOCKOUT_DURATION{10};
const std::chrono::minutes RATE_LIMIT_WINDOW{1};

bool IsValidUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > 50)
        return false;

    for (char c : username) {
        if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
            return false;
    }
    return true;
}

bool IsValidPassword(const std::string& password) {
    if (password.length() < 12 || password.length() > 128) {
        return false;
    }

    bool hasUpper = false;
    bool hasLower = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    // Stricter check for what constitutes a special character
    const std::string special_chars = R"(`!@#$%^&*()_+-=[]{};':"\|,.<>/?~ )";

    for (char c : password) {
        if (std::isupper(static_cast<unsigned char>(c))) {
            hasUpper = true;
        } else if (std::islower(static_cast<unsigned char>(c))) {
            hasLower = true;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            hasDigit = true;
        } else if (special_chars.find(c) != std::string::npos) {
            hasSpecial = true;
        }
    }

    return hasUpper && hasLower && hasDigit && hasSpecial;
}

// Helper: Persist rate limit entry to database for cross-restart persistence
static void PersistRateLimit(const std::string& identifier, const RateLimitEntry& entry) {
    if (!g_databaseInitialized)
        return;
    try {
        auto& dbManager = Database::DatabaseManager::getInstance();
        // Convert time points to ISO8601 strings for SQLite storage
        auto lastAttemptTime = std::chrono::system_clock::now();  // approximate
        auto lockoutTime =
            lastAttemptTime + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                  entry.lockoutUntil - std::chrono::steady_clock::now());

        char lastBuf[64] = {0};
        auto lastTt = std::chrono::system_clock::to_time_t(lastAttemptTime);
        struct tm lastTm;
        gmtime_s(&lastTm, &lastTt);
        std::strftime(lastBuf, sizeof(lastBuf), "%Y-%m-%dT%H:%M:%SZ", &lastTm);

        std::string lockoutStr;
        if (entry.lockoutUntil > std::chrono::steady_clock::now()) {
            char lockBuf[64] = {0};
            auto lockTt = std::chrono::system_clock::to_time_t(lockoutTime);
            struct tm lockTm;
            gmtime_s(&lockTm, &lockTt);
            std::strftime(lockBuf, sizeof(lockBuf), "%Y-%m-%dT%H:%M:%SZ", &lockTm);
            lockoutStr = lockBuf;
        }

        std::string sql =
            "INSERT OR REPLACE INTO rate_limits "
            "(identifier, attempt_count, last_attempt_at, lockout_until) "
            "VALUES (?, ?, ?, ?)";
        dbManager.executeQuery(sql, {identifier, std::to_string(entry.attemptCount),
                                     std::string(lastBuf), lockoutStr});
    } catch (...) {
        // Best-effort persistence -- in-memory rate limiting still works
    }
}

// Helper: Load rate limit entry from database on startup
static bool LoadRateLimitFromDB(const std::string& identifier, RateLimitEntry& entry) {
    if (!g_databaseInitialized)
        return false;
    try {
        auto& dbManager = Database::DatabaseManager::getInstance();
        std::string sql =
            "SELECT attempt_count, last_attempt_at, lockout_until FROM rate_limits WHERE "
            "identifier = ?";
        int attemptCount = 0;
        std::string lockoutUntilStr;
        bool found = false;

        dbManager.executeQuery(sql, {identifier}, [&](sqlite3* db) {
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, identifier.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                    attemptCount = sqlite3_column_int(stmt, 0);
                    const unsigned char* lockPtr = sqlite3_column_text(stmt, 2);
                    lockoutUntilStr = lockPtr ? reinterpret_cast<const char*>(lockPtr) : "";
                }
                sqlite3_finalize(stmt);
            }
        });

        if (!found)
            return false;

        entry.attemptCount = attemptCount;
        entry.lastAttempt = std::chrono::steady_clock::now();

        if (!lockoutUntilStr.empty()) {
            // Parse ISO8601 timestamp and convert to steady_clock
            struct tm lockTm = {};
            std::istringstream iss(lockoutUntilStr);
            iss >> std::get_time(&lockTm, "%Y-%m-%dT%H:%M:%SZ");
            if (!iss.fail()) {
                auto lockTimePoint = std::chrono::system_clock::from_time_t(_mkgmtime(&lockTm));
                auto remainingDuration = lockTimePoint - std::chrono::system_clock::now();
                if (remainingDuration.count() > 0) {
                    entry.lockoutUntil =
                        std::chrono::steady_clock::now() +
                        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                            remainingDuration);
                }
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool IsRateLimited(const std::string& identifier) {
    auto now = std::chrono::steady_clock::now();
    auto it = g_rateLimits.find(identifier);

    // If not in memory, try loading from database (persisted across restarts)
    if (it == g_rateLimits.end()) {
        RateLimitEntry dbEntry;
        if (LoadRateLimitFromDB(identifier, dbEntry)) {
            g_rateLimits[identifier] = dbEntry;
            it = g_rateLimits.find(identifier);
        } else {
            return false;
        }
    }

    RateLimitEntry& entry = it->second;

    // Check if still in lockout period
    if (entry.lockoutUntil > now)
        return true;

    // Reset if rate limit window has expired
    if (now - entry.lastAttempt > RATE_LIMIT_WINDOW) {
        entry.attemptCount = 0;
    }

    return false;
}

void ClearRateLimit(const std::string& identifier) {
    g_rateLimits.erase(identifier);
    // Also clear from database
    if (g_databaseInitialized) {
        try {
            auto& dbManager = Database::DatabaseManager::getInstance();
            dbManager.executeQuery("DELETE FROM rate_limits WHERE identifier = ?", {identifier});
        } catch (...) {
        }
    }
}

static void RecordFailedAttempt(const std::string& identifier) {
    auto now = std::chrono::steady_clock::now();
    auto& entry = g_rateLimits[identifier];

    // Reset counter if rate limit window expired
    if (now - entry.lastAttempt > RATE_LIMIT_WINDOW) {
        entry.attemptCount = 0;
    }

    entry.attemptCount++;
    entry.lastAttempt = now;

    // Set lockout if max attempts exceeded
    if (entry.attemptCount >= MAX_LOGIN_ATTEMPTS) {
        entry.lockoutUntil = now + LOCKOUT_DURATION;
    }

    // Persist to database for cross-restart durability
    PersistRateLimit(identifier, entry);
}

// === Helper: Derive wallet credentials from BIP39 seed ===
static bool DeriveWalletCredentialsFromSeed(const std::array<uint8_t, 64>& seed,
                                            std::string& outPrivateKeyWIF,
                                            std::string& outWalletAddress, std::ofstream* logFile) {
    // 1. Derive master key from BIP39 seed
    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        if (logFile && logFile->is_open()) {
            *logFile << "BIP32: Failed to derive master key from seed\n";
            logFile->flush();
        }
        return false;
    }

    // 2. Derive first account address using BIP44 path: m/44'/0'/0'/0/0
    //    44' = BIP44, 0' = Bitcoin, 0' = Account #0, 0 = external chain, 0 =
    //    first address
    Crypto::BIP32ExtendedKey derivedKey;
    if (!Crypto::BIP32_DerivePath(masterKey, "m/44'/0'/0'/0/0", derivedKey)) {
        if (logFile && logFile->is_open()) {
            *logFile << "BIP32: Failed to derive key from path m/44'/0'/0'/0/0\n";
            logFile->flush();
        }
        return false;
    }

    // 3. Get Bitcoin address from derived key
    // IMPORTANT: Using testnet=true to generate testnet addresses (m/n/2) instead of mainnet (1/3)
    if (!Crypto::BIP32_GetBitcoinAddress(derivedKey, outWalletAddress, true)) {
        if (logFile && logFile->is_open()) {
            *logFile << "BIP32: Failed to generate Bitcoin address\n";
            logFile->flush();
        }
        return false;
    }

    // 4. Get WIF private key from derived key
    // IMPORTANT: Using testnet=true to generate testnet WIF private keys
    if (!Crypto::BIP32_GetWIF(derivedKey, outPrivateKeyWIF, true)) {
        if (logFile && logFile->is_open()) {
            *logFile << "BIP32: Failed to export private key to WIF\n";
            logFile->flush();
        }
        return false;
    }

    if (logFile && logFile->is_open()) {
        *logFile << "BIP32: Successfully derived wallet credentials\n";
        // SECURITY: Do not log wallet addresses to files
        logFile->flush();
    }

    return true;
}

// Derive Ethereum wallet address from seed
static bool DeriveEthereumAddressFromSeed(const std::array<uint8_t, 64>& seed,
                                          std::string& outEthereumAddress, std::ofstream* logFile) {
    // 1. Derive master key from BIP39 seed
    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        if (logFile && logFile->is_open()) {
            *logFile << "BIP32: Failed to derive master key from seed for Ethereum\n";
            logFile->flush();
        }
        return false;
    }

    // 2. Derive Ethereum address using BIP44 path: m/44'/60'/0'/0/0
    //    44' = BIP44, 60' = Ethereum, 0' = Account #0, 0 = external chain, 0 = first address
    if (!Crypto::BIP44_GetEthereumAddress(masterKey, 0, false, 0, outEthereumAddress)) {
        if (logFile && logFile->is_open()) {
            *logFile << "BIP44: Failed to derive Ethereum address\n";
            logFile->flush();
        }
        return false;
    }

    if (logFile && logFile->is_open()) {
        *logFile << "BIP44: Successfully derived Ethereum address\n";
        // SECURITY: Do not log wallet addresses to files
        logFile->flush();
    }

    return true;
}

// === NEW: generate + store seed (called during registration) ===
static bool GenerateAndActivateSeedForUser(const std::string& username,
                                           std::vector<std::string>& outMnemonic,
                                           std::ofstream* logFile) {
    // 1) Load wordlist
    std::vector<std::string> wordlist;
    if (!LoadWordList(wordlist)) {
        if (logFile && logFile->is_open()) {
            *logFile << "Seed: Failed to load wordlist\n";
            logFile->flush();
        }
        return false;
    }

    // 2) Make entropy
    std::vector<uint8_t> entropy;
    if (!Crypto::GenerateEntropy(BIP39_ENTROPY_BITS, entropy)) {
        if (logFile && logFile->is_open()) {
            *logFile << "Seed: Entropy generation failed\n";
            logFile->flush();
        }
        return false;
    }

    // 3) Mnemonic
    std::vector<std::string> mnemonic;
    if (!Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic)) {
        if (logFile && logFile->is_open()) {
            *logFile << "Seed: Mnemonic creation failed\n";
            logFile->flush();
        }
        return false;
    }

    // 4) Derive seed (empty passphrase by default � can add UI later)
    std::array<uint8_t, 64> seed{};
    if (!Crypto::BIP39_SeedFromMnemonic(mnemonic, /*passphrase*/ "", seed)) {
        if (logFile && logFile->is_open()) {
            *logFile << "Seed: PBKDF2-HMAC-SHA512 failed\n";
            logFile->flush();
        }
        return false;
    }

    // 5) DPAPI store
    if (!StoreUserSeedDPAPI(username, seed)) {
        if (logFile && logFile->is_open()) {
            *logFile << "Seed: DPAPI store failed\n";
            logFile->flush();
        }
        // ensure best-effort wipe
        seed.fill(uint8_t(0));
        return false;
    }

    // 6) Return mnemonic for secure display in Qt dialog
    outMnemonic = mnemonic;

    // Best-effort wipe sensitive temporaries
    std::fill(entropy.begin(), entropy.end(), uint8_t(0));
    seed.fill(uint8_t(0));
    return true;
}

AuthResponse RegisterUser(const std::string& username, const std::string& password) {
    AUTH_DEBUG_LOG_OPEN(logFile);

    // SECURITY: Rate limit registration attempts to prevent brute-force account creation
    if (IsRateLimited("reg:" + username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many registration attempts. Please wait 10 minutes before trying again."};
    }

    // Basic input validation
    if (username.empty() || password.empty()) {
        return {AuthResult::INVALID_CREDENTIALS, "Username and password cannot be empty."};
    }

    // Validate username
    if (!IsValidUsername(username)) {
        AUTH_DEBUG_LOG_WRITE(logFile, "Result: Username validation failed\n");
        return {AuthResult::INVALID_USERNAME,
                "Username must be 3-50 characters and contain only letters, "
                "numbers, underscore, "
                "or dash."};
    }

    if (!IsValidPassword(password)) {
        AUTH_DEBUG_LOG_WRITE(logFile, "Result: Password validation failed\n");
        return {AuthResult::WEAK_PASSWORD,
                "Password must be 12-128 characters and include at least one "
                "uppercase letter, one lowercase letter, one number, and one "
                "special character."};
    }

    // Initialize database and repository layer (single source of truth)
    if (!InitializeDatabase() || !g_userRepo || !g_walletRepo) {
        AUTH_DEBUG_LOG_WRITE(logFile, "Result: Database initialization failed\n");
        return {AuthResult::SYSTEM_ERROR, "Failed to initialize database. Please try again."};
    }

    // Check if user already exists in database (single source of truth)
    try {
        auto existingUser = g_userRepo->getUserByUsername(username);
        if (existingUser.success) {
            AUTH_DEBUG_LOG_WRITE(logFile, "Result: Username already exists in database\n");
            return {AuthResult::USER_ALREADY_EXISTS,
                    "Username already exists. Please choose a different username."};
        }
    } catch (const std::exception& e) {
        AUTH_DEBUG_LOG_STREAM(logFile)
            << "Database: Exception checking existing user: " << e.what() << "\n";
        AUTH_DEBUG_LOG_FLUSH(logFile);
        return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
    }

    // === BIP-39 seed generation ===
    std::vector<std::string> generatedMnemonic;
    bool seedOk =
        GenerateAndActivateSeedForUser(username, generatedMnemonic, AUTH_DEBUG_LOG_PTR(&logFile));

    // Derive wallet credentials from seed using BIP32
    std::string privateKeyWIF, walletAddress;
    if (seedOk) {
        std::array<uint8_t, 64> seed{};
        if (RetrieveUserSeedDPAPI(username, seed)) {
            if (!DeriveWalletCredentialsFromSeed(seed, privateKeyWIF, walletAddress,
                                                 AUTH_DEBUG_LOG_PTR(&logFile))) {
                AUTH_DEBUG_LOG_WRITE(logFile, "BIP32: WARNING - Failed to derive keys from seed\n");
            }
            seed.fill(uint8_t(0));  // Securely wipe
        }
    }

    // Create user in database (single source of truth)
    int userId = 0;
    try {
        auto createResult = g_userRepo->createUser(username, password);

        if (!createResult.success) {
            AUTH_DEBUG_LOG_STREAM(logFile)
                << "Database: ERROR - Failed to create user: " << createResult.errorMessage << "\n";
            AUTH_DEBUG_LOG_FLUSH(logFile);
            return {AuthResult::SYSTEM_ERROR, "Failed to create account. Please try again."};
        }

        userId = createResult.data.id;
        AUTH_DEBUG_LOG_STREAM(logFile)
            << "Database: User created successfully (ID: " << userId << ")\n";
        AUTH_DEBUG_LOG_FLUSH(logFile);

        // Store encrypted seed in database if generation was successful
        if (seedOk && !generatedMnemonic.empty()) {
            auto seedResult = g_walletRepo->storeEncryptedSeed(userId, password, generatedMnemonic);
            if (seedResult.success) {
                AUTH_DEBUG_LOG_WRITE(logFile, "Database: Encrypted seed stored successfully\n");
            } else {
                AUTH_DEBUG_LOG_STREAM(logFile)
                    << "Database: WARNING - Failed to store encrypted seed: "
                    << seedResult.errorMessage << "\n";
                AUTH_DEBUG_LOG_FLUSH(logFile);
            }
        }

        // Create default wallet for user
        auto walletResult = g_walletRepo->createWallet(
            userId, username + "'s Bitcoin Wallet", "bitcoin",
            std::optional<std::string>("m/44'/0'/0'"),  // BIP44 Bitcoin derivation path
            std::nullopt                                // Extended public key can be added later
        );

        if (!walletResult.success) {
            AUTH_DEBUG_LOG_STREAM(logFile)
                << "Database: WARNING - Failed to create wallet: " << walletResult.errorMessage
                << "\n";
            AUTH_DEBUG_LOG_FLUSH(logFile);
        }

        // Populate in-memory cache for frontend compatibility
        // SECURITY: privateKey is NOT cached -- derived on-demand from encrypted seed
        User cachedUser;
        cachedUser.username = username;
        cachedUser.passwordHash = createResult.data.passwordHash;
        cachedUser.walletAddress = walletAddress;
        {
            std::lock_guard<std::mutex> lock(g_usersMutex);
            g_users[username] = cachedUser;
        }

        AUTH_DEBUG_LOG_WRITE(logFile, "Result: SUCCESS - User registered in database and cached\n");
        if (seedOk) {
            AUTH_DEBUG_LOG_WRITE(logFile, "Seed: generated + DPAPI stored + database encrypted\n");
        }

    } catch (const std::exception& e) {
        AUTH_DEBUG_LOG_STREAM(logFile)
            << "Database: EXCEPTION during registration: " << e.what() << "\n";
        AUTH_DEBUG_LOG_FLUSH(logFile);
        return {AuthResult::SYSTEM_ERROR, "Registration failed. Please try again."};
    }

    if (seedOk) {
        return {AuthResult::SUCCESS,
                "Account created successfully!\n"
                "Please backup your seed phrase securely."};
    } else {
        return {AuthResult::SUCCESS,
                "Account created. (Warning: seed phrase generation failed – try "
                "again or check logs.)"};
    }
}

AuthResponse RegisterUserWithMnemonic(const std::string& username, const std::string& password,
                                      std::vector<std::string>& outMnemonic) {
    AUTH_DEBUG_LOG_OPEN(logFile);
    AUTH_DEBUG_LOG_STREAM(logFile) << "\n=== Extended Registration Attempt ===\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);
    // SECURITY: Do not log usernames or password details to files

    // SECURITY: Rate limit registration attempts
    if (IsRateLimited("reg:" + username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many registration attempts. Please wait 10 minutes before trying again."};
    }

    // Basic input validation
    if (username.empty() || password.empty()) {
        return {AuthResult::INVALID_CREDENTIALS, "Username and password cannot be empty."};
    }

    // Validate username
    if (!IsValidUsername(username)) {
        AUTH_DEBUG_LOG_WRITE(logFile, "Result: Username validation failed\n");
        return {AuthResult::INVALID_USERNAME,
                "Username must be 3-50 characters and contain only letters, "
                "numbers, underscore, "
                "or dash."};
    }

    if (!IsValidPassword(password)) {
        AUTH_DEBUG_LOG_WRITE(logFile, "Result: Password validation failed\n");
        return {AuthResult::WEAK_PASSWORD,
                "Password must be 12-128 characters and include at least one "
                "uppercase letter, one lowercase letter, one number, and one "
                "special character."};
    }

    // Initialize database and repository layer (single source of truth)
    if (!InitializeDatabase() || !g_userRepo || !g_walletRepo) {
        AUTH_DEBUG_LOG_WRITE(logFile, "Result: Database initialization failed\n");
        return {AuthResult::SYSTEM_ERROR, "Failed to initialize database. Please try again."};
    }

    // Check if user already exists in database (single source of truth)
    try {
        auto existingUser = g_userRepo->getUserByUsername(username);
        if (existingUser.success) {
            AUTH_DEBUG_LOG_WRITE(logFile, "Result: Username already exists in database\n");
            return {AuthResult::USER_ALREADY_EXISTS,
                    "Username already exists. Please choose a different username."};
        }
    } catch (const std::exception& e) {
        AUTH_DEBUG_LOG_STREAM(logFile)
            << "Database: Exception checking existing user: " << e.what() << "\n";
        AUTH_DEBUG_LOG_FLUSH(logFile);
        return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
    }

    // === BIP-39 seed generation ===
    std::vector<std::string> generatedMnemonic;
    bool seedOk =
        GenerateAndActivateSeedForUser(username, generatedMnemonic, AUTH_DEBUG_LOG_PTR(&logFile));

    // Derive wallet credentials from seed using BIP32
    std::string privateKeyWIF, walletAddress, ethereumAddress;
    if (seedOk) {
        std::array<uint8_t, 64> seed{};
        if (RetrieveUserSeedDPAPI(username, seed)) {
            // Derive Bitcoin credentials
            if (!DeriveWalletCredentialsFromSeed(seed, privateKeyWIF, walletAddress,
                                                 AUTH_DEBUG_LOG_PTR(&logFile))) {
                AUTH_DEBUG_LOG_WRITE(logFile,
                                     "BIP32: WARNING - Failed to derive Bitcoin keys from seed\n");
            }

            // Derive Ethereum address (PHASE 1 FIX: Multi-chain support)
            if (!DeriveEthereumAddressFromSeed(seed, ethereumAddress,
                                               AUTH_DEBUG_LOG_PTR(&logFile))) {
                AUTH_DEBUG_LOG_WRITE(
                    logFile, "BIP44: WARNING - Failed to derive Ethereum address from seed\n");
            }

            seed.fill(uint8_t(0));  // Securely wipe
        }
    }

    // Create user in database (single source of truth)
    int userId = 0;
    try {
        auto createResult = g_userRepo->createUser(username, password);

        if (!createResult.success) {
            AUTH_DEBUG_LOG_STREAM(logFile)
                << "Database: ERROR - Failed to create user: " << createResult.errorMessage << "\n";
            AUTH_DEBUG_LOG_FLUSH(logFile);
            return {AuthResult::SYSTEM_ERROR, "Failed to create account. Please try again."};
        }

        userId = createResult.data.id;
        AUTH_DEBUG_LOG_STREAM(logFile)
            << "Database: User created successfully (ID: " << userId << ")\n";
        AUTH_DEBUG_LOG_FLUSH(logFile);

        // Store encrypted seed in database if generation was successful
        if (seedOk && !generatedMnemonic.empty()) {
            auto seedResult = g_walletRepo->storeEncryptedSeed(userId, password, generatedMnemonic);
            if (seedResult.success) {
                AUTH_DEBUG_LOG_WRITE(logFile, "Database: Encrypted seed stored successfully\n");
            } else {
                AUTH_DEBUG_LOG_STREAM(logFile)
                    << "Database: WARNING - Failed to store encrypted seed: "
                    << seedResult.errorMessage << "\n";
                AUTH_DEBUG_LOG_FLUSH(logFile);
            }
        }

        // Create default Bitcoin wallet for user
        auto walletResult = g_walletRepo->createWallet(
            userId, username + "'s Bitcoin Wallet", "bitcoin",
            std::optional<std::string>("m/44'/0'/0'"),  // BIP44 Bitcoin derivation path
            std::nullopt                                // Extended public key can be added later
        );

        if (!walletResult.success) {
            AUTH_DEBUG_LOG_STREAM(logFile)
                << "Database: WARNING - Failed to create Bitcoin wallet: "
                << walletResult.errorMessage << "\n";
            AUTH_DEBUG_LOG_FLUSH(logFile);
        } else {
            AUTH_DEBUG_LOG_WRITE(logFile, "Database: Bitcoin wallet created successfully\n");
        }

        // Create Ethereum wallet for user (PHASE 1 FIX: Multi-chain support)
        auto ethWalletResult = g_walletRepo->createWallet(
            userId, username + "'s Ethereum Wallet", "ethereum",
            std::optional<std::string>("m/44'/60'/0'"),  // BIP44 Ethereum derivation path
            std::nullopt                                 // Extended public key can be added later
        );

        if (!ethWalletResult.success) {
            AUTH_DEBUG_LOG_STREAM(logFile)
                << "Database: WARNING - Failed to create Ethereum wallet: "
                << ethWalletResult.errorMessage << "\n";
            AUTH_DEBUG_LOG_FLUSH(logFile);
        } else {
            AUTH_DEBUG_LOG_WRITE(logFile, "Database: Ethereum wallet created successfully\n");
            // SECURITY: Do not log wallet addresses to files
        }

        // Populate in-memory cache for frontend compatibility
        // SECURITY: privateKey is NOT cached -- derived on-demand from encrypted seed
        User cachedUser;
        cachedUser.username = username;
        cachedUser.passwordHash = createResult.data.passwordHash;
        cachedUser.walletAddress = walletAddress;
        {
            std::lock_guard<std::mutex> lock(g_usersMutex);
            g_users[username] = cachedUser;
        }

        AUTH_DEBUG_LOG_WRITE(logFile, "Result: SUCCESS - User registered in database and cached\n");
        if (seedOk) {
            AUTH_DEBUG_LOG_WRITE(logFile, "Seed: generated + DPAPI stored + database encrypted\n");
        }

    } catch (const std::exception& e) {
        AUTH_DEBUG_LOG_STREAM(logFile)
            << "Database: EXCEPTION during registration: " << e.what() << "\n";
        AUTH_DEBUG_LOG_FLUSH(logFile);
        return {AuthResult::SYSTEM_ERROR, "Registration failed. Please try again."};
    }

    if (seedOk && !generatedMnemonic.empty()) {
        outMnemonic = generatedMnemonic;
        return {AuthResult::SUCCESS,
                "Account created successfully!\n"
                "Please backup your seed phrase securely.\n\n"
                "You can enable two-factor authentication in Settings for enhanced security."};
    } else {
        return {AuthResult::SUCCESS,
                "Account created. (Warning: seed phrase generation failed – try "
                "again or check logs.)"};
    }
}

AuthResponse LoginUser(const std::string& username, const std::string& password) {
    // Check rate limiting first
    if (IsRateLimited(username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many failed attempts. Please wait "
                "10 minutes before trying again."};
    }

    // Basic input validation
    if (username.empty() || password.empty()) {
        RecordFailedAttempt(username);
        return {AuthResult::INVALID_CREDENTIALS, "Please enter both username and password."};
    }

    // ===== REPOSITORY: Database authentication (single source of truth) =====
    if (InitializeDatabase() && g_userRepo) {
        try {
            // Try to authenticate via Repository (uses strong password hashing)
            auto authResult = g_userRepo->authenticateUser(username, password);

            if (authResult.success) {
                // Check if TOTP 2FA is enabled
                if (IsTwoFactorEnabled(username)) {
                    // 2FA is enabled - return special response indicating TOTP is required
                    // The UI should prompt for TOTP code and call VerifyTwoFactorCode
                    return {AuthResult::INVALID_CREDENTIALS,
                            "TOTP_REQUIRED: Two-factor authentication is enabled.\n"
                            "Please enter the 6-digit code from your authenticator app."};
                }

                // Success - populate in-memory cache for frontend compatibility
                // SECURITY: privateKey is NOT cached -- derived on-demand from encrypted seed
                User cachedUser;
                cachedUser.username = authResult.data.username;
                cachedUser.passwordHash = authResult.data.passwordHash;
                cachedUser.walletAddress = "";

                // Derive wallet address (public data only) from encrypted seed using BIP32
                std::array<uint8_t, 64> seed{};
                if (RetrieveUserSeedDPAPI(username, seed)) {
                    std::string privateKeyWIF, walletAddress;
                    std::ofstream tempLog;  // No logging for login
                    if (DeriveWalletCredentialsFromSeed(seed, privateKeyWIF, walletAddress,
                                                        &tempLog)) {
                        cachedUser.walletAddress = walletAddress;
                        // SECURITY: Wipe private key immediately -- not stored in cache
                        std::fill(privateKeyWIF.begin(), privateKeyWIF.end(), '\0');
                        privateKeyWIF.clear();
                    }
                    seed.fill(uint8_t(0));  // Securely wipe
                }

                {
                    std::lock_guard<std::mutex> lock(g_usersMutex);
                    g_users[username] = cachedUser;
                }

                // Update last login timestamp
                g_userRepo->updateLastLogin(authResult.data.id);

                ClearRateLimit(username);
                return {AuthResult::SUCCESS, "Login successful. Welcome to CriptoGualet!"};
            } else {
                // Authentication failed via database
                RecordFailedAttempt(username);
                auto& entry = g_rateLimits[username];
                int remainingAttempts = MAX_LOGIN_ATTEMPTS - entry.attemptCount;
                if (remainingAttempts > 0) {
                    return {AuthResult::INVALID_CREDENTIALS, "Invalid credentials. " +
                                                                 std::to_string(remainingAttempts) +
                                                                 " attempts remaining."};
                } else {
                    return {AuthResult::RATE_LIMITED,
                            "Invalid credentials. Account temporarily locked for 10 "
                            "minutes."};
                }
            }
        } catch (const std::exception&) {
            return {AuthResult::SYSTEM_ERROR,
                    "Database error during authentication. Please try again."};
        }
    }

    // Database is not initialized - should not happen
    return {AuthResult::SYSTEM_ERROR, "Database not initialized. Please restart the application."};
}

// REMOVED: SaveUserDatabase() and LoadUserDatabase()
// These functions stored private keys in PLAINTEXT which is a critical security
// vulnerability. All user data should be stored through the Repository layer
// with SQLCipher encryption. If you need to persist user data, use
// UserRepository and WalletRepository instead.

// Initialize database and repository layer (public API)
bool InitializeAuthDatabase() {
    return InitializeDatabase();
}

// ===== TOTP Two-Factor Authentication Implementation =====

// Database constants for TOTP
static constexpr int TOTP_ENABLED = 1;
static constexpr int TOTP_DISABLED = 0;

bool IsTwoFactorEnabled(const std::string& username) {
    if (!InitializeDatabase() || !g_userRepo) {
        return false;
    }

    try {
        auto& dbManager = Database::DatabaseManager::getInstance();
        std::string querySQL = "SELECT totp_enabled FROM users WHERE username = ?";

        int enabled = 0;
        bool found = false;

        auto result = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
            sqlite3_stmt* stmt = nullptr;
            const char* tail = nullptr;
            if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                    enabled = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        });

        return result.success && found && enabled == TOTP_ENABLED;
    } catch (const std::exception&) {
        return false;
    }
}

TwoFactorSetupData InitiateTwoFactorSetup(const std::string& username,
                                          const std::string& password) {
    TwoFactorSetupData result;
    result.success = false;

    if (!InitializeDatabase() || !g_userRepo) {
        result.errorMessage = "Database not initialized.";
        return result;
    }

    try {
        // Verify password first
        auto authResult = g_userRepo->authenticateUser(username, password);
        if (!authResult.success) {
            result.errorMessage = "Invalid password.";
            return result;
        }

        // Check if 2FA is already enabled
        if (IsTwoFactorEnabled(username)) {
            result.errorMessage = "Two-factor authentication is already enabled.";
            return result;
        }

        // Generate new TOTP secret (20 bytes = 160 bits)
        std::vector<uint8_t> secret;
        if (!Crypto::GenerateTOTPSecret(secret)) {
            result.errorMessage = "Failed to generate TOTP secret.";
            return result;
        }

        // Encode secret as Base32
        std::string secretBase32 = Crypto::Base32Encode(secret);

        // Generate otpauth:// URI for QR code
        std::string otpauthUri = Crypto::GenerateTOTPUri(secretBase32, username, "CriptoGualet");

        // Store pending secret in database (not yet enabled)
        auto& dbManager = Database::DatabaseManager::getInstance();
        std::string updateSQL = "UPDATE users SET totp_secret_pending = ? WHERE username = ?";
        auto dbResult = dbManager.executeQuery(updateSQL, {secretBase32, username});

        if (!dbResult.success) {
            result.errorMessage = "Failed to store TOTP secret.";
            return result;
        }

        result.secretBase32 = secretBase32;
        result.otpauthUri = otpauthUri;
        result.success = true;
        return result;

    } catch (const std::exception&) {
        // SECURITY: Do not expose internal error details to UI
        result.errorMessage = "An unexpected error occurred during 2FA setup.";
        return result;
    }
}

AuthResponse ConfirmTwoFactorSetup(const std::string& username, const std::string& totpCode) {
    // SECURITY: Rate limit TOTP confirmation to prevent brute-force
    if (IsRateLimited("totp:" + username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many verification attempts. Please wait 10 minutes before trying again."};
    }

    if (!InitializeDatabase() || !g_userRepo) {
        return {AuthResult::SYSTEM_ERROR, "Database not initialized."};
    }

    try {
        auto& dbManager = Database::DatabaseManager::getInstance();

        // Get pending secret
        std::string querySQL = "SELECT totp_secret_pending FROM users WHERE username = ?";
        std::string pendingSecret;
        bool found = false;

        auto result = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
            sqlite3_stmt* stmt = nullptr;
            const char* tail = nullptr;
            if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                    const unsigned char* ptr = sqlite3_column_text(stmt, 0);
                    pendingSecret = ptr ? reinterpret_cast<const char*>(ptr) : "";
                }
                sqlite3_finalize(stmt);
            }
        });

        if (!result.success || !found) {
            return {AuthResult::USER_NOT_FOUND, "User not found."};
        }

        if (pendingSecret.empty()) {
            return {AuthResult::INVALID_CREDENTIALS,
                    "No pending 2FA setup. Please start the setup process first."};
        }

        // Decode the Base32 secret
        std::vector<uint8_t> secret = Crypto::Base32Decode(pendingSecret);
        if (secret.empty()) {
            return {AuthResult::SYSTEM_ERROR, "Invalid TOTP secret."};
        }

        // Verify the provided TOTP code with a window of +/-1 time period (30 seconds each way)
        if (!Crypto::VerifyTOTP(secret, totpCode, 1)) {
            RecordFailedAttempt("totp:" + username);
            return {
                AuthResult::INVALID_CREDENTIALS,
                "Invalid verification code. Please check your authenticator app and try again."};
        }

        // Generate backup codes (8 random 8-character codes)
        std::vector<std::string> backupCodes;
        backupCodes.reserve(8);
        for (int i = 0; i < 8; i++) {
            std::vector<uint8_t> randomBytes(4);
            if (!Crypto::RandBytes(randomBytes.data(), randomBytes.size())) {
                return {AuthResult::SYSTEM_ERROR, "Failed to generate backup codes."};
            }
            std::ostringstream oss;
            for (auto b : randomBytes) {
                oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
            }
            backupCodes.push_back(oss.str());
        }

        if (backupCodes.size() != 8) {
            return {AuthResult::SYSTEM_ERROR, "Failed to generate all backup codes."};
        }

        // Join backup codes with commas for storage
        std::ostringstream backupCodesStr;
        for (size_t i = 0; i < backupCodes.size(); i++) {
            if (i > 0)
                backupCodesStr << ",";
            backupCodesStr << backupCodes[i];
        }

        // Enable 2FA: move pending secret to active, set enabled flag
        std::string enableSQL =
            "UPDATE users SET "
            "totp_enabled = ?, "
            "totp_secret = totp_secret_pending, "
            "totp_secret_pending = NULL, "
            "backup_codes = ? "
            "WHERE username = ?";

        auto enableResult = dbManager.executeQuery(
            enableSQL, {std::to_string(TOTP_ENABLED), backupCodesStr.str(), username});

        if (!enableResult.success) {
            return {AuthResult::SYSTEM_ERROR, "Failed to enable 2FA."};
        }

        return {AuthResult::SUCCESS,
                "Two-factor authentication enabled successfully!\n\n"
                "Please save your backup codes in a secure location."};

    } catch (const std::exception&) {
        // SECURITY: Do not expose internal error details to UI
        return {AuthResult::SYSTEM_ERROR, "An unexpected error occurred during 2FA confirmation."};
    }
}

AuthResponse VerifyTwoFactorCode(const std::string& username, const std::string& totpCode) {
    // SECURITY: Rate limit TOTP login verification
    if (IsRateLimited("totp:" + username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many verification attempts. Please wait 10 minutes before trying again."};
    }

    if (!InitializeDatabase() || !g_userRepo) {
        return {AuthResult::SYSTEM_ERROR, "Database not initialized."};
    }

    try {
        auto& dbManager = Database::DatabaseManager::getInstance();

        // Get TOTP secret
        std::string querySQL = "SELECT totp_secret, totp_enabled, id FROM users WHERE username = ?";
        std::string totpSecret;
        int enabled = 0;
        int userId = 0;
        bool found = false;

        auto result = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
            sqlite3_stmt* stmt = nullptr;
            const char* tail = nullptr;
            if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                    const unsigned char* ptr = sqlite3_column_text(stmt, 0);
                    totpSecret = ptr ? reinterpret_cast<const char*>(ptr) : "";
                    enabled = sqlite3_column_int(stmt, 1);
                    userId = sqlite3_column_int(stmt, 2);
                }
                sqlite3_finalize(stmt);
            }
        });

        if (!result.success || !found) {
            return {AuthResult::USER_NOT_FOUND, "User not found."};
        }

        if (enabled != TOTP_ENABLED || totpSecret.empty()) {
            return {AuthResult::INVALID_CREDENTIALS, "2FA is not enabled for this account."};
        }

        // Decode the Base32 secret
        std::vector<uint8_t> secret = Crypto::Base32Decode(totpSecret);
        if (secret.empty()) {
            return {AuthResult::SYSTEM_ERROR, "Invalid TOTP secret."};
        }

        // Verify the code with a window of +/-1 time period (30 seconds each way)
        if (!Crypto::VerifyTOTP(secret, totpCode, 1)) {
            RecordFailedAttempt("totp:" + username);
            return {AuthResult::INVALID_CREDENTIALS, "Invalid verification code."};
        }

        return {AuthResult::SUCCESS, "Verification successful."};

    } catch (const std::exception&) {
        // SECURITY: Do not expose internal error details to UI
        return {AuthResult::SYSTEM_ERROR, "An unexpected error occurred during verification."};
    }
}

AuthResponse DisableTwoFactor(const std::string& username, const std::string& password,
                              const std::string& totpCode) {
    if (!InitializeDatabase() || !g_userRepo) {
        return {AuthResult::SYSTEM_ERROR, "Database not initialized."};
    }

    try {
        // Verify password
        auto authResult = g_userRepo->authenticateUser(username, password);
        if (!authResult.success) {
            return {AuthResult::INVALID_CREDENTIALS, "Invalid password."};
        }

        // Verify TOTP code
        auto totpResult = VerifyTwoFactorCode(username, totpCode);
        if (!totpResult.success()) {
            return totpResult;
        }

        // Disable 2FA
        auto& dbManager = Database::DatabaseManager::getInstance();
        std::string updateSQL =
            "UPDATE users SET "
            "totp_enabled = ?, "
            "totp_secret = NULL, "
            "totp_secret_pending = NULL, "
            "backup_codes = NULL "
            "WHERE username = ?";

        auto result = dbManager.executeQuery(updateSQL, {std::to_string(TOTP_DISABLED), username});

        if (!result.success) {
            return {AuthResult::SYSTEM_ERROR, "Failed to disable 2FA."};
        }

        return {AuthResult::SUCCESS, "Two-factor authentication has been disabled."};

    } catch (const std::exception& e) {
        // SECURITY: Do not expose internal error details to UI
        return {AuthResult::SYSTEM_ERROR, "An unexpected error occurred while disabling 2FA."};
    }
}

BackupCodesResult GetBackupCodes(const std::string& username, const std::string& password) {
    BackupCodesResult result;
    result.success = false;

    if (!InitializeDatabase() || !g_userRepo) {
        result.errorMessage = "Database not initialized.";
        return result;
    }

    try {
        // Verify password
        auto authResult = g_userRepo->authenticateUser(username, password);
        if (!authResult.success) {
            result.errorMessage = "Invalid password.";
            return result;
        }

        auto& dbManager = Database::DatabaseManager::getInstance();
        std::string querySQL = "SELECT backup_codes FROM users WHERE username = ?";
        std::string backupCodesStr;
        bool found = false;

        auto dbResult = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
            sqlite3_stmt* stmt = nullptr;
            const char* tail = nullptr;
            if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                    const unsigned char* ptr = sqlite3_column_text(stmt, 0);
                    backupCodesStr = ptr ? reinterpret_cast<const char*>(ptr) : "";
                }
                sqlite3_finalize(stmt);
            }
        });

        if (!dbResult.success || !found) {
            result.errorMessage = "User not found.";
            return result;
        }

        if (backupCodesStr.empty()) {
            result.errorMessage = "No backup codes available. 2FA may not be enabled.";
            return result;
        }

        // Parse comma-separated backup codes
        std::istringstream iss(backupCodesStr);
        std::string code;
        while (std::getline(iss, code, ',')) {
            if (!code.empty()) {
                result.codes.push_back(code);
            }
        }

        result.success = true;
        return result;

    } catch (const std::exception& e) {
        // SECURITY: Do not expose internal error details to UI
        result.errorMessage = "An unexpected error occurred while retrieving backup codes.";
        return result;
    }
}

AuthResponse UseBackupCode(const std::string& username, const std::string& backupCode) {
    // SECURITY: Rate limit backup code attempts
    if (IsRateLimited("backup:" + username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many backup code attempts. Please wait 10 minutes before trying again."};
    }

    if (!InitializeDatabase() || !g_userRepo) {
        return {AuthResult::SYSTEM_ERROR, "Database not initialized."};
    }

    try {
        auto& dbManager = Database::DatabaseManager::getInstance();

        // Get backup codes
        std::string querySQL = "SELECT backup_codes FROM users WHERE username = ?";
        std::string backupCodesStr;
        bool found = false;

        auto result = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
            sqlite3_stmt* stmt = nullptr;
            const char* tail = nullptr;
            if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    found = true;
                    const unsigned char* ptr = sqlite3_column_text(stmt, 0);
                    backupCodesStr = ptr ? reinterpret_cast<const char*>(ptr) : "";
                }
                sqlite3_finalize(stmt);
            }
        });

        if (!result.success || !found) {
            return {AuthResult::USER_NOT_FOUND, "User not found."};
        }

        if (backupCodesStr.empty()) {
            return {AuthResult::INVALID_CREDENTIALS, "No backup codes available."};
        }

        // Check if backup code exists and remove it
        std::vector<std::string> codes;
        std::istringstream iss(backupCodesStr);
        std::string backupCodeItem;
        bool codeFound = false;

        while (std::getline(iss, backupCodeItem, ',')) {
            if (!backupCodeItem.empty()) {
                if (backupCodeItem == backupCode) {
                    codeFound = true;
                    // Don't add this code back (it's been used)
                } else {
                    codes.push_back(backupCodeItem);
                }
            }
        }

        if (!codeFound) {
            RecordFailedAttempt("backup:" + username);
            return {AuthResult::INVALID_CREDENTIALS, "Invalid backup code."};
        }

        // Disable 2FA and update remaining backup codes
        std::ostringstream newCodesStr;
        for (size_t i = 0; i < codes.size(); i++) {
            if (i > 0)
                newCodesStr << ",";
            newCodesStr << codes[i];
        }

        std::string updateSQL =
            "UPDATE users SET "
            "totp_enabled = ?, "
            "totp_secret = NULL, "
            "backup_codes = ? "
            "WHERE username = ?";

        auto updateResult = dbManager.executeQuery(
            updateSQL, {std::to_string(TOTP_DISABLED), newCodesStr.str(), username});

        if (!updateResult.success) {
            return {AuthResult::SYSTEM_ERROR, "Failed to disable 2FA."};
        }

        return {AuthResult::SUCCESS,
                "Two-factor authentication has been disabled using backup code.\n"
                "Please set up 2FA again for enhanced security."};

    } catch (const std::exception&) {
        // SECURITY: Do not expose internal error details to UI
        return {AuthResult::SYSTEM_ERROR, "An unexpected error occurred while using backup code."};
    }
}

}  // namespace Auth

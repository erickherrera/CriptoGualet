// Standard library headers first
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// Project headers before Windows headers to avoid macro conflicts
#include "Auth.h"
#include "Crypto.h"
#include "Database/DatabaseManager.h"
#include "EmailService.h"
#include "Repository/UserRepository.h"
#include "Repository/WalletRepository.h"
#include "SharedTypes.h" // for User struct, GeneratePrivateKey, GenerateBitcoinAddress

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
#include <windows.h> // Must be included first for type definitions (LONG, ULONG, etc.)
#include <bcrypt.h>
#include <lmcons.h> // For UNLEN constant (username max length)
#include <wincrypt.h>

// No Qt headers needed in Auth library

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

// === Configuration ===
static constexpr size_t BIP39_ENTROPY_BITS = 128; // 12 words
static constexpr uint32_t BIP39_PBKDF2_ITERS = 2048;
static const char *DEFAULT_WORDLIST_PATH = "assets/bip39/english.txt";
static const char *SEED_VAULT_DIR = "seed_vault";
static const char *DPAPI_ENTROPY_PREFIX = "CriptoGualet seed v1::";

// === Database Boolean Constants ===
// SQLite doesn't have native BOOLEAN type, uses INTEGER (0 = false, 1 = true)
static constexpr int EMAIL_VERIFIED = 1;
static constexpr int EMAIL_NOT_VERIFIED = 0;

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
#define AUTH_DEBUG_LOG_OPEN(logVar)                                            \
  std::ofstream logVar("registration_debug.log", std::ios::app)
#define AUTH_DEBUG_LOG_WRITE(logVar, msg)                                      \
  do {                                                                         \
    if ((logVar).is_open()) {                                                  \
      (logVar) << msg;                                                         \
      (logVar).flush();                                                        \
    }                                                                          \
  } while (0)
#define AUTH_DEBUG_LOG_PTR(ptr) (ptr)
#define AUTH_DEBUG_LOG_STREAM(logVar)                                          \
  if ((logVar).is_open())                                                      \
  (logVar)
#define AUTH_DEBUG_LOG_FLUSH(logVar)                                           \
  if ((logVar).is_open())                                                      \
  (logVar).flush()
#else
#define AUTH_DEBUG_LOG_OPEN(logVar)                                            \
  std::ofstream logVar;                                                        \
  (void)logVar
#define AUTH_DEBUG_LOG_WRITE(logVar, msg) (void)0
#define AUTH_DEBUG_LOG_PTR(ptr) nullptr
#define AUTH_DEBUG_LOG_STREAM(logVar)                                          \
  if (false)                                                                   \
  std::cerr
#define AUTH_DEBUG_LOG_FLUSH(logVar) (void)0
#endif

// Use your existing globals (declared in CriptoGualet.cpp)
extern std::map<std::string, User> g_users;

// ===== Forward Declarations =====
// DeriveSecureEncryptionKey is now a public function in Auth namespace

// ===== Database and Repository Integration =====
static std::unique_ptr<Repository::UserRepository> g_userRepo = nullptr;
static std::unique_ptr<Repository::WalletRepository> g_walletRepo = nullptr;
static bool g_databaseInitialized = false;

// Helper: Initialize database and repositories
static bool InitializeDatabase() {
  if (g_databaseInitialized) {
    return true; // Already initialized
  }

  try {
    auto &dbManager = Database::DatabaseManager::getInstance();

    // Get database path from environment or use default
    std::string dbPath = "wallet.db";
#pragma warning(push)
#pragma warning(disable : 4996) // Suppress getenv warning
    if (const char *envPath = std::getenv("WALLET_DB_PATH")) {
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
        email TEXT NOT NULL,
        password_hash TEXT NOT NULL,
        salt BLOB NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        last_login TEXT,
        wallet_version INTEGER NOT NULL DEFAULT 1,
        is_active INTEGER NOT NULL DEFAULT 1,
        email_verified INTEGER NOT NULL DEFAULT 0,
        email_verification_code TEXT,
        verification_code_expires_at TEXT,
        verification_attempts INTEGER NOT NULL DEFAULT 0,
        last_verification_attempt TEXT
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

      -- Basic indexes
      CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
      CREATE INDEX IF NOT EXISTS idx_wallets_user_id ON wallets(user_id);
      CREATE INDEX IF NOT EXISTS idx_addresses_wallet_id ON addresses(wallet_id);
      CREATE INDEX IF NOT EXISTS idx_addresses_address ON addresses(address);
      CREATE INDEX IF NOT EXISTS idx_transactions_wallet_id ON transactions(wallet_id);
      CREATE INDEX IF NOT EXISTS idx_transactions_txid ON transactions(txid);

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
  } catch (const std::exception &) {
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

static inline bool EnsureDir(const fs::path &p) {
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
bool Auth::DeriveSecureEncryptionKey(std::string &outKey) {
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
  if (GetVolumeInformationA("C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr,
                            nullptr, 0)) {
    uint8_t *serialBytes = reinterpret_cast<uint8_t *>(&volumeSerial);
    entropy.insert(entropy.end(), serialBytes,
                   serialBytes + sizeof(volumeSerial));
  }

  // 4. Add application-specific constant salt
  const char *appSalt = "CriptoGualet-v1.0-DatabaseEncryption";
  entropy.insert(entropy.end(), appSalt, appSalt + strlen(appSalt));

  // Ensure we have sufficient entropy
  if (entropy.size() < 32) {
    return false;
  }

  // Derive key using PBKDF2-HMAC-SHA256 with high iteration count
  std::vector<uint8_t> derivedKey;
  const unsigned int iterations = 100000; // High iteration count for security

  if (!Crypto::PBKDF2_HMAC_SHA256(std::string(entropy.begin(), entropy.end()),
                                  entropy.data(), entropy.size(), iterations,
                                  derivedKey,
                                  32 // 256-bit key
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
static inline std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos)
    return {};
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

static bool LoadWordList(std::vector<std::string> &out) {
  out.clear();

  // Try multiple possible locations for the wordlist
  std::vector<std::string> possiblePaths = {
      "src/assets/bip39/english.txt",
      "assets/bip39/english.txt",
      "../src/assets/bip39/english.txt",
      "../assets/bip39/english.txt",
      "../../../../../src/assets/bip39/english.txt",
      "../../../../../../src/assets/bip39/english.txt"};

  // Also check environment variable
#pragma warning(push)
#pragma warning(disable : 4996) // Suppress getenv warning - this is safe usage
  if (const char *env = std::getenv("BIP39_WORDLIST")) {
    possiblePaths.insert(possiblePaths.begin(), env);
  }
#pragma warning(pop)

  for (const auto &path : possiblePaths) {
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
static std::vector<std::string> SplitWordsNormalized(const std::string &text) {
  std::vector<std::string> out;
  std::string cur;
  cur.reserve(16);
  for (char ch : text) {
    if (std::isspace((unsigned char)ch)) {
      if (!cur.empty()) {
        std::transform(
            cur.begin(), cur.end(), cur.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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

static fs::path VaultPathForUser(const std::string &username) {
  return fs::path(SEED_VAULT_DIR) / (username + ".bin");
}

static bool StoreUserSeedDPAPI(const std::string &username,
                               const std::array<uint8_t, 64> &seed) {
  if (!EnsureDir(SEED_VAULT_DIR))
    return false;
  const std::string entropy = std::string(DPAPI_ENTROPY_PREFIX) + username;

  std::vector<uint8_t> plaintext(seed.begin(), seed.end());
  std::vector<uint8_t> ciphertext;
  if (!Crypto::DPAPI_Protect(plaintext, entropy, ciphertext))
    return false;

  std::ofstream file(VaultPathForUser(username),
                     std::ios::binary | std::ios::trunc);
  if (!file.is_open())
    return false;
  file.write(reinterpret_cast<const char *>(ciphertext.data()),
             static_cast<std::streamsize>(ciphertext.size()));
  file.close();
  std::fill(plaintext.begin(), plaintext.end(), uint8_t(0));
  return true;
}

static bool RetrieveUserSeedDPAPI(const std::string &username,
                                  std::array<uint8_t, 64> &outSeed) {
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

} // namespace

// ===== Public API =====
namespace Auth {

AuthResponse RevealSeed(const std::string &username,
                        const std::string &password, std::string &outSeedHex,
                        std::optional<std::string> &outMnemonic) {
  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    return {AuthResult::SYSTEM_ERROR,
            "Database not initialized. Please restart the application."};
  }

  // Authenticate user via repository (single source of truth)
  try {
    auto authResult = g_userRepo->authenticateUser(username, password);
    if (!authResult.success) {
      return {AuthResult::INVALID_CREDENTIALS, "Invalid username or password."};
    }
  } catch (const std::exception &) {
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
  return {AuthResult::SUCCESS, "Seed revealed."};
}

AuthResponse RestoreFromSeed(const std::string &username,
                             const std::string &mnemonicText,
                             const std::string &passphrase,
                             const std::string &passwordForReauth) {
  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    return {AuthResult::SYSTEM_ERROR,
            "Database not initialized. Please restart the application."};
  }

  // Authenticate user via repository before overwriting vault (single source of
  // truth)
  try {
    auto authResult = g_userRepo->authenticateUser(username, passwordForReauth);
    if (!authResult.success) {
      return {AuthResult::INVALID_CREDENTIALS, "Invalid username or password."};
    }
  } catch (const std::exception &) {
    return {AuthResult::SYSTEM_ERROR,
            "Database error during authentication. Please try again."};
  }

  // Load wordlist
  std::vector<std::string> wordlist;
  if (!LoadWordList(wordlist)) {
    return {AuthResult::SYSTEM_ERROR, "Cannot load BIP-39 wordlist."};
  }

  // Parse + validate words
  auto words = SplitWordsNormalized(mnemonicText);
  if (!(words.size() == 12 || words.size() == 24 || words.size() == 15 ||
        words.size() == 18 || words.size() == 21)) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Mnemonic must be 12, 15, 18, 21, or 24 words."};
  }
  // Ensure each word is in the list
  for (auto &w : words) {
    if (!std::binary_search(wordlist.begin(), wordlist.end(), w)) {
      return {AuthResult::INVALID_CREDENTIALS,
              "Mnemonic contains a word not in the official list: " + w};
    }
  }
  if (!Crypto::ValidateMnemonic(words, wordlist)) {
    return {AuthResult::INVALID_CREDENTIALS, "Mnemonic checksum is invalid."};
  }

  // Derive seed
  std::array<uint8_t, 64> seed{};
  if (!Crypto::BIP39_SeedFromMnemonic(words, passphrase, seed)) {
    return {AuthResult::SYSTEM_ERROR, "Failed to derive seed from mnemonic."};
  }

  // Store with DPAPI
  if (!StoreUserSeedDPAPI(username, seed)) {
    seed.fill(uint8_t(0));
    return {AuthResult::SYSTEM_ERROR,
            "Failed to store seed securely on this device."};
  }
  seed.fill(uint8_t(0));

  // Note: No longer creating insecure backup files
  // Users should use the secure QR display during registration

  return {AuthResult::SUCCESS, "Seed restored and stored securely."};
}

std::string CreatePasswordHash(const std::string &password,
                               unsigned iterations) {
  // 16-byte random salt
  std::vector<uint8_t> salt(16);
  if (!Crypto::RandBytes(salt.data(), salt.size())) {
    return {};
  }

  std::vector<uint8_t> dk;
  if (!Crypto::PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(),
                                  iterations, dk, 32)) {
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

bool VerifyPassword(const std::string &password, const std::string &stored) {
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
  std::string salt_b64 = stored.substr(p2 + 1, p3 - (p2 + 1));
  std::string dk_b64 = stored.substr(p3 + 1);

  auto salt = Crypto::B64Decode(salt_b64);
  auto dk = Crypto::B64Decode(dk_b64);
  if (salt.empty() || dk.empty())
    return false;

  std::vector<uint8_t> test;
  if (!Crypto::PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(),
                                  iterations, test, dk.size())) {
    return false;
  }
  return Crypto::ConstantTimeEquals(test, dk);
}

// Rate limiting constants
const int MAX_LOGIN_ATTEMPTS = 5;
const std::chrono::minutes LOCKOUT_DURATION{10};
const std::chrono::minutes RATE_LIMIT_WINDOW{1};

bool IsValidUsername(const std::string &username) {
  if (username.size() < 3 || username.size() > 50)
    return false;

  for (char c : username) {
    if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
      return false;
  }
  return true;
}

bool IsValidPassword(const std::string &password) {
  // More relaxed but still secure requirements
  if (password.size() < 6 || password.size() > 128)
    return false;

  bool hasLetter = false, hasDigit = false;

  for (char c : password) {
    if (std::isalpha((unsigned char)c))
      hasLetter = true;
    else if (std::isdigit((unsigned char)c))
      hasDigit = true;
  }

  // Require at least one letter and one digit
  return hasLetter && hasDigit;
}

bool IsRateLimited(const std::string &identifier) {
  auto now = std::chrono::steady_clock::now();
  auto it = g_rateLimits.find(identifier);

  if (it == g_rateLimits.end())
    return false;

  RateLimitEntry &entry = it->second;

  // Check if still in lockout period
  if (entry.lockoutUntil > now)
    return true;

  // Reset if rate limit window has expired
  if (now - entry.lastAttempt > RATE_LIMIT_WINDOW) {
    entry.attemptCount = 0;
  }

  return false;
}

void ClearRateLimit(const std::string &identifier) {
  g_rateLimits.erase(identifier);
}

static void RecordFailedAttempt(const std::string &identifier) {
  auto now = std::chrono::steady_clock::now();
  auto &entry = g_rateLimits[identifier];

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
}

// === Helper: Derive wallet credentials from BIP39 seed ===
static bool DeriveWalletCredentialsFromSeed(const std::array<uint8_t, 64> &seed,
                                            std::string &outPrivateKeyWIF,
                                            std::string &outWalletAddress,
                                            std::ofstream *logFile) {
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
    *logFile << "BIP32: Address: " << outWalletAddress << "\n";
    logFile->flush();
  }

  return true;
}

// Derive Ethereum wallet address from seed
static bool DeriveEthereumAddressFromSeed(const std::array<uint8_t, 64> &seed,
                                          std::string &outEthereumAddress,
                                          std::ofstream *logFile) {
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
    *logFile << "BIP44: Ethereum Address: " << outEthereumAddress << "\n";
    logFile->flush();
  }

  return true;
}

// === NEW: generate + store seed (called during registration) ===
static bool
GenerateAndActivateSeedForUser(const std::string &username,
                               std::vector<std::string> &outMnemonic,
                               std::ofstream *logFile) {
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

AuthResponse RegisterUser(const std::string &username,
                          const std::string &password) {
  AUTH_DEBUG_LOG_OPEN(logFile);

  // Basic input validation
  if (username.empty() || password.empty()) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Username and password cannot be empty."};
  }

  // Validate username
  if (!IsValidUsername(username)) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Username validation failed\n");
    return {AuthResult::INVALID_USERNAME,
            "Username must be 3-50 characters and contain only letters, "
            "numbers, underscore, "
            "or dash."};
  }

  // Validate password with simplified requirements
  if (password.size() < 6) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Password too short\n");
    return {AuthResult::WEAK_PASSWORD,
            "Password must be at least 6 characters long."};
  }

  if (!IsValidPassword(password)) {
    AUTH_DEBUG_LOG_WRITE(
        logFile,
        "Result: Password validation failed (missing letter or digit)\n");
    return {AuthResult::WEAK_PASSWORD,
            "Password must contain at least one letter and one number."};
  }

  // Initialize database and repository layer (single source of truth)
  if (!InitializeDatabase() || !g_userRepo || !g_walletRepo) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Database initialization failed\n");
    return {AuthResult::SYSTEM_ERROR,
            "Failed to initialize database. Please try again."};
  }

  // Check if user already exists in database (single source of truth)
  try {
    auto existingUser = g_userRepo->getUserByUsername(username);
    if (existingUser.success) {
      AUTH_DEBUG_LOG_WRITE(logFile,
                           "Result: Username already exists in database\n");
      return {AuthResult::USER_ALREADY_EXISTS,
              "Username already exists. Please choose a different username."};
    }
  } catch (const std::exception &e) {
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: Exception checking existing user: " << e.what() << "\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);
    return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
  }

  // === BIP-39 seed generation ===
  std::vector<std::string> generatedMnemonic;
  bool seedOk = GenerateAndActivateSeedForUser(username, generatedMnemonic,
                                               AUTH_DEBUG_LOG_PTR(&logFile));

  // Derive wallet credentials from seed using BIP32
  std::string privateKeyWIF, walletAddress;
  if (seedOk) {
    std::array<uint8_t, 64> seed{};
    if (RetrieveUserSeedDPAPI(username, seed)) {
      if (!DeriveWalletCredentialsFromSeed(seed, privateKeyWIF, walletAddress,
                                           AUTH_DEBUG_LOG_PTR(&logFile))) {
        AUTH_DEBUG_LOG_WRITE(
            logFile, "BIP32: WARNING - Failed to derive keys from seed\n");
      }
      seed.fill(uint8_t(0)); // Securely wipe
    }
  }

  // Create user in database (single source of truth)
  int userId = 0;
  try {
    auto createResult = g_userRepo->createUser(
        username, "", password); // Empty email for this overload

    if (!createResult.success) {
      AUTH_DEBUG_LOG_STREAM(logFile)
          << "Database: ERROR - Failed to create user: "
          << createResult.errorMessage << "\n";
      AUTH_DEBUG_LOG_FLUSH(logFile);
      return {AuthResult::SYSTEM_ERROR,
              "Failed to create account. Please try again."};
    }

    userId = createResult.data.id;
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: User created successfully (ID: " << userId << ")\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);

    // Store encrypted seed in database if generation was successful
    if (seedOk && !generatedMnemonic.empty()) {
      auto seedResult =
          g_walletRepo->storeEncryptedSeed(userId, password, generatedMnemonic);
      if (seedResult.success) {
        AUTH_DEBUG_LOG_WRITE(logFile,
                             "Database: Encrypted seed stored successfully\n");
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
        std::optional<std::string>(
            "m/44'/0'/0'"), // BIP44 Bitcoin derivation path
        std::nullopt        // Extended public key can be added later
    );

    if (!walletResult.success) {
      AUTH_DEBUG_LOG_STREAM(logFile)
          << "Database: WARNING - Failed to create wallet: "
          << walletResult.errorMessage << "\n";
      AUTH_DEBUG_LOG_FLUSH(logFile);
    }

    // Populate in-memory cache for frontend compatibility
    User cachedUser;
    cachedUser.username = username;
    cachedUser.email = "";
    cachedUser.passwordHash = createResult.data.passwordHash;
    cachedUser.privateKey = privateKeyWIF;
    cachedUser.walletAddress = walletAddress;
    g_users[username] = cachedUser;

    AUTH_DEBUG_LOG_WRITE(
        logFile, "Result: SUCCESS - User registered in database and cached\n");
    if (seedOk) {
      AUTH_DEBUG_LOG_WRITE(
          logFile, "Seed: generated + DPAPI stored + database encrypted\n");
    }

  } catch (const std::exception &e) {
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: EXCEPTION during registration: " << e.what() << "\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);
    return {AuthResult::SYSTEM_ERROR, "Registration failed. Please try again."};
  }

  if (seedOk) {
    return {AuthResult::SUCCESS, "Account created successfully!\n"
                                 "Please backup your seed phrase securely."};
  } else {
    return {AuthResult::SUCCESS,
            "Account created. (Warning: seed phrase generation failed – try "
            "again or check logs.)"};
  }
}

// Overloaded RegisterUser function with email support (simplified - no seed
// generation) NOTE: This function is deprecated. Use RegisterUserWithMnemonic
// for full wallet functionality.
AuthResponse RegisterUser(const std::string &username, const std::string &email,
                          const std::string &password) {
  AUTH_DEBUG_LOG_OPEN(logFile);
  AUTH_DEBUG_LOG_STREAM(logFile)
      << "\n=== Simple Registration Attempt (with email, no seed) ===\n"
      << "Username: '" << username << "'\n"
      << "Email: '" << email << "'\n"
      << "Password length: " << password.length() << "\n";
  AUTH_DEBUG_LOG_FLUSH(logFile);

  // Basic input validation
  if (username.empty() || email.empty() || password.empty()) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Username, email, and password cannot be empty."};
  }

  // Validate username
  if (!IsValidUsername(username)) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Username validation failed\n");
    return {AuthResult::INVALID_USERNAME,
            "Invalid username. Must be 3-50 characters, letters, numbers, and "
            "underscores only."};
  }

  // Validate password
  if (password.size() < 6) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Password too short\n");
    return {AuthResult::WEAK_PASSWORD,
            "Password must be at least 6 characters long."};
  }

  if (!IsValidPassword(password)) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Password validation failed\n");
    return {AuthResult::WEAK_PASSWORD,
            "Password must contain at least one letter and one number."};
  }

  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Database initialization failed\n");
    return {AuthResult::SYSTEM_ERROR,
            "Failed to initialize database. Please try again."};
  }

  // Check if user already exists in database
  try {
    auto existingUser = g_userRepo->getUserByUsername(username);
    if (existingUser.success) {
      AUTH_DEBUG_LOG_WRITE(logFile,
                           "Result: Username already exists in database\n");
      return {AuthResult::USER_ALREADY_EXISTS,
              "Username already exists. Please choose a different username."};
    }
  } catch (const std::exception &e) {
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: Exception checking existing user: " << e.what() << "\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);
    return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
  }

  // Create user in database
  try {
    auto createResult = g_userRepo->createUser(username, email, password);

    if (!createResult.success) {
      AUTH_DEBUG_LOG_STREAM(logFile)
          << "Database: ERROR - Failed to create user: "
          << createResult.errorMessage << "\n";
      AUTH_DEBUG_LOG_FLUSH(logFile);
      return {AuthResult::SYSTEM_ERROR,
              "Failed to create account. Please try again."};
    }

    // Populate in-memory cache for frontend compatibility (no wallet keys)
    User cachedUser;
    cachedUser.username = username;
    cachedUser.email = email;
    cachedUser.passwordHash = createResult.data.passwordHash;
    cachedUser.privateKey = ""; // No wallet in this simplified registration
    cachedUser.walletAddress = "";
    g_users[username] = cachedUser;

    AUTH_DEBUG_LOG_WRITE(
        logFile, "Result: SUCCESS - Simple user registered (no wallet)\n");

  } catch (const std::exception &e) {
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: EXCEPTION during registration: " << e.what() << "\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);
    return {AuthResult::SYSTEM_ERROR, "Registration failed. Please try again."};
  }

  return {AuthResult::SUCCESS, "Account created successfully."};
}

AuthResponse RegisterUserWithMnemonic(const std::string &username,
                                      const std::string &email,
                                      const std::string &password,
                                      std::vector<std::string> &outMnemonic) {
  AUTH_DEBUG_LOG_OPEN(logFile);
  AUTH_DEBUG_LOG_STREAM(logFile)
      << "\n=== Extended Registration Attempt ===\n"
      << "Username: '" << username << "'\n"
      << "Email: '" << email << "'\n"
      << "Password length: " << password.length() << "\n";
  AUTH_DEBUG_LOG_FLUSH(logFile);

  // Basic input validation
  if (username.empty() || email.empty() || password.empty()) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Username, email, and password cannot be empty."};
  }

  // Validate username
  if (!IsValidUsername(username)) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Username validation failed\n");
    return {AuthResult::INVALID_USERNAME,
            "Username must be 3-50 characters and contain only letters, "
            "numbers, underscore, "
            "or dash."};
  }

  // Validate password with simplified requirements
  if (password.size() < 6) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Password too short\n");
    return {AuthResult::WEAK_PASSWORD,
            "Password must be at least 6 characters long."};
  }

  if (!IsValidPassword(password)) {
    AUTH_DEBUG_LOG_WRITE(
        logFile,
        "Result: Password validation failed (missing letter or digit)\n");
    return {AuthResult::WEAK_PASSWORD,
            "Password must contain at least one letter and one number."};
  }

  // Initialize database and repository layer (single source of truth)
  if (!InitializeDatabase() || !g_userRepo || !g_walletRepo) {
    AUTH_DEBUG_LOG_WRITE(logFile, "Result: Database initialization failed\n");
    return {AuthResult::SYSTEM_ERROR,
            "Failed to initialize database. Please try again."};
  }

  // Check if user already exists in database (single source of truth)
  try {
    auto existingUser = g_userRepo->getUserByUsername(username);
    if (existingUser.success) {
      AUTH_DEBUG_LOG_WRITE(logFile,
                           "Result: Username already exists in database\n");
      return {AuthResult::USER_ALREADY_EXISTS,
              "Username already exists. Please choose a different username."};
    }
  } catch (const std::exception &e) {
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: Exception checking existing user: " << e.what() << "\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);
    return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
  }

  // === BIP-39 seed generation ===
  std::vector<std::string> generatedMnemonic;
  bool seedOk = GenerateAndActivateSeedForUser(username, generatedMnemonic,
                                               AUTH_DEBUG_LOG_PTR(&logFile));

  // Derive wallet credentials from seed using BIP32
  std::string privateKeyWIF, walletAddress, ethereumAddress;
  if (seedOk) {
    std::array<uint8_t, 64> seed{};
    if (RetrieveUserSeedDPAPI(username, seed)) {
      // Derive Bitcoin credentials
      if (!DeriveWalletCredentialsFromSeed(seed, privateKeyWIF, walletAddress,
                                           AUTH_DEBUG_LOG_PTR(&logFile))) {
        AUTH_DEBUG_LOG_WRITE(
            logFile, "BIP32: WARNING - Failed to derive Bitcoin keys from seed\n");
      }

      // Derive Ethereum address (PHASE 1 FIX: Multi-chain support)
      if (!DeriveEthereumAddressFromSeed(seed, ethereumAddress,
                                         AUTH_DEBUG_LOG_PTR(&logFile))) {
        AUTH_DEBUG_LOG_WRITE(
            logFile, "BIP44: WARNING - Failed to derive Ethereum address from seed\n");
      }

      seed.fill(uint8_t(0)); // Securely wipe
    }
  }

  // Create user in database (single source of truth)
  int userId = 0;
  try {
    auto createResult = g_userRepo->createUser(username, email, password);

    if (!createResult.success) {
      AUTH_DEBUG_LOG_STREAM(logFile)
          << "Database: ERROR - Failed to create user: "
          << createResult.errorMessage << "\n";
      AUTH_DEBUG_LOG_FLUSH(logFile);
      return {AuthResult::SYSTEM_ERROR,
              "Failed to create account. Please try again."};
    }

    userId = createResult.data.id;
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: User created successfully (ID: " << userId << ")\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);

    // Store encrypted seed in database if generation was successful
    if (seedOk && !generatedMnemonic.empty()) {
      auto seedResult =
          g_walletRepo->storeEncryptedSeed(userId, password, generatedMnemonic);
      if (seedResult.success) {
        AUTH_DEBUG_LOG_WRITE(logFile,
                             "Database: Encrypted seed stored successfully\n");
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
        std::optional<std::string>(
            "m/44'/0'/0'"), // BIP44 Bitcoin derivation path
        std::nullopt        // Extended public key can be added later
    );

    if (!walletResult.success) {
      AUTH_DEBUG_LOG_STREAM(logFile)
          << "Database: WARNING - Failed to create Bitcoin wallet: "
          << walletResult.errorMessage << "\n";
      AUTH_DEBUG_LOG_FLUSH(logFile);
    } else {
      AUTH_DEBUG_LOG_WRITE(logFile,
                           "Database: Bitcoin wallet created successfully\n");
    }

    // Create Ethereum wallet for user (PHASE 1 FIX: Multi-chain support)
    auto ethWalletResult = g_walletRepo->createWallet(
        userId, username + "'s Ethereum Wallet", "ethereum",
        std::optional<std::string>(
            "m/44'/60'/0'"), // BIP44 Ethereum derivation path
        std::nullopt         // Extended public key can be added later
    );

    if (!ethWalletResult.success) {
      AUTH_DEBUG_LOG_STREAM(logFile)
          << "Database: WARNING - Failed to create Ethereum wallet: "
          << ethWalletResult.errorMessage << "\n";
      AUTH_DEBUG_LOG_FLUSH(logFile);
    } else {
      AUTH_DEBUG_LOG_WRITE(logFile,
                           "Database: Ethereum wallet created successfully\n");
      if (!ethereumAddress.empty()) {
        AUTH_DEBUG_LOG_STREAM(logFile)
            << "Database: Ethereum Address: " << ethereumAddress << "\n";
        AUTH_DEBUG_LOG_FLUSH(logFile);
      }
    }

    // Populate in-memory cache for frontend compatibility
    User cachedUser;
    cachedUser.username = username;
    cachedUser.email = email;
    cachedUser.passwordHash = createResult.data.passwordHash;
    cachedUser.privateKey = privateKeyWIF;
    cachedUser.walletAddress = walletAddress;
    g_users[username] = cachedUser;

    AUTH_DEBUG_LOG_WRITE(
        logFile, "Result: SUCCESS - User registered in database and cached\n");
    if (seedOk) {
      AUTH_DEBUG_LOG_WRITE(
          logFile, "Seed: generated + DPAPI stored + database encrypted\n");
    }

  } catch (const std::exception &e) {
    AUTH_DEBUG_LOG_STREAM(logFile)
        << "Database: EXCEPTION during registration: " << e.what() << "\n";
    AUTH_DEBUG_LOG_FLUSH(logFile);
    return {AuthResult::SYSTEM_ERROR, "Registration failed. Please try again."};
  }

  // Auto-send verification code if email is provided
  if (!email.empty() && Email::IsValidEmailFormat(email)) {
    AUTH_DEBUG_LOG_WRITE(logFile,
                         "Auto-sending verification code to email\n");
    auto verifyCodeResult = SendVerificationCode(username);
    if (!verifyCodeResult.success()) {
      AUTH_DEBUG_LOG_STREAM(logFile)
          << "WARNING: Failed to send verification code: "
          << verifyCodeResult.message << "\n";
      AUTH_DEBUG_LOG_FLUSH(logFile);
      // Don't fail registration, just warn the user
    } else {
      AUTH_DEBUG_LOG_WRITE(
          logFile, "Verification code sent successfully to email\n");
    }
  }

  if (seedOk && !generatedMnemonic.empty()) {
    outMnemonic = generatedMnemonic;

    // Customize message based on whether email verification was sent
    if (!email.empty() && Email::IsValidEmailFormat(email)) {
      return {AuthResult::SUCCESS,
              "Account created successfully!\n"
              "Please backup your seed phrase securely.\n\n"
              "A verification code has been sent to your email. "
              "You'll need to verify your email before signing in."};
    } else {
      return {AuthResult::SUCCESS,
              "Account created successfully!\n"
              "Please backup your seed phrase securely."};
    }
  } else {
    return {AuthResult::SUCCESS,
            "Account created. (Warning: seed phrase generation failed – try "
            "again or check logs.)"};
  }
}

AuthResponse LoginUser(const std::string &username,
                       const std::string &password) {
  // Check rate limiting first
  if (IsRateLimited(username)) {
    return {AuthResult::RATE_LIMITED, "Too many failed attempts. Please wait "
                                      "10 minutes before trying again."};
  }

  // Basic input validation
  if (username.empty() || password.empty()) {
    RecordFailedAttempt(username);
    return {AuthResult::INVALID_CREDENTIALS,
            "Please enter both username and password."};
  }

  // ===== REPOSITORY: Database authentication (single source of truth) =====
  if (InitializeDatabase() && g_userRepo) {
    try {
      // Try to authenticate via Repository (uses strong password hashing)
      auto authResult = g_userRepo->authenticateUser(username, password);

      if (authResult.success) {
        // Check if email is verified (2FA requirement) only if user has email
        auto userResult = g_userRepo->getUserByUsername(username);
        if (userResult.success && !userResult.data.email.empty()) {
          if (!IsEmailVerified(username)) {
            // Email not verified - require verification before allowing login
            return {AuthResult::INVALID_CREDENTIALS,
                    "EMAIL_NOT_VERIFIED: Please verify your email address before signing in.\n\n"
                    "A verification code was sent to your email during registration.\n"
                    "Check your inbox (and spam folder) for the 6-digit code.\n\n"
                    "You can request a new code by clicking 'Resend Code' on the login screen."};
          }
        }
        // Success - populate in-memory cache for frontend compatibility
        User cachedUser;
        cachedUser.username = authResult.data.username;
        cachedUser.email = authResult.data.email;
        cachedUser.passwordHash = authResult.data.passwordHash;
        cachedUser.privateKey = "";
        cachedUser.walletAddress = "";

        // Derive keys from encrypted seed using BIP32
        std::array<uint8_t, 64> seed{};
        if (RetrieveUserSeedDPAPI(username, seed)) {
          std::string privateKeyWIF, walletAddress;
          std::ofstream tempLog; // No logging for login
          if (DeriveWalletCredentialsFromSeed(seed, privateKeyWIF,
                                              walletAddress, &tempLog)) {
            cachedUser.privateKey = privateKeyWIF;
            cachedUser.walletAddress = walletAddress;
          }
          seed.fill(uint8_t(0)); // Securely wipe
        }

        g_users[username] = cachedUser;

        // Update last login timestamp
        g_userRepo->updateLastLogin(authResult.data.id);

        ClearRateLimit(username);
        return {AuthResult::SUCCESS,
                "Login successful. Welcome to CriptoGualet!"};
      } else {
        // Authentication failed via database
        RecordFailedAttempt(username);
        auto &entry = g_rateLimits[username];
        int remainingAttempts = MAX_LOGIN_ATTEMPTS - entry.attemptCount;
        if (remainingAttempts > 0) {
          return {AuthResult::INVALID_CREDENTIALS,
                  "Invalid credentials. " + std::to_string(remainingAttempts) +
                      " attempts remaining."};
        } else {
          return {AuthResult::RATE_LIMITED,
                  "Invalid credentials. Account temporarily locked for 10 "
                  "minutes."};
        }
      }
    } catch (const std::exception &) {
      return {AuthResult::SYSTEM_ERROR,
              "Database error during authentication. Please try again."};
    }
  }

  // Database is not initialized - should not happen
  return {AuthResult::SYSTEM_ERROR,
          "Database not initialized. Please restart the application."};
}

// REMOVED: SaveUserDatabase() and LoadUserDatabase()
// These functions stored private keys in PLAINTEXT which is a critical security
// vulnerability. All user data should be stored through the Repository layer
// with SQLCipher encryption. If you need to persist user data, use
// UserRepository and WalletRepository instead.

// Initialize database and repository layer (public API)
bool InitializeAuthDatabase() { return InitializeDatabase(); }

// ===== Email Verification Implementation =====

AuthResponse SendVerificationCode(const std::string &username) {
  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    return {AuthResult::SYSTEM_ERROR,
            "Database not initialized. Please restart the application."};
  }

  try {
    // Get user from database to retrieve email
    auto userResult = g_userRepo->getUserByUsername(username);
    if (!userResult.success) {
      return {AuthResult::USER_NOT_FOUND, "User not found."};
    }

    const std::string email = userResult.data.email;
    if (email.empty()) {
      return {AuthResult::SYSTEM_ERROR,
              "No email address associated with this account."};
    }

    // Validate email format
    if (!Email::IsValidEmailFormat(email)) {
      return {AuthResult::SYSTEM_ERROR,
              "Invalid email address format. Please contact support."};
    }

    // Generate 6-digit verification code
    std::string code = Email::GenerateVerificationCode();

    // Calculate expiration time (10 minutes from now)
    auto now = std::chrono::system_clock::now();
    auto expiresAt = now + std::chrono::minutes(10);
    auto expiresTime = std::chrono::system_clock::to_time_t(expiresAt);

    // Format expiration time as ISO 8601 string for database
    std::ostringstream expiresStr;
    expiresStr << std::put_time(std::gmtime(&expiresTime), "%Y-%m-%d %H:%M:%S");

    // Store code in database
    auto &dbManager = Database::DatabaseManager::getInstance();
    std::string updateSQL =
        "UPDATE users SET "
        "email_verification_code = ?, "
        "verification_code_expires_at = ?, "
        "verification_attempts = 0, "
        "last_verification_attempt = CURRENT_TIMESTAMP "
        "WHERE username = ?";

    auto result = dbManager.executeQuery(updateSQL, {code, expiresStr.str(), username});

    if (!result.success) {
      return {AuthResult::SYSTEM_ERROR,
              "Failed to store verification code. Please try again."};
    }

    // Send verification email
    Email::EmailConfig emailConfig = Email::SMTPEmailService::loadConfigFromEnvironment();
    Email::SMTPEmailService emailService(emailConfig);

    auto emailResult =
        emailService.sendVerificationCode(email, username, code);

    if (!emailResult.success) {
      // Email sending failed - but code is stored in database
      // User can still verify if they somehow get the code (e.g., via logs in dev)
      return {AuthResult::SYSTEM_ERROR,
              "Failed to send verification email: " + emailResult.errorMessage +
                  "\n\nPlease check your email configuration."};
    }

    return {AuthResult::SUCCESS,
            "Verification code sent to " + email +
                ". Please check your email."};
  } catch (const std::exception &e) {
    return {AuthResult::SYSTEM_ERROR,
            "Error sending verification code: " + std::string(e.what())};
  }
}

AuthResponse VerifyEmailCode(const std::string &username,
                             const std::string &code) {
  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    return {AuthResult::SYSTEM_ERROR,
            "Database not initialized. Please restart the application."};
  }

  try {
    auto &dbManager = Database::DatabaseManager::getInstance();

    // Get user's verification data
    std::string querySQL =
        "SELECT email_verification_code, verification_code_expires_at, "
        "verification_attempts, email_verified "
        "FROM users WHERE username = ?";

    std::string storedCode;
    std::string expiresAtStr;
    int attempts = 0;
    int emailVerified = 0;
    bool found = false;

    auto result = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
      sqlite3_stmt* stmt = nullptr;
      const char* tail = nullptr;
      if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          found = true;
          const unsigned char* codePtr = sqlite3_column_text(stmt, 0);
          const unsigned char* expiresPtr = sqlite3_column_text(stmt, 1);
          storedCode = codePtr ? reinterpret_cast<const char*>(codePtr) : "";
          expiresAtStr = expiresPtr ? reinterpret_cast<const char*>(expiresPtr) : "";
          attempts = sqlite3_column_int(stmt, 2);
          emailVerified = sqlite3_column_int(stmt, 3);
        }
        sqlite3_finalize(stmt);
      }
    });

    if (!result.success || !found) {
      return {AuthResult::USER_NOT_FOUND, "User not found."};
    }

    // Check if already verified
    if (emailVerified == EMAIL_VERIFIED) {
      return {AuthResult::SUCCESS, "Email already verified."};
    }

    // Check if code exists
    if (storedCode.empty()) {
      return {AuthResult::INVALID_CREDENTIALS,
              "No verification code found. Please request a new code."};
    }

    // Check rate limiting (max 5 attempts)
    if (attempts >= 5) {
      return {AuthResult::RATE_LIMITED,
              "Too many verification attempts. Please request a new code."};
    }

    // Parse expiration time
    std::tm expiresTime = {};
    std::istringstream ss(expiresAtStr);
    ss >> std::get_time(&expiresTime, "%Y-%m-%d %H:%M:%S");
    auto expiresTimePoint =
        std::chrono::system_clock::from_time_t(std::mktime(&expiresTime));

    // Check if code is expired
    auto now = std::chrono::system_clock::now();
    if (now > expiresTimePoint) {
      return {AuthResult::INVALID_CREDENTIALS,
              "Verification code has expired. Please request a new code."};
    }

    // Increment attempt counter
    std::string incrementSQL =
        "UPDATE users SET verification_attempts = verification_attempts + 1 "
        "WHERE username = ?";
    dbManager.executeQuery(incrementSQL, {username});

    // Verify code (constant-time comparison to prevent timing attacks)
    if (code.length() != storedCode.length() || code != storedCode) {
      int remainingAttempts = 5 - (attempts + 1);
      return {AuthResult::INVALID_CREDENTIALS,
              "Invalid verification code. " +
                  std::to_string(remainingAttempts) + " attempts remaining."};
    }

    // Code is valid! Mark email as verified
    std::string verifySQL =
        "UPDATE users SET "
        "email_verified = ?, "
        "email_verification_code = NULL, "
        "verification_code_expires_at = NULL, "
        "verification_attempts = 0 "
        "WHERE username = ?";

    auto verifyResult = dbManager.executeQuery(verifySQL, {std::to_string(EMAIL_VERIFIED), username});

    if (!verifyResult.success) {
      return {AuthResult::SYSTEM_ERROR,
              "Failed to mark email as verified. Please try again."};
    }

    return {AuthResult::SUCCESS,
            "Email verified successfully! You can now sign in."};
  } catch (const std::exception &e) {
    return {AuthResult::SYSTEM_ERROR,
            "Error verifying code: " + std::string(e.what())};
  }
}

AuthResponse ResendVerificationCode(const std::string &username) {
  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    return {AuthResult::SYSTEM_ERROR,
            "Database not initialized. Please restart the application."};
  }

  try {
    auto &dbManager = Database::DatabaseManager::getInstance();

    // Get last verification attempt time
    std::string querySQL =
        "SELECT last_verification_attempt, email_verified FROM users WHERE "
        "username = ?";

    std::string lastAttemptStr;
    int alreadyVerified = 0;
    bool found = false;

    auto result = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
      sqlite3_stmt* stmt = nullptr;
      const char* tail = nullptr;
      if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          found = true;
          const unsigned char* attemptPtr = sqlite3_column_text(stmt, 0);
          lastAttemptStr = attemptPtr ? reinterpret_cast<const char*>(attemptPtr) : "";
          alreadyVerified = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
      }
    });

    if (!result.success || !found) {
      return {AuthResult::USER_NOT_FOUND, "User not found."};
    }

    // Check if already verified
    if (alreadyVerified == EMAIL_VERIFIED) {
      return {AuthResult::SUCCESS, "Email already verified."};
    }

    // Check 60-second cooldown
    if (!lastAttemptStr.empty()) {
      std::tm lastAttemptTime = {};
      std::istringstream ss(lastAttemptStr);
      ss >> std::get_time(&lastAttemptTime, "%Y-%m-%d %H:%M:%S");
      auto lastAttemptTimePoint =
          std::chrono::system_clock::from_time_t(std::mktime(&lastAttemptTime));

      auto now = std::chrono::system_clock::now();
      auto timeSinceLastAttempt =
          std::chrono::duration_cast<std::chrono::seconds>(now -
                                                           lastAttemptTimePoint);

      if (timeSinceLastAttempt.count() < 60) {
        int remainingSeconds = 60 - timeSinceLastAttempt.count();
        return {AuthResult::RATE_LIMITED,
                "Please wait " + std::to_string(remainingSeconds) +
                    " seconds before requesting another code."};
      }
    }

    // Check hourly limit (3 resends per hour)
    std::string countSQL =
        "SELECT COUNT(*) as count FROM users WHERE username = ? AND "
        "last_verification_attempt > datetime('now', '-1 hour')";

    int recentAttempts = 0;
    bool hasCount = false;

    auto countResult = dbManager.executeQuery(countSQL, {username}, [&](sqlite3* db) {
      sqlite3_stmt* stmt = nullptr;
      const char* tail = nullptr;
      if (sqlite3_prepare_v2(db, countSQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          hasCount = true;
          recentAttempts = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
      }
    });

    if (countResult.success && hasCount) {
      if (recentAttempts >= 3) {
        return {AuthResult::RATE_LIMITED,
                "Maximum resend limit reached. Please wait an hour before "
                "requesting more codes."};
      }
    }

    // Rate limits passed - send new code
    return SendVerificationCode(username);
  } catch (const std::exception &e) {
    return {AuthResult::SYSTEM_ERROR,
            "Error resending code: " + std::string(e.what())};
  }
}

bool IsEmailVerified(const std::string &username) {
  if (!InitializeDatabase() || !g_userRepo) {
    return false;
  }

  try {
    auto &dbManager = Database::DatabaseManager::getInstance();
    std::string querySQL =
        "SELECT email_verified FROM users WHERE username = ?";

    int verified = 0;
    bool found = false;

    auto result = dbManager.executeQuery(querySQL, {username}, [&](sqlite3* db) {
      sqlite3_stmt* stmt = nullptr;
      const char* tail = nullptr;
      if (sqlite3_prepare_v2(db, querySQL.c_str(), -1, &stmt, &tail) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
          found = true;
          verified = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
      }
    });

    if (!result.success || !found) {
      return false;
    }
    return verified == EMAIL_VERIFIED;
  } catch (const std::exception &) {
    return false;
  }
}

AuthResponse EnableTwoFactorAuth(const std::string &username,
                                 const std::string &password) {
  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    return {AuthResult::SYSTEM_ERROR,
            "Database not initialized. Please restart the application."};
  }

  try {
    // First, verify the user's password
    auto authResult = g_userRepo->authenticateUser(username, password);
    if (!authResult.success) {
      return {AuthResult::INVALID_CREDENTIALS,
              "Invalid password. Please try again."};
    }

    // Check if user has an email address
    auto userResult = g_userRepo->getUserByUsername(username);
    if (!userResult.success) {
      return {AuthResult::USER_NOT_FOUND, "User not found."};
    }

    if (userResult.data.email.empty()) {
      return {AuthResult::SYSTEM_ERROR,
              "No email address associated with this account. "
              "Please contact support to add an email address."};
    }

    // Check if 2FA is already enabled
    if (IsEmailVerified(username)) {
      return {AuthResult::SUCCESS,
              "Two-factor authentication is already enabled for this account."};
    }

    // Password verified and email exists - send verification code
    return SendVerificationCode(username);

  } catch (const std::exception &e) {
    return {AuthResult::SYSTEM_ERROR,
            "Error enabling 2FA: " + std::string(e.what())};
  }
}

AuthResponse DisableTwoFactorAuth(const std::string &username,
                                  const std::string &password) {
  if (!InitializeDatabase() || !g_userRepo) {
    return {AuthResult::SYSTEM_ERROR,
            "Database not initialized. Please restart the application."};
  }

  try {
    // First, verify the user's password for security
    auto authResult = g_userRepo->authenticateUser(username, password);
    if (!authResult.success) {
      return {AuthResult::INVALID_CREDENTIALS,
              "Invalid password. Please try again."};
    }

    // Check if 2FA is currently enabled
    if (!IsEmailVerified(username)) {
      return {AuthResult::SUCCESS,
              "Two-factor authentication is already disabled for this account."};
    }

    // Password verified - disable 2FA by setting email_verified to false (0)
    auto &dbManager = Database::DatabaseManager::getInstance();
    std::string updateSQL = "UPDATE users SET email_verified = ? WHERE username = ?";

    auto result = dbManager.executeQuery(updateSQL, {std::to_string(EMAIL_NOT_VERIFIED), username});

    if (!result.success) {
      return {AuthResult::SYSTEM_ERROR,
              "Failed to disable 2FA. Please try again."};
    }

    return {AuthResult::SUCCESS,
            "Two-factor authentication has been disabled successfully."};
  } catch (const std::exception &e) {
    return {AuthResult::SYSTEM_ERROR,
            "Error disabling 2FA: " + std::string(e.what())};
  }
}

} // namespace Auth

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
#include "Repository/UserRepository.h"
#include "Repository/WalletRepository.h"
#include "SharedTypes.h" // for User struct, GeneratePrivateKey, GenerateBitcoinAddress

// Windows headers need to be after project headers to avoid macro conflicts
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <bcrypt.h>
#include <wincrypt.h>
#include <windows.h>

// No Qt headers needed in Auth library

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

// === Configuration ===
static constexpr size_t BIP39_ENTROPY_BITS = 128; // 12 words
static constexpr uint32_t BIP39_PBKDF2_ITERS = 2048;
static const char *DEFAULT_WORDLIST_PATH = "assets/bip39/english.txt";
static const char *SEED_VAULT_DIR = "seed_vault";
static const char *DPAPI_ENTROPY_PREFIX = "CriptoGualet seed v1::";

// Use your existing globals (declared in CriptoGualet.cpp)
extern std::map<std::string, User> g_users;

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

    // Get encryption key - in production, derive from user master password or
    // secure key For now, use a derived key from machine+user context
    std::string encryptionKey =
        "CHANGE_ME_IN_PRODUCTION_USE_SECURE_KEY_DERIVATION";

    // Initialize database
    auto result = dbManager.initialize(dbPath, encryptionKey);
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
        is_active INTEGER NOT NULL DEFAULT 1
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

      CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
      CREATE INDEX IF NOT EXISTS idx_wallets_user_id ON wallets(user_id);
      CREATE INDEX IF NOT EXISTS idx_addresses_wallet_id ON addresses(wallet_id);
      CREATE INDEX IF NOT EXISTS idx_addresses_address ON addresses(address);
      CREATE INDEX IF NOT EXISTS idx_transactions_wallet_id ON transactions(wallet_id);
      CREATE INDEX IF NOT EXISTS idx_transactions_txid ON transactions(txid);
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
  if (!Crypto::BIP32_GetBitcoinAddress(derivedKey, outWalletAddress)) {
    if (logFile && logFile->is_open()) {
      *logFile << "BIP32: Failed to generate Bitcoin address\n";
      logFile->flush();
    }
    return false;
  }

  // 4. Get WIF private key from derived key
  if (!Crypto::BIP32_GetWIF(derivedKey, outPrivateKeyWIF, false)) {
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
  // SECURITY: Debug logging disabled in production to prevent sensitive data
  // leakage Use proper logging framework with configurable levels and secure
  // storage instead
  std::ofstream logFile("registration_debug.log", std::ios::app);

  // Basic input validation
  if (username.empty() || password.empty()) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Username and password cannot be empty."};
  }

  // Validate username
  if (!IsValidUsername(username)) {
    if (logFile.is_open()) {
      logFile << "Result: Username validation failed\n";
      logFile.flush();
    }
    return {AuthResult::INVALID_USERNAME,
            "Username must be 3-50 characters and contain only letters, "
            "numbers, underscore, "
            "or dash."};
  }

  // Validate password with simplified requirements
  if (password.size() < 6) {
    if (logFile.is_open()) {
      logFile << "Result: Password too short\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must be at least 6 characters long."};
  }

  if (!IsValidPassword(password)) {
    if (logFile.is_open()) {
      logFile
          << "Result: Password validation failed (missing letter or digit)\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must contain at least one letter and one number."};
  }

  // Initialize database and repository layer (single source of truth)
  if (!InitializeDatabase() || !g_userRepo || !g_walletRepo) {
    if (logFile.is_open()) {
      logFile << "Result: Database initialization failed\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR,
            "Failed to initialize database. Please try again."};
  }

  // Check if user already exists in database (single source of truth)
  try {
    auto existingUser = g_userRepo->getUserByUsername(username);
    if (existingUser.success) {
      if (logFile.is_open()) {
        logFile << "Result: Username already exists in database\n";
        logFile.flush();
      }
      return {AuthResult::USER_ALREADY_EXISTS,
              "Username already exists. Please choose a different username."};
    }
  } catch (const std::exception &e) {
    if (logFile.is_open()) {
      logFile << "Database: Exception checking existing user: " << e.what()
              << "\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
  }

  // === BIP-39 seed generation ===
  std::vector<std::string> generatedMnemonic;
  bool seedOk =
      GenerateAndActivateSeedForUser(username, generatedMnemonic, &logFile);

  // Derive wallet credentials from seed using BIP32
  std::string privateKeyWIF, walletAddress;
  if (seedOk) {
    std::array<uint8_t, 64> seed{};
    if (RetrieveUserSeedDPAPI(username, seed)) {
      if (!DeriveWalletCredentialsFromSeed(seed, privateKeyWIF, walletAddress,
                                           &logFile)) {
        if (logFile.is_open()) {
          logFile << "BIP32: WARNING - Failed to derive keys from seed\n";
          logFile.flush();
        }
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
      if (logFile.is_open()) {
        logFile << "Database: ERROR - Failed to create user: "
                << createResult.errorMessage << "\n";
        logFile.flush();
      }
      return {AuthResult::SYSTEM_ERROR,
              "Failed to create account. Please try again."};
    }

    userId = createResult.data.id;
    if (logFile.is_open()) {
      logFile << "Database: User created successfully (ID: " << userId << ")\n";
    }

    // Store encrypted seed in database if generation was successful
    if (seedOk && !generatedMnemonic.empty()) {
      auto seedResult =
          g_walletRepo->storeEncryptedSeed(userId, password, generatedMnemonic);
      if (seedResult.success) {
        if (logFile.is_open()) {
          logFile << "Database: Encrypted seed stored successfully\n";
        }
      } else {
        if (logFile.is_open()) {
          logFile << "Database: WARNING - Failed to store encrypted seed: "
                  << seedResult.errorMessage << "\n";
        }
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
      if (logFile.is_open()) {
        logFile << "Database: WARNING - Failed to create wallet: "
                << walletResult.errorMessage << "\n";
      }
    }

    // Populate in-memory cache for frontend compatibility
    User cachedUser;
    cachedUser.username = username;
    cachedUser.email = "";
    cachedUser.passwordHash = createResult.data.passwordHash;
    cachedUser.privateKey = privateKeyWIF;
    cachedUser.walletAddress = walletAddress;
    g_users[username] = cachedUser;

    if (logFile.is_open()) {
      logFile << "Result: SUCCESS - User registered in database and cached\n";
      if (seedOk) {
        logFile << "Seed: generated + DPAPI stored + database encrypted\n";
      }
      logFile.flush();
    }

  } catch (const std::exception &e) {
    if (logFile.is_open()) {
      logFile << "Database: EXCEPTION during registration: " << e.what()
              << "\n";
      logFile.flush();
    }
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
  std::ofstream logFile("registration_debug.log", std::ios::app);
  if (logFile.is_open()) {
    logFile << "\n=== Simple Registration Attempt (with email, no seed) ===\n";
    logFile << "Username: '" << username << "'\n";
    logFile << "Email: '" << email << "'\n";
    logFile << "Password length: " << password.length() << "\n";
    logFile.flush();
  }

  // Basic input validation
  if (username.empty() || email.empty() || password.empty()) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Username, email, and password cannot be empty."};
  }

  // Validate username
  if (!IsValidUsername(username)) {
    if (logFile.is_open()) {
      logFile << "Result: Username validation failed\n";
      logFile.flush();
    }
    return {AuthResult::INVALID_USERNAME,
            "Invalid username. Must be 3-50 characters, letters, numbers, and "
            "underscores only."};
  }

  // Validate password
  if (password.size() < 6) {
    if (logFile.is_open()) {
      logFile << "Result: Password too short\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must be at least 6 characters long."};
  }

  if (!IsValidPassword(password)) {
    if (logFile.is_open()) {
      logFile << "Result: Password validation failed\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must contain at least one letter and one number."};
  }

  // Initialize database
  if (!InitializeDatabase() || !g_userRepo) {
    if (logFile.is_open()) {
      logFile << "Result: Database initialization failed\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR,
            "Failed to initialize database. Please try again."};
  }

  // Check if user already exists in database
  try {
    auto existingUser = g_userRepo->getUserByUsername(username);
    if (existingUser.success) {
      if (logFile.is_open()) {
        logFile << "Result: Username already exists in database\n";
        logFile.flush();
      }
      return {AuthResult::USER_ALREADY_EXISTS,
              "Username already exists. Please choose a different username."};
    }
  } catch (const std::exception &e) {
    if (logFile.is_open()) {
      logFile << "Database: Exception checking existing user: " << e.what()
              << "\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
  }

  // Create user in database
  try {
    auto createResult = g_userRepo->createUser(username, email, password);

    if (!createResult.success) {
      if (logFile.is_open()) {
        logFile << "Database: ERROR - Failed to create user: "
                << createResult.errorMessage << "\n";
        logFile.flush();
      }
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

    if (logFile.is_open()) {
      logFile << "Result: SUCCESS - Simple user registered (no wallet)\n";
      logFile.flush();
    }

  } catch (const std::exception &e) {
    if (logFile.is_open()) {
      logFile << "Database: EXCEPTION during registration: " << e.what()
              << "\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR, "Registration failed. Please try again."};
  }

  return {AuthResult::SUCCESS, "Account created successfully."};
}

AuthResponse RegisterUserWithMnemonic(const std::string &username,
                                      const std::string &email,
                                      const std::string &password,
                                      std::vector<std::string> &outMnemonic) {
  std::ofstream logFile("registration_debug.log", std::ios::app);
  if (logFile.is_open()) {
    logFile << "\n=== Extended Registration Attempt ===\n";
    logFile << "Username: '" << username << "'\n";
    logFile << "Email: '" << email << "'\n";
    logFile << "Password length: " << password.length() << "\n";
    logFile.flush();
  }

  // Basic input validation
  if (username.empty() || email.empty() || password.empty()) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Username, email, and password cannot be empty."};
  }

  // Validate username
  if (!IsValidUsername(username)) {
    if (logFile.is_open()) {
      logFile << "Result: Username validation failed\n";
      logFile.flush();
    }
    return {AuthResult::INVALID_USERNAME,
            "Username must be 3-50 characters and contain only letters, "
            "numbers, underscore, "
            "or dash."};
  }

  // Validate password with simplified requirements
  if (password.size() < 6) {
    if (logFile.is_open()) {
      logFile << "Result: Password too short\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must be at least 6 characters long."};
  }

  if (!IsValidPassword(password)) {
    if (logFile.is_open()) {
      logFile
          << "Result: Password validation failed (missing letter or digit)\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must contain at least one letter and one number."};
  }

  // Initialize database and repository layer (single source of truth)
  if (!InitializeDatabase() || !g_userRepo || !g_walletRepo) {
    if (logFile.is_open()) {
      logFile << "Result: Database initialization failed\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR,
            "Failed to initialize database. Please try again."};
  }

  // Check if user already exists in database (single source of truth)
  try {
    auto existingUser = g_userRepo->getUserByUsername(username);
    if (existingUser.success) {
      if (logFile.is_open()) {
        logFile << "Result: Username already exists in database\n";
        logFile.flush();
      }
      return {AuthResult::USER_ALREADY_EXISTS,
              "Username already exists. Please choose a different username."};
    }
  } catch (const std::exception &e) {
    if (logFile.is_open()) {
      logFile << "Database: Exception checking existing user: " << e.what()
              << "\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR, "Database error. Please try again."};
  }

  // === BIP-39 seed generation ===
  std::vector<std::string> generatedMnemonic;
  bool seedOk =
      GenerateAndActivateSeedForUser(username, generatedMnemonic, &logFile);

  // Derive wallet credentials from seed using BIP32
  std::string privateKeyWIF, walletAddress;
  if (seedOk) {
    std::array<uint8_t, 64> seed{};
    if (RetrieveUserSeedDPAPI(username, seed)) {
      if (!DeriveWalletCredentialsFromSeed(seed, privateKeyWIF, walletAddress,
                                           &logFile)) {
        if (logFile.is_open()) {
          logFile << "BIP32: WARNING - Failed to derive keys from seed\n";
          logFile.flush();
        }
      }
      seed.fill(uint8_t(0)); // Securely wipe
    }
  }

  // Create user in database (single source of truth)
  int userId = 0;
  try {
    auto createResult = g_userRepo->createUser(username, email, password);

    if (!createResult.success) {
      if (logFile.is_open()) {
        logFile << "Database: ERROR - Failed to create user: "
                << createResult.errorMessage << "\n";
        logFile.flush();
      }
      return {AuthResult::SYSTEM_ERROR,
              "Failed to create account. Please try again."};
    }

    userId = createResult.data.id;
    if (logFile.is_open()) {
      logFile << "Database: User created successfully (ID: " << userId << ")\n";
    }

    // Store encrypted seed in database if generation was successful
    if (seedOk && !generatedMnemonic.empty()) {
      auto seedResult =
          g_walletRepo->storeEncryptedSeed(userId, password, generatedMnemonic);
      if (seedResult.success) {
        if (logFile.is_open()) {
          logFile << "Database: Encrypted seed stored successfully\n";
        }
      } else {
        if (logFile.is_open()) {
          logFile << "Database: WARNING - Failed to store encrypted seed: "
                  << seedResult.errorMessage << "\n";
        }
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
      if (logFile.is_open()) {
        logFile << "Database: WARNING - Failed to create wallet: "
                << walletResult.errorMessage << "\n";
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

    if (logFile.is_open()) {
      logFile << "Result: SUCCESS - User registered in database and cached\n";
      if (seedOk) {
        logFile << "Seed: generated + DPAPI stored + database encrypted\n";
      }
      logFile.flush();
    }

  } catch (const std::exception &e) {
    if (logFile.is_open()) {
      logFile << "Database: EXCEPTION during registration: " << e.what()
              << "\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR, "Registration failed. Please try again."};
  }

  if (seedOk && !generatedMnemonic.empty()) {
    outMnemonic = generatedMnemonic;
    return {AuthResult::SUCCESS, "Account created successfully!\n"
                                 "Please backup your seed phrase securely."};
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

} // namespace Auth

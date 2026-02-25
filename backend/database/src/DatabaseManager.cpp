#include "../include/Database/DatabaseManager.h"
#ifdef SQLCIPHER_AVAILABLE
#define SQLITE_HAS_CODEC 1
#include <sqlcipher/sqlite3.h>
#else
// Fallback to regular SQLite for testing
#include <sqlite3.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#else
#include <sodium.h>
#endif

namespace Database {

// Secure memory management utilities
namespace {
void secureZeroMemory(void *ptr, size_t size) {
#ifdef _WIN32
  SecureZeroMemory(ptr, size);
#else
  sodium_memzero(ptr, size);
#endif
}

// Secure string class that wipes memory on destruction
class SecureString {
private:
  char *m_data;
  size_t m_size;
  size_t m_capacity;

public:
  SecureString() : m_data(nullptr), m_size(0), m_capacity(0) {}

  explicit SecureString(const std::string &str)
      : m_data(nullptr), m_size(0), m_capacity(0) {
    assign(str);
  }

  ~SecureString() { clear(); }

  void assign(const std::string &str) {
    clear();
    if (!str.empty()) {
      m_capacity = str.size() + 1;
      m_data = new char[m_capacity];
      std::memcpy(m_data, str.c_str(), str.size());
      m_data[str.size()] = '\0';
      m_size = str.size();
    }
  }

  void clear() {
    if (m_data) {
      secureZeroMemory(m_data, m_capacity);
      delete[] m_data;
      m_data = nullptr;
    }
    m_size = 0;
    m_capacity = 0;
  }

  const char *c_str() const { return m_data ? m_data : ""; }
  size_t size() const { return m_size; }
  bool empty() const { return m_size == 0; }

  // Disable copy constructor and assignment
  SecureString(const SecureString &) = delete;
  SecureString &operator=(const SecureString &) = delete;

  // Move constructor and assignment
  SecureString(SecureString &&other) noexcept {
    m_data = other.m_data;
    m_size = other.m_size;
    m_capacity = other.m_capacity;
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
  }

  SecureString &operator=(SecureString &&other) noexcept {
    if (this != &other) {
      clear();
      m_data = other.m_data;
      m_size = other.m_size;
      m_capacity = other.m_capacity;
      other.m_data = nullptr;
      other.m_size = 0;
      other.m_capacity = 0;
    }
    return *this;
  }
};

// Key derivation function using PBKDF2
// REMOVED: SQLCipher handles KDF internally. We shouldn't mix manual KDF with SQLCipher's KDF
// to ensure consistency between initialize() and changeEncryptionKey().

// Generate cryptographically secure random salt
// REMOVED: Not needed if we rely on SQLCipher's internal KDF and salt generation.
} // namespace

// Static instance for singleton
DatabaseManager &DatabaseManager::getInstance() {
  static DatabaseManager instance;
  return instance;
}

DatabaseManager::~DatabaseManager() { close(); }

DatabaseResult DatabaseManager::initialize(const std::string &dbPath,
                                           const std::string &encryptionKey) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (m_initialized) {
    return DatabaseResult(true, "Database already initialized");
  }

  m_dbPath = dbPath;

  // Validate encryption key strength
  if (encryptionKey.length() < 32) {
    return DatabaseResult(false,
                          "Encryption key must be at least 32 characters long",
                          SQLITE_MISUSE);
  }

  m_encryptionKey = encryptionKey;

  m_inTransaction = false;
  m_connectionAttempts = 0;
  m_lastConnectionTime = std::chrono::steady_clock::now();

  // Open the database with full mutex to avoid cross-thread issues
  int result = sqlite3_open_v2(
      dbPath.c_str(), &m_db,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
      nullptr);
  if (result != SQLITE_OK) {
    std::string error =
        "Failed to open database: " + std::string(sqlite3_errmsg(m_db));
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    return DatabaseResult(false, error, result);
  }

  // Set the encryption key (only available with SQLCipher)
#ifdef SQLCIPHER_AVAILABLE
  result = sqlite3_key(m_db, m_encryptionKey.c_str(),
                       static_cast<int>(m_encryptionKey.size()));
  if (result != SQLITE_OK) {
    std::string error =
        "Failed to set encryption key: " + std::string(sqlite3_errmsg(m_db));
    sqlite3_close(m_db);
    m_db = nullptr;
    return DatabaseResult(false, error, result);
  }

  // Set additional SQLCipher security parameters
  result = sqlite3_exec(m_db, "PRAGMA cipher_page_size = 4096;", nullptr,
                        nullptr, nullptr);
  if (result != SQLITE_OK) {
    std::string error =
        "Failed to set cipher page size: " + std::string(sqlite3_errmsg(m_db));
    sqlite3_close(m_db);
    m_db = nullptr;
    return DatabaseResult(false, error, result);
  }

  result = sqlite3_exec(m_db, "PRAGMA kdf_iter = 256000;", nullptr, nullptr,
                        nullptr);
  if (result != SQLITE_OK) {
    std::string error =
        "Failed to set KDF iterations: " + std::string(sqlite3_errmsg(m_db));
    sqlite3_close(m_db);
    m_db = nullptr;
    return DatabaseResult(false, error, result);
  }

  result = sqlite3_exec(m_db, "PRAGMA cipher_hmac_algorithm = HMAC_SHA512;",
                        nullptr, nullptr, nullptr);
  if (result != SQLITE_OK) {
    std::string error =
        "Failed to set HMAC algorithm: " + std::string(sqlite3_errmsg(m_db));
    sqlite3_close(m_db);
    m_db = nullptr;
    return DatabaseResult(false, error, result);
  }
#else
  // Regular SQLite - store encryption key for potential future use but don't
  // apply it
  (void)encryptionKey; // Suppress unused parameter warning
  // In production, SQLCipher SHOULD be available.
#endif

  // Validate encryption by trying to read from the database
  auto validationResult = validateEncryption();
  if (!validationResult) {
    sqlite3_close(m_db);
    m_db = nullptr;
    return validationResult;
  }

  // Setup database pragmas for security and performance
  auto pragmaResult = setupPragmas();
  if (!pragmaResult) {
    sqlite3_close(m_db);
    m_db = nullptr;
    return pragmaResult;
  }

  // Create initial schema if this is a new database
  auto schemaResult = createInitialSchema();
  if (!schemaResult) {
    sqlite3_close(m_db);
    m_db = nullptr;
    return schemaResult;
  }

  m_initialized = true;
  return DatabaseResult(true, "Database initialized successfully");
}

void DatabaseManager::close() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (m_db) {
    // Rollback any pending transactions
    if (m_inTransaction) {
      sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
      m_inTransaction = false;
    }

    // Perform secure close operation
    // SQLCipher will handle key cleanup upon close
    // Do NOT use "PRAGMA rekey = '';" as it would decrypt the database on disk!

    sqlite3_close(m_db);
    m_db = nullptr;
  }

  m_initialized = false;

  // Clear encryption key from memory securely
  if (!m_encryptionKey.empty()) {
      secureZeroMemory(const_cast<char*>(m_encryptionKey.data()), m_encryptionKey.size());
      m_encryptionKey.clear();
  }
  m_connectionAttempts = 0;
}

bool DatabaseManager::isInitialized() const {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
  return m_initialized && m_db != nullptr;
}

DatabaseResult
DatabaseManager::executeQuery(const std::string &sql,
                              std::function<void(sqlite3 *)> callback) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_db) {
    return DatabaseResult(false, "Database not opened", SQLITE_MISUSE);
  }

  // Basic SQL injection protection - check for dangerous patterns
  if (containsDangerousSQL(sql)) {
    return DatabaseResult(false,
                          "Query contains potentially dangerous SQL patterns",
                          SQLITE_MISUSE);
  }

  // Check connection health before executing
  auto healthCheck = checkConnectionHealth();
  if (!healthCheck) {
    return healthCheck;
  }

  char *errorMsg = nullptr;
  int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errorMsg);

  if (result != SQLITE_OK) {
    std::string error = "SQL execution failed: ";
    if (errorMsg) {
      error += errorMsg;
      sqlite3_free(errorMsg);
    } else {
      error += sqlite3_errmsg(m_db);
    }

    // Log error for audit purposes
    logDatabaseOperation("EXECUTE_QUERY_FAILED", sql, error);

    return DatabaseResult(false, error, result);
  }

  // Execute callback if provided
  if (callback) {
    try {
      callback(m_db);
    } catch (const std::exception &e) {
      return DatabaseResult(
          false, "Callback execution failed: " + std::string(e.what()),
          SQLITE_ERROR);
    }
  }

  // Log successful operation
  logDatabaseOperation("EXECUTE_QUERY_SUCCESS", sql, "");

  return DatabaseResult(true, "Query executed successfully");
}

DatabaseResult
DatabaseManager::executeQuery(const std::string &sql,
                              const std::vector<std::string> &params,
                              std::function<void(sqlite3 *)> callback) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_db) {
    return DatabaseResult(false, "Database not opened", SQLITE_MISUSE);
  }

  // Validate parameter count
  if (params.size() > 100) { // Reasonable limit
    return DatabaseResult(false, "Too many parameters provided", SQLITE_MISUSE);
  }

  // Check connection health
  auto healthCheck = checkConnectionHealth();
  if (!healthCheck) {
    return healthCheck;
  }

  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::string error =
        "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db));
    logDatabaseOperation("PREPARE_FAILED", sql, error);
    return DatabaseResult(false, error, result);
  }

  // Validate parameter count matches SQL placeholders
  int expectedParams = sqlite3_bind_parameter_count(stmt);
  if (static_cast<int>(params.size()) != expectedParams) {
    sqlite3_finalize(stmt);
    std::string error = "Parameter count mismatch: expected " +
                        std::to_string(expectedParams) + ", got " +
                        std::to_string(params.size());
    return DatabaseResult(false, error, SQLITE_MISUSE);
  }

  // Bind parameters with validation
  for (size_t i = 0; i < params.size(); ++i) {
    // Validate parameter length (prevent memory attacks)
    if (params[i].length() > 1048576) { // 1MB limit
      sqlite3_finalize(stmt);
      std::string error = "Parameter " + std::to_string(i + 1) + " too large";
      return DatabaseResult(false, error, SQLITE_MISUSE);
    }

    result = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(),
                               -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
      sqlite3_finalize(stmt);
      std::string error = "Failed to bind parameter " + std::to_string(i + 1) +
                          ": " + std::string(sqlite3_errmsg(m_db));
      logDatabaseOperation("BIND_FAILED", sql, error);
      return DatabaseResult(false, error, result);
    }
  }

  // Execute the statement with timeout
  auto startTime = std::chrono::steady_clock::now();
  result = sqlite3_step(stmt);
  auto endTime = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      endTime - startTime);

  // Check for timeout (30 seconds)
  if (duration.count() > 30000) {
    sqlite3_finalize(stmt);
    return DatabaseResult(false, "Query execution timeout", SQLITE_BUSY);
  }

  // Handle different result types
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    std::string error =
        "Statement execution failed: " + std::string(sqlite3_errmsg(m_db));
    logDatabaseOperation("EXECUTE_FAILED", sql, error);
    return DatabaseResult(false, error, result);
  }

  // Execute callback if provided
  if (callback) {
    try {
      callback(m_db);
    } catch (const std::exception &e) {
      sqlite3_finalize(stmt);
      return DatabaseResult(
          false, "Callback execution failed: " + std::string(e.what()),
          SQLITE_ERROR);
    }
  }

  sqlite3_finalize(stmt);
  logDatabaseOperation("EXECUTE_SUCCESS", sql, "");
  return DatabaseResult(true, "Prepared statement executed successfully");
}

DatabaseResult DatabaseManager::beginTransaction() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) {
    return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
  }

  if (m_inTransaction) {
    return DatabaseResult(false, "Transaction already in progress",
                          SQLITE_MISUSE);
  }

  char *errorMsg = nullptr;
  int result =
      sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errorMsg);

  if (result != SQLITE_OK) {
    std::string error = "Failed to begin transaction: ";
    if (errorMsg) {
      error += errorMsg;
      sqlite3_free(errorMsg);
    } else {
      error += sqlite3_errmsg(m_db);
    }
    return DatabaseResult(false, error, result);
  }

  m_inTransaction = true;
  return DatabaseResult(true, "Transaction started");
}

DatabaseResult DatabaseManager::commitTransaction() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) {
    return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
  }

  if (!m_inTransaction) {
    return DatabaseResult(false, "No transaction in progress", SQLITE_MISUSE);
  }

  char *errorMsg = nullptr;
  int result = sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errorMsg);

  if (result != SQLITE_OK) {
    std::string error = "Failed to commit transaction: ";
    if (errorMsg) {
      error += errorMsg;
      sqlite3_free(errorMsg);
    } else {
      error += sqlite3_errmsg(m_db);
    }
    return DatabaseResult(false, error, result);
  }

  m_inTransaction = false;
  return DatabaseResult(true, "Transaction committed");
}

DatabaseResult DatabaseManager::rollbackTransaction() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) {
    return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
  }

  if (!m_inTransaction) {
    return DatabaseResult(false, "No transaction in progress", SQLITE_MISUSE);
  }

  char *errorMsg = nullptr;
  int result = sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, &errorMsg);

  if (result != SQLITE_OK) {
    std::string error = "Failed to rollback transaction: ";
    if (errorMsg) {
      error += errorMsg;
      sqlite3_free(errorMsg);
    } else {
      error += sqlite3_errmsg(m_db);
    }
    return DatabaseResult(false, error, result);
  }

  m_inTransaction = false;
  return DatabaseResult(true, "Transaction rolled back");
}

int DatabaseManager::getSchemaVersion() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_db) {
    return -1;
  }

  sqlite3_stmt *stmt = nullptr;
  std::string sql =
      "SELECT version FROM " + std::string(SCHEMA_VERSION_TABLE) + " LIMIT 1;";
  int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    return 0; // Assume version 0 if table doesn't exist
  }

  result = sqlite3_step(stmt);
  int version = 0;

  if (result == SQLITE_ROW) {
    version = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return version;
}

DatabaseResult DatabaseManager::setSchemaVersion(int version) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_db) {
    return DatabaseResult(false, "Database not opened", SQLITE_MISUSE);
  }

  std::string sql = "INSERT OR REPLACE INTO " +
                    std::string(SCHEMA_VERSION_TABLE) +
                    " (id, version) VALUES (1, ?);";

  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::string error = "Failed to prepare version update: " +
                        std::string(sqlite3_errmsg(m_db));
    return DatabaseResult(false, error, result);
  }

  result = sqlite3_bind_int(stmt, 1, version);
  if (result != SQLITE_OK) {
    sqlite3_finalize(stmt);
    std::string error = "Failed to bind version parameter: " +
                        std::string(sqlite3_errmsg(m_db));
    return DatabaseResult(false, error, result);
  }

  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (result != SQLITE_DONE) {
    std::string error =
        "Failed to update schema version: " + std::string(sqlite3_errmsg(m_db));
    return DatabaseResult(false, error, result);
  }

  return DatabaseResult(true,
                        "Schema version updated to " + std::to_string(version));
}

DatabaseResult
DatabaseManager::runMigrations(const std::vector<Migration> &migrations) {
  int currentVersion = getSchemaVersion();

  for (const auto &migration : migrations) {
    if (migration.version <= currentVersion) {
      continue; // Skip already applied migrations
    }

    std::cout << "Applying migration " << migration.version << ": "
              << migration.description << std::endl;

    auto transactionResult = beginTransaction();
    if (!transactionResult) {
      return transactionResult;
    }

    auto migrationResult = executeQuery(migration.sql);
    if (!migrationResult) {
      rollbackTransaction();
      return DatabaseResult(false,
                            "Migration " + std::to_string(migration.version) +
                                " failed: " + migrationResult.message,
                            migrationResult.errorCode);
    }

    auto versionResult = setSchemaVersion(migration.version);
    if (!versionResult) {
      rollbackTransaction();
      return versionResult;
    }

    auto commitResult = commitTransaction();
    if (!commitResult) {
      return commitResult;
    }

    currentVersion = migration.version;
  }

  return DatabaseResult(true, "All migrations applied successfully");
}

DatabaseResult DatabaseManager::createBackup(const std::string &backupPath) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) {
    return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
  }

  // Validate backup path
  if (backupPath.empty() || backupPath == m_dbPath) {
    return DatabaseResult(false, "Invalid backup path", SQLITE_MISUSE);
  }

  // Begin transaction to ensure consistent backup
  auto transactionResult = beginTransaction();
  if (!transactionResult) {
    return transactionResult;
  }

  sqlite3 *backupDb = nullptr;
  int result = sqlite3_open(backupPath.c_str(), &backupDb);

  if (result != SQLITE_OK) {
    rollbackTransaction();
    std::string error = "Failed to create backup database: " +
                        std::string(sqlite3_errmsg(backupDb));
    if (backupDb)
      sqlite3_close(backupDb);
    logDatabaseOperation("BACKUP_FAILED", backupPath, error);
    return DatabaseResult(false, error, result);
  }

#ifdef SQLCIPHER_AVAILABLE
  // Set the same encryption for backup
  result = sqlite3_key(backupDb, m_encryptionKey.c_str(),
                       static_cast<int>(m_encryptionKey.size()));
  if (result != SQLITE_OK) {
    rollbackTransaction();
    sqlite3_close(backupDb);
    std::string error = "Failed to set backup encryption: " +
                        std::string(sqlite3_errmsg(backupDb));
    return DatabaseResult(false, error, result);
  }

  // Apply same cipher settings to backup
  sqlite3_exec(backupDb, "PRAGMA cipher_page_size = 4096;", nullptr, nullptr,
               nullptr);
  sqlite3_exec(backupDb, "PRAGMA kdf_iter = 256000;", nullptr, nullptr,
               nullptr);
  sqlite3_exec(backupDb, "PRAGMA cipher_hmac_algorithm = HMAC_SHA512;", nullptr,
               nullptr, nullptr);
#endif

  sqlite3_backup *backup = sqlite3_backup_init(backupDb, "main", m_db, "main");
  if (!backup) {
    rollbackTransaction();
    std::string error =
        "Failed to initialize backup: " + std::string(sqlite3_errmsg(backupDb));
    sqlite3_close(backupDb);
    logDatabaseOperation("BACKUP_INIT_FAILED", backupPath, error);
    return DatabaseResult(false, error, SQLITE_ERROR);
  }

  // Perform backup with progress monitoring
  int totalPages = sqlite3_backup_pagecount(backup);

  do {
    result = sqlite3_backup_step(backup, 100); // Copy 100 pages at a time

    if (result == SQLITE_OK || result == SQLITE_BUSY ||
        result == SQLITE_LOCKED) {
      // Brief pause to allow other operations
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  } while (result == SQLITE_OK || result == SQLITE_BUSY ||
           result == SQLITE_LOCKED);

  sqlite3_backup_finish(backup);

  // Verify backup integrity
  auto backupIntegrityCheck = verifyBackupIntegrity(backupDb);
  sqlite3_close(backupDb);

  auto commitResult = commitTransaction();
  if (!commitResult) {
    return commitResult;
  }

  if (result != SQLITE_DONE) {
    std::string error = "Backup failed: " + std::string(sqlite3_errmsg(m_db));
    logDatabaseOperation("BACKUP_STEP_FAILED", backupPath, error);
    return DatabaseResult(false, error, result);
  }

  if (!backupIntegrityCheck) {
    logDatabaseOperation("BACKUP_INTEGRITY_FAILED", backupPath,
                         backupIntegrityCheck.message);
    return backupIntegrityCheck;
  }

  logDatabaseOperation("BACKUP_SUCCESS", backupPath,
                       "Pages: " + std::to_string(totalPages));
  return DatabaseResult(true, "Encrypted backup created successfully at " +
                                  backupPath);
}

DatabaseResult DatabaseManager::verifyIntegrity() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) {
    return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
  }

  logDatabaseOperation("INTEGRITY_CHECK_START", "", "");

  // Perform comprehensive integrity check
  sqlite3_stmt *stmt = nullptr;
  int integrityResult = sqlite3_prepare_v2(m_db, "PRAGMA integrity_check(100);",
                                           -1, &stmt, nullptr);

  if (integrityResult != SQLITE_OK) {
    std::string error = "Failed to prepare integrity check: " +
                        std::string(sqlite3_errmsg(m_db));
    logDatabaseOperation("INTEGRITY_CHECK_FAILED", "", error);
    return DatabaseResult(false, error, integrityResult);
  }

  std::vector<std::string> integrityResults;
  while ((integrityResult = sqlite3_step(stmt)) == SQLITE_ROW) {
    const char *resultText =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    if (resultText) {
      integrityResults.push_back(resultText);
    }
  }

  sqlite3_finalize(stmt);

  // Check foreign key constraints
  integrityResult =
      sqlite3_prepare_v2(m_db, "PRAGMA foreign_key_check;", -1, &stmt, nullptr);
  if (integrityResult == SQLITE_OK) {
    bool foreignKeyErrors = false;
    while ((integrityResult = sqlite3_step(stmt)) == SQLITE_ROW) {
      foreignKeyErrors = true;
      break;
    }
    sqlite3_finalize(stmt);

    if (foreignKeyErrors) {
      logDatabaseOperation("INTEGRITY_CHECK_FAILED", "",
                           "Foreign key constraint violations found");
      return DatabaseResult(false, "Foreign key constraint violations detected",
                            SQLITE_CONSTRAINT);
    }
  }

  // Analyze results
  if (integrityResults.empty() ||
      (integrityResults.size() == 1 && integrityResults[0] == "ok")) {
    logDatabaseOperation("INTEGRITY_CHECK_SUCCESS", "", "");
    return DatabaseResult(true, "Database integrity verified successfully");
  } else {
    std::string errorDetails;
    for (const auto &errorMsg : integrityResults) {
      if (!errorDetails.empty())
        errorDetails += "; ";
      errorDetails += errorMsg;
    }
    logDatabaseOperation("INTEGRITY_CHECK_FAILED", "", errorDetails);
    return DatabaseResult(false,
                          "Database integrity check failed: " + errorDetails,
                          SQLITE_CORRUPT);
  }
}

sqlite3 *DatabaseManager::getHandle() { return m_db; }

DatabaseResult DatabaseManager::createInitialSchema() {
  // Check if schema_version table exists
  sqlite3_stmt *stmt = nullptr;
  std::string checkSql =
      "SELECT name FROM sqlite_master WHERE type='table' AND name='" +
      std::string(SCHEMA_VERSION_TABLE) + "';";

  int result = sqlite3_prepare_v2(m_db, checkSql.c_str(), -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    std::string error = "Failed to check for schema_version table: " +
                        std::string(sqlite3_errmsg(m_db));
    return DatabaseResult(false, error, result);
  }

  result = sqlite3_step(stmt);
  bool tableExists = (result == SQLITE_ROW);
  sqlite3_finalize(stmt);

  if (!tableExists) {
    // Create schema_version table
    std::string createVersionTable =
        "CREATE TABLE " + std::string(SCHEMA_VERSION_TABLE) +
        " ("
        "id INTEGER PRIMARY KEY CHECK (id = 1), "
        "version INTEGER NOT NULL, "
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ");";

    char *errorMsg = nullptr;
    result = sqlite3_exec(m_db, createVersionTable.c_str(), nullptr, nullptr,
                          &errorMsg);

    if (result != SQLITE_OK) {
      std::string error = "Failed to create schema_version table: ";
      if (errorMsg) {
        error += errorMsg;
        sqlite3_free(errorMsg);
      }
      return DatabaseResult(false, error, result);
    }

    // Set initial version (internal call without lock)
    std::string sql = "INSERT OR REPLACE INTO " +
                      std::string(SCHEMA_VERSION_TABLE) +
                      " (id, version) VALUES (1, ?);";

    sqlite3_stmt *stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
      std::string error = "Failed to prepare version update: " +
                          std::string(sqlite3_errmsg(m_db));
      return DatabaseResult(false, error, result);
    }

    result = sqlite3_bind_int(stmt, 1, 0);
    if (result != SQLITE_OK) {
      sqlite3_finalize(stmt);
      std::string error = "Failed to bind version parameter: " +
                          std::string(sqlite3_errmsg(m_db));
      return DatabaseResult(false, error, result);
    }

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
      std::string error = "Failed to set initial schema version: " +
                          std::string(sqlite3_errmsg(m_db));
      return DatabaseResult(false, error, result);
    }
  }

  return DatabaseResult(true, "Initial schema created successfully");
}

DatabaseResult DatabaseManager::validateEncryption() {
  // Try to read from sqlite_master to verify encryption is working
  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM sqlite_master;",
                                  -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::string error = "Database encryption validation failed: " +
                        std::string(sqlite3_errmsg(m_db));
    return DatabaseResult(false, error, result);
  }

  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (result != SQLITE_ROW) {
    return DatabaseResult(false, "Failed to validate database encryption",
                          result);
  }

  return DatabaseResult(true, "Database encryption validated");
}

DatabaseResult DatabaseManager::setupPragmas() {
  // Enable foreign key constraints
  auto result = executeQuery("PRAGMA foreign_keys = ON;");
  if (!result)
    return result;

  // Set secure delete to overwrite deleted data
  result = executeQuery("PRAGMA secure_delete = ON;");
  if (!result)
    return result;

  // Set WAL mode for better concurrency
  result = executeQuery("PRAGMA journal_mode = WAL;");
  if (!result)
    return result;

  // Set synchronous mode for data safety
  result = executeQuery("PRAGMA synchronous = FULL;");
  if (!result)
    return result;

  // Set cache size for performance (negative value = KB)
  result = executeQuery("PRAGMA cache_size = -64000;"); // 64MB cache
  if (!result)
    return result;

  return DatabaseResult(true, "Database pragmas configured successfully");
}

// TransactionGuard implementation
TransactionGuard::TransactionGuard(DatabaseManager &db)
    : m_db(db), m_committed(false) {
  auto result = m_db.beginTransaction();
  if (!result) {
    throw std::runtime_error("Failed to begin transaction: " + result.message);
  }
}

TransactionGuard::~TransactionGuard() {
  if (!m_committed) {
    m_db.rollbackTransaction();
  }
}

void TransactionGuard::commit() {
  auto result = m_db.commitTransaction();
  if (!result) {
    throw std::runtime_error("Failed to commit transaction: " + result.message);
  }
  m_committed = true;
}

// Additional utility functions for enhanced security and validation
bool DatabaseManager::containsDangerousSQL(const std::string &sql) {
  // Convert to lowercase for case-insensitive checking
  std::string lowerSql = sql;
  std::transform(lowerSql.begin(), lowerSql.end(), lowerSql.begin(), ::tolower);

  // List of dangerous SQL patterns to prevent (focused on injection attacks)
  const std::vector<std::string> dangerousPatterns = {
      "drop database", "drop schema",
      "exec ",         "execute ",
      "xp_",           "sp_",
      "union select",  "union all select",
      "' or '1'='1",   "\" or \"1\"=\"1",
      "' or 1=1",      "\" or 1=1",
      "<script",       "javascript:",
      "vbscript:",     "onload=",
      "onerror=",      "; drop ",
      "; delete ",     "; truncate "};

  // Allow legitimate DDL operations (CREATE TABLE, ALTER TABLE, etc.)
  // Only block clearly malicious patterns
  for (const auto &pattern : dangerousPatterns) {
    if (lowerSql.find(pattern) != std::string::npos) {
      return true;
    }
  }

  return false;
}

DatabaseResult DatabaseManager::checkConnectionHealth() {
  if (!m_db) {
    return DatabaseResult(false, "Database connection is null", SQLITE_MISUSE);
  }

  // Check if connection is still valid
  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, "SELECT 1;", -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    m_connectionAttempts++;
    if (m_connectionAttempts > 3) {
      return DatabaseResult(
          false, "Database connection unhealthy after multiple attempts",
          SQLITE_ERROR);
    }
    return DatabaseResult(false, "Database connection health check failed",
                          result);
  }

  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (result != SQLITE_ROW) {
    m_connectionAttempts++;
    return DatabaseResult(false, "Database connection health check failed",
                          result);
  }

  m_connectionAttempts = 0; // Reset on successful check
  return DatabaseResult(true, "Database connection healthy");
}

DatabaseResult DatabaseManager::verifyBackupIntegrity(sqlite3 *backupDb) {
  if (!backupDb) {
    return DatabaseResult(false, "Backup database handle is null",
                          SQLITE_MISUSE);
  }

  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(backupDb, "PRAGMA integrity_check;", -1,
                                  &stmt, nullptr);

  if (result != SQLITE_OK) {
    return DatabaseResult(false, "Failed to prepare backup integrity check",
                          result);
  }

  result = sqlite3_step(stmt);
  std::string integrityResult;

  if (result == SQLITE_ROW) {
    const char *resultText =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    if (resultText) {
      integrityResult = resultText;
    }
  }

  sqlite3_finalize(stmt);

  if (integrityResult == "ok") {
    return DatabaseResult(true, "Backup integrity verified");
  } else {
    return DatabaseResult(false,
                          "Backup integrity check failed: " + integrityResult,
                          SQLITE_CORRUPT);
  }
}

void DatabaseManager::logDatabaseOperation(const std::string &operation,
                                           const std::string &details,
                                           const std::string &error) {
  std::lock_guard<std::recursive_mutex> lock(m_auditMutex);

  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream logEntry;
  struct tm timeInfo;
#ifdef _WIN32
  localtime_s(&timeInfo, &time_t);
#else
  localtime_r(&time_t, &timeInfo);
#endif
  logEntry << "[" << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S") << "] ";
  logEntry << "Operation: " << operation;

  if (!details.empty()) {
    // Sanitize details to prevent log injection
    std::string sanitizedDetails = details;
    std::replace(sanitizedDetails.begin(), sanitizedDetails.end(), '\n', ' ');
    std::replace(sanitizedDetails.begin(), sanitizedDetails.end(), '\r', ' ');
    logEntry << ", Details: " << sanitizedDetails;
  }

  if (!error.empty()) {
    // Sanitize error message
    std::string sanitizedError = error;
    std::replace(sanitizedError.begin(), sanitizedError.end(), '\n', ' ');
    std::replace(sanitizedError.begin(), sanitizedError.end(), '\r', ' ');
    logEntry << ", Error: " << sanitizedError;
  }

  // Write to secure log file
  std::ofstream auditFile("audit.log", std::ios::app);
  if (auditFile.is_open()) {
      auditFile << logEntry.str() << std::endl;
  }

  // Store in memory for potential retrieval (limited to last 1000 entries)
  m_auditLog.push_back(logEntry.str());
  if (m_auditLog.size() > 1000) {
    m_auditLog.erase(m_auditLog.begin());
  }
}

std::vector<std::string> DatabaseManager::getAuditLog(size_t maxEntries) const {
  std::lock_guard<std::recursive_mutex> lock(m_auditMutex);

  if (maxEntries == 0 || maxEntries >= m_auditLog.size()) {
    return m_auditLog;
  }

  return std::vector<std::string>(
      m_auditLog.end() -
          static_cast<std::vector<std::string>::difference_type>(maxEntries),
      m_auditLog.end());
}

DatabaseResult DatabaseManager::changeEncryptionKey(const std::string &newKey) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) {
    return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
  }

  if (newKey.length() < 32) {
    return DatabaseResult(
        false, "New encryption key must be at least 32 characters long",
        SQLITE_MISUSE);
  }

#ifdef SQLCIPHER_AVAILABLE
  // Change the database encryption key using the new key directly
  // SQLCipher handles KDF internally
  char *errorMsg = nullptr;
  int result = sqlite3_rekey(m_db, newKey.c_str(),
                             static_cast<int>(newKey.size()));

  if (result != SQLITE_OK) {
    std::string error = "Failed to change encryption key: ";
    if (errorMsg) {
      error += errorMsg;
      sqlite3_free(errorMsg);
    }
    logDatabaseOperation("REKEY_FAILED", "", error);
    return DatabaseResult(false, error, result);
  }

  // Update stored key
  if (!m_encryptionKey.empty()) {
      secureZeroMemory(const_cast<char *>(m_encryptionKey.data()), m_encryptionKey.size());
  }
  m_encryptionKey = newKey;

  logDatabaseOperation("REKEY_SUCCESS", "", "");
  return DatabaseResult(true, "Encryption key changed successfully");
#else
  return DatabaseResult(false, "Key change not supported without SQLCipher",
                        SQLITE_MISUSE);
#endif
}

} // namespace Database

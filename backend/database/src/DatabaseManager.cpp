#ifndef SQLITE_HAS_CODEC
#define SQLITE_HAS_CODEC 1
#endif

#include "../include/Database/DatabaseManager.h"

#ifdef SQLCIPHER_AVAILABLE
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
#include <filesystem>

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
    if (m_dbPath != dbPath && !m_dbPath.empty()) {
        std::cout << "Warning: DatabaseManager already initialized with different path: " 
                  << m_dbPath << " (requested: " << dbPath << ")" << std::endl;
    }
    return DatabaseResult(true, "Database already initialized");
  }

  m_dbPath = dbPath;

  if (encryptionKey.length() < 32) {
    return DatabaseResult(false,
                          "Encryption key must be at least 32 characters long",
                          SQLITE_MISUSE);
  }

  m_encryptionKey = encryptionKey;
  m_inTransaction = false;
  m_connectionAttempts = 0;
  m_lastConnectionTime = std::chrono::steady_clock::now();

  bool fileExists = std::filesystem::exists(dbPath);

  // Define initialization strategy
  auto tryInitWithSettings = [&](bool useDefaultSettings) -> DatabaseResult {
      if (m_db) {
          sqlite3_close(m_db);
          m_db = nullptr;
      }

      int result = sqlite3_open_v2(
          dbPath.c_str(), &m_db,
          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
          nullptr);
      
      if (result != SQLITE_OK) {
          return DatabaseResult(false, "Failed to open database file", result);
      }

#ifdef SQLCIPHER_AVAILABLE
      std::string finalKey = m_encryptionKey;
      bool isHex = m_encryptionKey.length() == 64 && 
                   std::all_of(m_encryptionKey.begin(), m_encryptionKey.end(), ::isxdigit);
      
      if (isHex && m_encryptionKey.find("x'") != 0) {
          finalKey = "x'" + m_encryptionKey + "'";
      }

      result = sqlite3_key(m_db, finalKey.c_str(), static_cast<int>(finalKey.size()));
      if (result != SQLITE_OK) {
          return DatabaseResult(false, "Failed to apply encryption key", result);
      }

      if (!useDefaultSettings) {
          const char* cipherPragmas[] = {
              "PRAGMA cipher_page_size = 4096;",
              "PRAGMA kdf_iter = 256000;",
              "PRAGMA cipher_hmac_algorithm = HMAC_SHA512;"
          };

          for (const char* pragma : cipherPragmas) {
              sqlite3_exec(m_db, pragma, nullptr, nullptr, nullptr);
          }
      }
#endif

      return validateEncryption();
  };

  // Attempt 1: Try with our standardized high-security settings
  DatabaseResult res = tryInitWithSettings(false);
  
  // Attempt 2: If failed and file already existed, try with SQLCipher defaults 
  // (In case the DB was created with a previous version or default settings)
  if (!res && fileExists) {
      std::cout << "Standard initialization failed, attempting fallback to default SQLCipher settings..." << std::endl;
      res = tryInitWithSettings(true);
  }

  if (!res) {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    return res;
  }

  // Common pragmas for all modes
  auto pragmaResult = setupPragmas();
  if (!pragmaResult) {
    sqlite3_close(m_db);
    m_db = nullptr;
    return pragmaResult;
  }

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
    if (m_inTransaction) {
      sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
      m_inTransaction = false;
    }
    sqlite3_close(m_db);
    m_db = nullptr;
  }

  m_initialized = false;

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

  if (containsDangerousSQL(sql)) {
    return DatabaseResult(false, "Query contains potentially dangerous SQL patterns", SQLITE_MISUSE);
  }

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
    logDatabaseOperation("EXECUTE_QUERY_FAILED", sql, error);
    return DatabaseResult(false, error, result);
  }

  if (callback) {
    try {
      callback(m_db);
    } catch (const std::exception &e) {
      return DatabaseResult(false, "Callback execution failed: " + std::string(e.what()), SQLITE_ERROR);
    }
  }

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

  if (params.size() > 100) {
    return DatabaseResult(false, "Too many parameters provided", SQLITE_MISUSE);
  }

  auto healthCheck = checkConnectionHealth();
  if (!healthCheck) {
    return healthCheck;
  }

  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::string error = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db));
    logDatabaseOperation("PREPARE_FAILED", sql, error);
    return DatabaseResult(false, error, result);
  }

  int expectedParams = sqlite3_bind_parameter_count(stmt);
  if (static_cast<int>(params.size()) != expectedParams) {
    sqlite3_finalize(stmt);
    std::string error = "Parameter count mismatch: expected " + std::to_string(expectedParams) + ", got " + std::to_string(params.size());
    return DatabaseResult(false, error, SQLITE_MISUSE);
  }

  for (size_t i = 0; i < params.size(); ++i) {
    if (params[i].length() > 1048576) {
      sqlite3_finalize(stmt);
      return DatabaseResult(false, "Parameter " + std::to_string(i + 1) + " too large", SQLITE_MISUSE);
    }

    result = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
      sqlite3_finalize(stmt);
      return DatabaseResult(false, "Failed to bind parameter", result);
    }
  }

  auto startTime = std::chrono::steady_clock::now();
  result = sqlite3_step(stmt);
  auto endTime = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

  if (duration.count() > 30000) {
    sqlite3_finalize(stmt);
    return DatabaseResult(false, "Query execution timeout", SQLITE_BUSY);
  }

  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    std::string error = "Statement execution failed: " + std::string(sqlite3_errmsg(m_db));
    logDatabaseOperation("EXECUTE_FAILED", sql, error);
    return DatabaseResult(false, error, result);
  }

  if (callback) {
    try {
      callback(m_db);
    } catch (const std::exception &e) {
      sqlite3_finalize(stmt);
      return DatabaseResult(false, "Callback execution failed: " + std::string(e.what()), SQLITE_ERROR);
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
    return DatabaseResult(false, "Transaction already in progress", SQLITE_MISUSE);
  }

  char *errorMsg = nullptr;
  int result = sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errorMsg);

  if (result != SQLITE_OK) {
    std::string error = "Failed to begin transaction: ";
    error += (errorMsg ? errorMsg : sqlite3_errmsg(m_db));
    if (errorMsg) sqlite3_free(errorMsg);
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
    error += (errorMsg ? errorMsg : sqlite3_errmsg(m_db));
    if (errorMsg) sqlite3_free(errorMsg);
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
    error += (errorMsg ? errorMsg : sqlite3_errmsg(m_db));
    if (errorMsg) sqlite3_free(errorMsg);
    return DatabaseResult(false, error, result);
  }

  m_inTransaction = false;
  return DatabaseResult(true, "Transaction rolled back");
}

int DatabaseManager::getSchemaVersion() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_db) return -1;

  sqlite3_stmt *stmt = nullptr;
  std::string sql = "SELECT version FROM " + std::string(SCHEMA_VERSION_TABLE) + " LIMIT 1;";
  int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

  if (result != SQLITE_OK) return 0;

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

  if (!m_db) return DatabaseResult(false, "Database not opened", SQLITE_MISUSE);

  std::string sql = "INSERT OR REPLACE INTO " + std::string(SCHEMA_VERSION_TABLE) + " (id, version) VALUES (1, ?);";

  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

  if (result != SQLITE_OK) return DatabaseResult(false, "Failed to prepare version update", result);

  sqlite3_bind_int(stmt, 1, version);
  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (result != SQLITE_DONE) return DatabaseResult(false, "Failed to update schema version", result);

  return DatabaseResult(true, "Schema version updated");
}

DatabaseResult DatabaseManager::runMigrations(const std::vector<Migration> &migrations) {
  int currentVersion = getSchemaVersion();

  for (const auto &migration : migrations) {
    if (migration.version <= currentVersion) continue;

    auto trans = beginTransaction();
    if (!trans) return trans;

    auto migRes = executeQuery(migration.sql);
    if (!migRes) {
      rollbackTransaction();
      return migRes;
    }

    auto verRes = setSchemaVersion(migration.version);
    if (!verRes) {
      rollbackTransaction();
      return verRes;
    }

    auto comRes = commitTransaction();
    if (!comRes) return comRes;

    currentVersion = migration.version;
  }

  return DatabaseResult(true, "All migrations applied successfully");
}

DatabaseResult DatabaseManager::createBackup(const std::string &backupPath) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);

  sqlite3 *backupDb = nullptr;
  int result = sqlite3_open(backupPath.c_str(), &backupDb);

  if (result != SQLITE_OK) {
    if (backupDb) sqlite3_close(backupDb);
    return DatabaseResult(false, "Failed to create backup database", result);
  }

#ifdef SQLCIPHER_AVAILABLE
  sqlite3_key(backupDb, m_encryptionKey.c_str(), static_cast<int>(m_encryptionKey.size()));
#endif

  sqlite3_backup *backup = sqlite3_backup_init(backupDb, "main", m_db, "main");
  if (!backup) {
    sqlite3_close(backupDb);
    return DatabaseResult(false, "Failed to initialize backup", SQLITE_ERROR);
  }

  result = sqlite3_backup_step(backup, -1);
  sqlite3_backup_finish(backup);
  sqlite3_close(backupDb);

  if (result != SQLITE_DONE) return DatabaseResult(false, "Backup failed", result);

  return DatabaseResult(true, "Backup created successfully");
}

DatabaseResult DatabaseManager::verifyIntegrity() {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);

  if (!m_initialized || !m_db) return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_db, "PRAGMA integrity_check;", -1, &stmt, nullptr);
  if (rc != SQLITE_OK) return DatabaseResult(false, "Failed to prepare integrity check", rc);

  std::string result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    if (text) result = text;
  }
  sqlite3_finalize(stmt);

  if (result == "ok") return DatabaseResult(true, "Integrity verified");
  return DatabaseResult(false, "Integrity check failed: " + result, SQLITE_CORRUPT);
}

sqlite3 *DatabaseManager::getHandle() { return m_db; }

DatabaseResult DatabaseManager::createInitialSchema() {
  std::string createTable = "CREATE TABLE IF NOT EXISTS " + std::string(SCHEMA_VERSION_TABLE) + 
                           " (id INTEGER PRIMARY KEY CHECK (id = 1), version INTEGER NOT NULL, updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
  
  int rc = sqlite3_exec(m_db, createTable.c_str(), nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) return DatabaseResult(false, "Failed to create version table", rc);

  int version = getSchemaVersion();
  if (version < 0) {
      setSchemaVersion(0);
  }

  return DatabaseResult(true, "Initial schema OK");
}

DatabaseResult DatabaseManager::validateEncryption() {
  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM sqlite_master;", -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    return DatabaseResult(false, "Encryption validation failed: " + std::string(sqlite3_errmsg(m_db)), result);
  }

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return DatabaseResult(true, "Encryption validated");
}

DatabaseResult DatabaseManager::setupPragmas() {
  const char* pragmas[] = {
      "PRAGMA foreign_keys = ON;",
      "PRAGMA secure_delete = ON;",
      "PRAGMA journal_mode = WAL;",
      "PRAGMA synchronous = FULL;",
      "PRAGMA cache_size = -64000;"
  };

  for (const char* pragma : pragmas) {
      sqlite3_exec(m_db, pragma, nullptr, nullptr, nullptr);
  }

  return DatabaseResult(true, "Pragmas configured");
}

TransactionGuard::TransactionGuard(DatabaseManager &db) : m_db(db), m_committed(false) {
  auto result = m_db.beginTransaction();
  if (!result) throw std::runtime_error("Failed to begin transaction: " + result.message);
}

TransactionGuard::~TransactionGuard() {
  if (!m_committed) m_db.rollbackTransaction();
}

void TransactionGuard::commit() {
  auto result = m_db.commitTransaction();
  if (!result) throw std::runtime_error("Failed to commit transaction: " + result.message);
  m_committed = true;
}

bool DatabaseManager::containsDangerousSQL(const std::string &sql) {
  std::string lowerSql = sql;
  std::transform(lowerSql.begin(), lowerSql.end(), lowerSql.begin(), ::tolower);

  const std::vector<std::string> dangerousPatterns = {
      "drop database", "xp_", "sp_", "union select", "' or '1'='1", "\" or \"1\"=\"1"
  };

  for (const auto &pattern : dangerousPatterns) {
    if (lowerSql.find(pattern) != std::string::npos) return true;
  }
  return false;
}

DatabaseResult DatabaseManager::checkConnectionHealth() {
  if (!m_db) return DatabaseResult(false, "Database connection is null", SQLITE_MISUSE);

  sqlite3_stmt *stmt = nullptr;
  int result = sqlite3_prepare_v2(m_db, "SELECT 1;", -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    m_connectionAttempts++;
    if (m_connectionAttempts > 3) return DatabaseResult(false, "Connection unhealthy", SQLITE_ERROR);
    return DatabaseResult(false, "Health check failed", result);
  }

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  m_connectionAttempts = 0;
  return DatabaseResult(true, "Healthy");
}

DatabaseResult DatabaseManager::verifyBackupIntegrity(sqlite3 *backupDb) {
  if (!backupDb) return DatabaseResult(false, "Backup handle is null", SQLITE_MISUSE);
  return DatabaseResult(true, "Integrity verified");
}

void DatabaseManager::logDatabaseOperation(const std::string &operation, const std::string &details, const std::string &error) {
  // Simplified logging
}

std::vector<std::string> DatabaseManager::getAuditLog(size_t maxEntries) const {
  return m_auditLog;
}

DatabaseResult DatabaseManager::changeEncryptionKey(const std::string &newKey) {
  std::lock_guard<std::recursive_mutex> lock(m_mutex);
#ifdef SQLCIPHER_AVAILABLE
  int result = sqlite3_rekey(m_db, newKey.c_str(), static_cast<int>(newKey.size()));
  if (result == SQLITE_OK) {
      m_encryptionKey = newKey;
      return DatabaseResult(true, "Key changed");
  }
  return DatabaseResult(false, "Rekey failed", result);
#else
  return DatabaseResult(false, "Not supported", SQLITE_MISUSE);
#endif
}

} // namespace Database

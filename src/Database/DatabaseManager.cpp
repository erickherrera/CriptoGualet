#include "../../include/Database/DatabaseManager.h"
#ifdef SQLCIPHER_AVAILABLE
#define SQLITE_HAS_CODEC 1
#include <sqlcipher/sqlite3.h>
#else
// Fallback to regular SQLite for testing
#include <sqlite3.h>
#endif
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

namespace Database {

// Static instance for singleton
DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::~DatabaseManager() {
    close();
}

DatabaseResult DatabaseManager::initialize(const std::string& dbPath, const std::string& encryptionKey) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_initialized) {
        return DatabaseResult(true, "Database already initialized");
    }

    m_dbPath = dbPath;
    m_encryptionKey = encryptionKey;
    m_inTransaction = false;

    // Open the database
    int result = sqlite3_open(dbPath.c_str(), &m_db);
    if (result != SQLITE_OK) {
        std::string error = "Failed to open database: " + std::string(sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return DatabaseResult(false, error, result);
    }

    // Set the encryption key (only available with SQLCipher)
#ifdef SQLCIPHER_AVAILABLE
    result = sqlite3_key(m_db, encryptionKey.c_str(), static_cast<int>(encryptionKey.length()));
    if (result != SQLITE_OK) {
        std::string error = "Failed to set encryption key: " + std::string(sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return DatabaseResult(false, error, result);
    }
#else
    // Regular SQLite - store encryption key for potential future use but don't apply it
    (void)encryptionKey; // Suppress unused parameter warning
    std::cout << "Warning: Using regular SQLite without encryption (SQLCipher not available)" << std::endl;
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

        sqlite3_close(m_db);
        m_db = nullptr;
    }

    m_initialized = false;
    m_encryptionKey.clear(); // Clear encryption key from memory
}

bool DatabaseManager::isInitialized() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_initialized && m_db != nullptr;
}

DatabaseResult DatabaseManager::executeQuery(const std::string& sql,
                                            std::function<void(sqlite3*)> callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!m_initialized || !m_db) {
        return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
    }

    char* errorMsg = nullptr;
    int result = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errorMsg);

    if (result != SQLITE_OK) {
        std::string error = "SQL execution failed: ";
        if (errorMsg) {
            error += errorMsg;
            sqlite3_free(errorMsg);
        } else {
            error += sqlite3_errmsg(m_db);
        }
        return DatabaseResult(false, error, result);
    }

    // Execute callback if provided
    if (callback) {
        callback(m_db);
    }

    return DatabaseResult(true, "Query executed successfully");
}

DatabaseResult DatabaseManager::executeQuery(const std::string& sql,
                                            const std::vector<std::string>& params,
                                            std::function<void(sqlite3*)> callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!m_initialized || !m_db) {
        return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
    }

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        std::string error = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        result = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_STATIC);
        if (result != SQLITE_OK) {
            sqlite3_finalize(stmt);
            std::string error = "Failed to bind parameter " + std::to_string(i + 1) + ": " +
                              std::string(sqlite3_errmsg(m_db));
            return DatabaseResult(false, error, result);
        }
    }

    // Execute the statement
    result = sqlite3_step(stmt);

    // Handle different result types
    if (result != SQLITE_DONE && result != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::string error = "Statement execution failed: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    // Execute callback if provided
    if (callback) {
        callback(m_db);
    }

    sqlite3_finalize(stmt);
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

    char* errorMsg = nullptr;
    int result = sqlite3_exec(m_db, "BEGIN IMMEDIATE;", nullptr, nullptr, &errorMsg);

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

    char* errorMsg = nullptr;
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

    char* errorMsg = nullptr;
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

    if (!m_initialized || !m_db) {
        return -1;
    }

    sqlite3_stmt* stmt = nullptr;
    std::string sql = "SELECT version FROM " + std::string(SCHEMA_VERSION_TABLE) + " LIMIT 1;";
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

    if (!m_initialized || !m_db) {
        return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
    }

    std::string sql = "INSERT OR REPLACE INTO " + std::string(SCHEMA_VERSION_TABLE) +
                     " (id, version) VALUES (1, ?);";

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        std::string error = "Failed to prepare version update: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    result = sqlite3_bind_int(stmt, 1, version);
    if (result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        std::string error = "Failed to bind version parameter: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        std::string error = "Failed to update schema version: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    return DatabaseResult(true, "Schema version updated to " + std::to_string(version));
}

DatabaseResult DatabaseManager::runMigrations(const std::vector<Migration>& migrations) {
    int currentVersion = getSchemaVersion();

    for (const auto& migration : migrations) {
        if (migration.version <= currentVersion) {
            continue; // Skip already applied migrations
        }

        std::cout << "Applying migration " << migration.version << ": " << migration.description << std::endl;

        auto transactionResult = beginTransaction();
        if (!transactionResult) {
            return transactionResult;
        }

        auto migrationResult = executeQuery(migration.sql);
        if (!migrationResult) {
            rollbackTransaction();
            return DatabaseResult(false, "Migration " + std::to_string(migration.version) +
                                " failed: " + migrationResult.message, migrationResult.errorCode);
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

DatabaseResult DatabaseManager::createBackup(const std::string& backupPath) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!m_initialized || !m_db) {
        return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
    }

    sqlite3* backupDb = nullptr;
    int result = sqlite3_open(backupPath.c_str(), &backupDb);

    if (result != SQLITE_OK) {
        std::string error = "Failed to create backup database: " + std::string(sqlite3_errmsg(backupDb));
        if (backupDb) sqlite3_close(backupDb);
        return DatabaseResult(false, error, result);
    }

    sqlite3_backup* backup = sqlite3_backup_init(backupDb, "main", m_db, "main");
    if (!backup) {
        std::string error = "Failed to initialize backup: " + std::string(sqlite3_errmsg(backupDb));
        sqlite3_close(backupDb);
        return DatabaseResult(false, error, SQLITE_ERROR);
    }

    result = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(backupDb);

    if (result != SQLITE_DONE) {
        std::string error = "Backup failed: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    return DatabaseResult(true, "Backup created successfully at " + backupPath);
}

DatabaseResult DatabaseManager::verifyIntegrity() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!m_initialized || !m_db) {
        return DatabaseResult(false, "Database not initialized", SQLITE_MISUSE);
    }

    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, "PRAGMA integrity_check;", -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        std::string error = "Failed to prepare integrity check: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    result = sqlite3_step(stmt);
    std::string integrityResult;

    if (result == SQLITE_ROW) {
        const char* resultText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (resultText) {
            integrityResult = resultText;
        }
    }

    sqlite3_finalize(stmt);

    if (integrityResult == "ok") {
        return DatabaseResult(true, "Database integrity verified");
    } else {
        return DatabaseResult(false, "Database integrity check failed: " + integrityResult, SQLITE_CORRUPT);
    }
}

sqlite3* DatabaseManager::getHandle() {
    return m_db;
}

DatabaseResult DatabaseManager::createInitialSchema() {
    // Check if schema_version table exists
    sqlite3_stmt* stmt = nullptr;
    std::string checkSql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" +
                          std::string(SCHEMA_VERSION_TABLE) + "';";

    int result = sqlite3_prepare_v2(m_db, checkSql.c_str(), -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        std::string error = "Failed to check for schema_version table: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    result = sqlite3_step(stmt);
    bool tableExists = (result == SQLITE_ROW);
    sqlite3_finalize(stmt);

    if (!tableExists) {
        // Create schema_version table
        std::string createVersionTable =
            "CREATE TABLE " + std::string(SCHEMA_VERSION_TABLE) + " ("
            "id INTEGER PRIMARY KEY CHECK (id = 1), "
            "version INTEGER NOT NULL, "
            "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ");";

        char* errorMsg = nullptr;
        result = sqlite3_exec(m_db, createVersionTable.c_str(), nullptr, nullptr, &errorMsg);

        if (result != SQLITE_OK) {
            std::string error = "Failed to create schema_version table: ";
            if (errorMsg) {
                error += errorMsg;
                sqlite3_free(errorMsg);
            }
            return DatabaseResult(false, error, result);
        }

        // Set initial version (internal call without lock)
        std::string sql = "INSERT OR REPLACE INTO " + std::string(SCHEMA_VERSION_TABLE) +
                         " (id, version) VALUES (1, ?);";

        sqlite3_stmt* stmt = nullptr;
        int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

        if (result != SQLITE_OK) {
            std::string error = "Failed to prepare version update: " + std::string(sqlite3_errmsg(m_db));
            return DatabaseResult(false, error, result);
        }

        result = sqlite3_bind_int(stmt, 1, 0);
        if (result != SQLITE_OK) {
            sqlite3_finalize(stmt);
            std::string error = "Failed to bind version parameter: " + std::string(sqlite3_errmsg(m_db));
            return DatabaseResult(false, error, result);
        }

        result = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (result != SQLITE_DONE) {
            std::string error = "Failed to set initial schema version: " + std::string(sqlite3_errmsg(m_db));
            return DatabaseResult(false, error, result);
        }
    }

    return DatabaseResult(true, "Initial schema created successfully");
}

DatabaseResult DatabaseManager::validateEncryption() {
    // Try to read from sqlite_master to verify encryption is working
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM sqlite_master;", -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        std::string error = "Database encryption validation failed: " + std::string(sqlite3_errmsg(m_db));
        return DatabaseResult(false, error, result);
    }

    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (result != SQLITE_ROW) {
        return DatabaseResult(false, "Failed to validate database encryption", result);
    }

    return DatabaseResult(true, "Database encryption validated");
}

DatabaseResult DatabaseManager::setupPragmas() {
    // Enable foreign key constraints
    auto result = executeQuery("PRAGMA foreign_keys = ON;");
    if (!result) return result;

    // Set secure delete to overwrite deleted data
    result = executeQuery("PRAGMA secure_delete = ON;");
    if (!result) return result;

    // Set WAL mode for better concurrency
    result = executeQuery("PRAGMA journal_mode = WAL;");
    if (!result) return result;

    // Set synchronous mode for data safety
    result = executeQuery("PRAGMA synchronous = FULL;");
    if (!result) return result;

    // Set cache size for performance (negative value = KB)
    result = executeQuery("PRAGMA cache_size = -64000;"); // 64MB cache
    if (!result) return result;

    return DatabaseResult(true, "Database pragmas configured successfully");
}

// TransactionGuard implementation
TransactionGuard::TransactionGuard(DatabaseManager& db) : m_db(db), m_committed(false) {
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

} // namespace Database
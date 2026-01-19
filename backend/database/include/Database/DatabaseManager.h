#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>
#include <cstdint>

// Forward declaration to avoid including SQLCipher headers in the header
struct sqlite3;

namespace Database {

/**
 * @brief Result structure for database operations
 */
struct DatabaseResult {
    bool success;
    std::string message;
    int errorCode;

    DatabaseResult(bool s = false, const std::string& msg = "", int errCode = 0)
        : success(s), message(msg), errorCode(errCode) {}

    operator bool() const { return success; }
};

/**
 * @brief Database migration information
 */
struct Migration {
    int version;
    std::string description;
    std::string sql;

    Migration(int v, const std::string& desc, const std::string& query)
        : version(v), description(desc), sql(query) {}
};

/**
 * @brief Secure database manager using SQLCipher for cryptocurrency wallet data
 *
 * This class provides:
 * - Encrypted database storage using SQLCipher
 * - Connection pooling and management
 * - Schema versioning and migrations
 * - Transaction management
 * - Thread-safe operations
 */
class DatabaseManager {
public:
    /**
     * @brief Get the singleton instance of DatabaseManager
     * @return Reference to the singleton instance
     */
    static DatabaseManager& getInstance();

    /**
     * @brief Initialize the database with encryption key
     * @param dbPath Path to the database file
     * @param encryptionKey Encryption key for SQLCipher
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult initialize(const std::string& dbPath, const std::string& encryptionKey);

    /**
     * @brief Close the database connection
     */
    void close();

    /**
     * @brief Check if database is initialized and connected
     * @return true if database is ready for operations
     */
    bool isInitialized() const;

    /**
     * @brief Execute a SQL query
     * @param sql SQL query to execute
     * @param callback Optional callback function for result processing
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult executeQuery(const std::string& sql,
                                std::function<void(sqlite3*)> callback = nullptr);

    /**
     * @brief Execute a SQL query with parameters (prepared statement)
     * @param sql SQL query with placeholders
     * @param params Parameters to bind to the query
     * @param callback Optional callback function for result processing
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult executeQuery(const std::string& sql,
                                const std::vector<std::string>& params,
                                std::function<void(sqlite3*)> callback = nullptr);

    /**
     * @brief Begin a database transaction
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult beginTransaction();

    /**
     * @brief Commit the current transaction
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult commitTransaction();

    /**
     * @brief Rollback the current transaction
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult rollbackTransaction();

    /**
     * @brief Get the current database schema version
     * @return Schema version number, -1 if error
     */
    int getSchemaVersion();

    /**
     * @brief Set the database schema version
     * @param version New schema version
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult setSchemaVersion(int version);

    /**
     * @brief Run database migrations to upgrade schema
     * @param migrations Vector of migrations to apply
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult runMigrations(const std::vector<Migration>& migrations);

    /**
     * @brief Create a backup of the database
     * @param backupPath Path where backup should be created
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult createBackup(const std::string& backupPath);

    /**
     * @brief Verify database integrity
     * @return DatabaseResult indicating integrity status
     */
    DatabaseResult verifyIntegrity();

    /**
     * @brief Get the SQLite handle (use with caution)
     * @return Raw SQLite3 database handle
     */
    sqlite3* getHandle();

    /**
     * @brief Change the database encryption key
     * @param newKey New encryption key (must be at least 32 characters)
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult changeEncryptionKey(const std::string& newKey);

    /**
     * @brief Get audit log entries
     * @param maxEntries Maximum number of entries to return (0 for all)
     * @return Vector of audit log entries
     */
    std::vector<std::string> getAuditLog(size_t maxEntries = 0) const;

private:
    DatabaseManager() : m_db(nullptr), m_initialized(false),
                       m_inTransaction(false), m_connectionAttempts(0) {}
    ~DatabaseManager();

    // Prevent copying
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    /**
     * @brief Create the initial database schema
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult createInitialSchema();

    /**
     * @brief Validate that the database is properly encrypted
     * @return DatabaseResult indicating validation status
     */
    DatabaseResult validateEncryption();

    /**
     * @brief Setup database pragmas for security and performance
     * @return DatabaseResult indicating success or failure
     */
    DatabaseResult setupPragmas();

    /**
     * @brief Check if SQL contains dangerous patterns
     * @param sql SQL string to check
     * @return true if dangerous patterns are found
     */
    bool containsDangerousSQL(const std::string& sql);

    /**
     * @brief Check database connection health
     * @return DatabaseResult indicating connection status
     */
    DatabaseResult checkConnectionHealth();

    /**
     * @brief Verify backup database integrity
     * @param backupDb Backup database handle
     * @return DatabaseResult indicating integrity status
     */
    DatabaseResult verifyBackupIntegrity(sqlite3* backupDb);

    /**
     * @brief Log database operations for audit trail
     * @param operation Operation type
     * @param details Operation details
     * @param error Error message if any
     */
    void logDatabaseOperation(const std::string& operation,
                             const std::string& details,
                             const std::string& error);

private:
    sqlite3* m_db;
    std::string m_dbPath;
    std::string m_encryptionKey;                       // Encryption key (will be securely cleared)
    std::vector<uint8_t> m_keyDerivationSalt;          // Salt for key derivation
    bool m_initialized;
    mutable std::recursive_mutex m_mutex;
    bool m_inTransaction;

    // Connection health monitoring
    int m_connectionAttempts;
    std::chrono::steady_clock::time_point m_lastConnectionTime;

    // Audit logging
    mutable std::recursive_mutex m_auditMutex;
    std::vector<std::string> m_auditLog;

    // Constants
    static constexpr int CURRENT_SCHEMA_VERSION = 1;
    static constexpr const char* SCHEMA_VERSION_TABLE = "schema_version";
};

/**
 * @brief RAII transaction guard for automatic rollback on exception
 */
class TransactionGuard {
public:
    explicit TransactionGuard(DatabaseManager& db);
    ~TransactionGuard();

    /**
     * @brief Commit the transaction (prevents automatic rollback)
     */
    void commit();

private:
    DatabaseManager& m_db;
    bool m_committed;
};

} // namespace Database

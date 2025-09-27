#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

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

    DatabaseResult(bool s = false, const std::string& msg = "", int code = 0)
        : success(s), message(msg), errorCode(code) {}

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

private:
    DatabaseManager() = default;
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

private:
    sqlite3* m_db;
    std::string m_dbPath;
    std::string m_encryptionKey;
    bool m_initialized;
    mutable std::recursive_mutex m_mutex;
    bool m_inTransaction;

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
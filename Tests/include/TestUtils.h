/**
 * @file TestUtils.h
 * @brief Shared testing utilities for repository unit tests
 *
 * Provides common macros, color codes, schema definitions, and helper functions
 * for all repository test suites.
 */

#pragma once

#include "Database/DatabaseManager.h"
#include "Repository/UserRepository.h"
#include "Repository/WalletRepository.h"
#include "Repository/TransactionRepository.h"
#include "Repository/Logger.h"
#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
// ANSI Color Codes
// ============================================================================

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

// ============================================================================
// Global Test Counters
// ============================================================================

namespace TestGlobals {
    extern int g_testsRun;
    extern int g_testsPassed;
    extern int g_testsFailed;
}

// ============================================================================
// Test Macros
// ============================================================================

#define TEST_START(name) \
    do { \
        std::cout << COLOR_BLUE << "[TEST] " << name << COLOR_RESET << std::endl; \
        TestGlobals::g_testsRun++; \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cout << COLOR_RED << "  ✗ FAILED: " << message << COLOR_RESET << std::endl; \
            TestGlobals::g_testsFailed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        std::cout << COLOR_GREEN << "  ✓ PASSED" << COLOR_RESET << std::endl; \
        TestGlobals::g_testsPassed++; \
        return true; \
    } while(0)

#define TEST_STEP(message) \
    do { \
        std::cout << "  → " << message << std::endl; \
    } while(0)

// ============================================================================
// Standard Test Encryption Key
// ============================================================================

constexpr const char* STANDARD_TEST_ENCRYPTION_KEY = "test-encryption-key-32-chars!!##";

// ============================================================================
// Database Schema Creation Functions
// ============================================================================

namespace TestUtils {

/**
 * @brief Creates the standard test database schema with all tables
 * @param dbManager Database manager instance
 * @return true if schema creation succeeded, false otherwise
 */
inline bool createFullSchema(Database::DatabaseManager& dbManager) {
    const std::string schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            email TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            salt BLOB NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_login TIMESTAMP,
            wallet_version INTEGER DEFAULT 1,
            is_active INTEGER DEFAULT 1
        );

        CREATE TABLE IF NOT EXISTS wallets (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            wallet_name TEXT NOT NULL,
            wallet_type TEXT NOT NULL DEFAULT 'bitcoin',
            derivation_path TEXT,
            extended_public_key TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            is_active INTEGER DEFAULT 1,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS addresses (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            wallet_id INTEGER NOT NULL,
            address TEXT NOT NULL UNIQUE,
            address_index INTEGER NOT NULL,
            is_change INTEGER DEFAULT 0,
            public_key TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            label TEXT,
            balance_satoshis INTEGER DEFAULT 0,
            FOREIGN KEY (wallet_id) REFERENCES wallets(id)
        );

        CREATE TABLE IF NOT EXISTS transactions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            wallet_id INTEGER NOT NULL,
            txid TEXT NOT NULL UNIQUE,
            block_height INTEGER,
            block_hash TEXT,
            amount_satoshis INTEGER NOT NULL,
            fee_satoshis INTEGER DEFAULT 0,
            direction TEXT NOT NULL,
            from_address TEXT,
            to_address TEXT,
            confirmation_count INTEGER DEFAULT 0,
            is_confirmed INTEGER DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            confirmed_at TIMESTAMP,
            memo TEXT,
            FOREIGN KEY (wallet_id) REFERENCES wallets(id)
        );

        CREATE TABLE IF NOT EXISTS encrypted_seeds (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL UNIQUE,
            encrypted_seed BLOB NOT NULL,
            encryption_salt BLOB NOT NULL,
            verification_hash BLOB NOT NULL,
            key_derivation_iterations INTEGER DEFAULT 600000,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            backup_confirmed INTEGER DEFAULT 0,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );

        CREATE TABLE IF NOT EXISTS transaction_outputs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            transaction_id INTEGER NOT NULL,
            output_index INTEGER NOT NULL,
            script_pubkey TEXT,
            address TEXT,
            amount_satoshis INTEGER NOT NULL,
            is_spent INTEGER DEFAULT 0,
            spent_in_txid TEXT,
            FOREIGN KEY (transaction_id) REFERENCES transactions(id)
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
        CREATE INDEX IF NOT EXISTS idx_tx_outputs_address_spent ON transaction_outputs(address, is_spent);
        CREATE INDEX IF NOT EXISTS idx_tx_outputs_tx_spent ON transaction_outputs(transaction_id, is_spent);
    )";

    auto result = dbManager.executeQuery(schema);
    if (!result.success) {
        std::cerr << COLOR_RED << "Failed to create schema: " << result.message << COLOR_RESET << std::endl;
        std::cerr << COLOR_RED << "Error code: " << result.errorCode << COLOR_RESET << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Cleans up test database files
 * @param dbPath Path to the database file
 */
inline void cleanupTestDatabase(const std::string& dbPath) {
    try {
        if (fs::exists(dbPath)) fs::remove(dbPath);
        if (fs::exists(dbPath + "-shm")) fs::remove(dbPath + "-shm");
        if (fs::exists(dbPath + "-wal")) fs::remove(dbPath + "-wal");
    } catch (...) {}
}

/**
 * @brief Initializes a test database environment
 * @param dbManager Database manager instance
 * @param dbPath Path to the test database
 * @param encryptionKey Encryption key for the database
 * @return true if initialization succeeded, false otherwise
 */
inline bool initializeTestDatabase(Database::DatabaseManager& dbManager,
                                   const std::string& dbPath,
                                   const std::string& encryptionKey) {
    cleanupTestDatabase(dbPath);

    // Close any previous database connection first
    dbManager.close();

    auto dbResult = dbManager.initialize(dbPath, encryptionKey);
    if (!dbResult) {
        std::cerr << COLOR_RED << "Failed to initialize database: " << dbResult.message << COLOR_RESET << std::endl;
        return false;
    }

    return createFullSchema(dbManager);
}

/**
 * @brief Helper function to create a test user
 * @param userRepo User repository instance
 * @param username Username for the test user
 * @param password Password for the test user
 * @return User ID if successful, -1 otherwise
 */
inline int createTestUser(Repository::UserRepository& userRepo,
                         const std::string& username,
                         const std::string& password = "SecurePass123!") {
    auto result = userRepo.createUser(username, username + "@example.com", password);
    return result.hasValue() ? result->id : -1;
}

/**
 * @brief Helper function to create a test wallet
 * @param walletRepo Wallet repository instance
 * @param userId User ID for the wallet owner
 * @param walletName Name for the wallet
 * @param walletType Type of wallet (default: "bitcoin")
 * @return Wallet ID if successful, -1 otherwise
 */
inline int createTestWallet(Repository::WalletRepository& walletRepo,
                           int userId,
                           const std::string& walletName,
                           const std::string& walletType = "bitcoin") {
    auto result = walletRepo.createWallet(userId, walletName, walletType);
    return result.hasValue() ? result->id : -1;
}

/**
 * @brief Helper function to create a test user with wallet
 * @param userRepo User repository instance
 * @param walletRepo Wallet repository instance
 * @param username Username for the test user
 * @param walletName Name for the wallet
 * @return Wallet ID if successful, -1 otherwise
 */
inline int createTestUserWithWallet(Repository::UserRepository& userRepo,
                                   Repository::WalletRepository& walletRepo,
                                   const std::string& username,
                                   const std::string& walletName = "Test Wallet") {
    int userId = createTestUser(userRepo, username);
    if (userId < 0) return -1;

    return createTestWallet(walletRepo, userId, walletName);
}

/**
 * @brief Prints test summary statistics
 * @param suiteName Name of the test suite
 */
inline void printTestSummary(const std::string& suiteName) {
    std::cout << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "  " << suiteName << " Summary" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << "Total tests run:    " << TestGlobals::g_testsRun << std::endl;
    std::cout << COLOR_GREEN << "Tests passed:       " << TestGlobals::g_testsPassed << COLOR_RESET << std::endl;

    if (TestGlobals::g_testsFailed > 0) {
        std::cout << COLOR_RED << "Tests failed:       " << TestGlobals::g_testsFailed << COLOR_RESET << std::endl;
        std::cout << COLOR_RED << "✗ Some tests failed" << COLOR_RESET << std::endl;
    } else {
        std::cout << "Tests failed:       " << TestGlobals::g_testsFailed << std::endl;
        std::cout << COLOR_GREEN << "✓ All tests passed!" << COLOR_RESET << std::endl;
    }
}

/**
 * @brief Prints test suite header
 * @param suiteName Name of the test suite
 */
inline void printTestHeader(const std::string& suiteName) {
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "  " << suiteName << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl << std::endl;
}

/**
 * @brief Initializes the logger for testing
 * @param logPath Path to the log file
 */
inline void initializeTestLogger(const std::string& logPath) {
    Repository::Logger::getInstance().initialize(logPath, Repository::LogLevel::DEBUG, false);
}

/**
 * @brief Shuts down logger and cleans up test environment
 * @param dbManager Database manager instance
 * @param dbPath Path to the test database
 */
inline void shutdownTestEnvironment(Database::DatabaseManager& dbManager, const std::string& dbPath) {
    dbManager.close();
    Repository::Logger::getInstance().shutdown();
    cleanupTestDatabase(dbPath);
}

} // namespace TestUtils

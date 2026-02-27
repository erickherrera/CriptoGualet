#include "TestUtils.h"
#include "Database/DatabaseManager.h"
#include "Repository/Logger.h"
#include "Repository/UserRepository.h"
#include "Repository/WalletRepository.h"
#include <filesystem>
#include <iostream>
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

// This file can be used for shared test utilities.

// Global test counters
namespace TestGlobals {
int g_testsRun = 0;
int g_testsPassed = 0;
int g_testsFailed = 0;
}  // namespace TestGlobals

namespace TestUtils {

bool createFullSchema(Database::DatabaseManager& dbManager);

void printTestHeader(const std::string& title) {
    std::cout << "\n"
              << COLOR_CYAN << "==================================================" << COLOR_RESET
              << std::endl;
    std::cout << COLOR_CYAN << " " << title << COLOR_RESET << std::endl;
    std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET
              << std::endl;
}

void initializeTestLogger(const std::string& logFileName) {
    Repository::Logger::getInstance().initialize(logFileName);
}

void initializeTestDatabase(Database::DatabaseManager& dbManager, const std::string& dbPath,
                            const std::string& encryptionKey) {
    if (std::filesystem::exists(dbPath)) {
        std::filesystem::remove(dbPath);
    }

    auto initResult = dbManager.initialize(dbPath, encryptionKey);
    if (!initResult.success) {
        std::cout << COLOR_RED << "[TEST ERROR] Database init failed: " << initResult.message
                  << COLOR_RESET << std::endl;
        return;
    }
    createFullSchema(dbManager);
}

bool createFullSchema(Database::DatabaseManager& dbManager) {
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
                backup_codes TEXT,
                encrypted_seed TEXT
            );

            CREATE TABLE IF NOT EXISTS user_seeds (
                username TEXT PRIMARY KEY,
                encrypted_seed TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
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

            CREATE TABLE IF NOT EXISTS transaction_inputs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                transaction_id INTEGER NOT NULL,
                input_index INTEGER NOT NULL,
                prev_txid TEXT NOT NULL,
                prev_output_index INTEGER NOT NULL,
                script_sig TEXT,
                sequence INTEGER DEFAULT 4294967295,
                address TEXT,
                amount_satoshis INTEGER,
                FOREIGN KEY (transaction_id) REFERENCES transactions(id),
                UNIQUE (transaction_id, input_index)
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
                FOREIGN KEY (transaction_id) REFERENCES transactions(id),
                UNIQUE (transaction_id, output_index)
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
            CREATE INDEX IF NOT EXISTS idx_transaction_inputs_txid ON transaction_inputs(transaction_id);
            CREATE INDEX IF NOT EXISTS idx_transaction_outputs_txid ON transaction_outputs(transaction_id);
            CREATE INDEX IF NOT EXISTS idx_erc20_tokens_wallet_contract ON erc20_tokens(wallet_id, contract_address);
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
        std::cout << COLOR_RED << "[TEST ERROR] Schema init failed: " << schemaResult.message
                  << COLOR_RESET << std::endl;
        return false;
    }
    return true;
}

void shutdownTestEnvironment(Database::DatabaseManager& dbManager, const std::string& dbPath) {
    dbManager.close();
    if (std::filesystem::exists(dbPath)) {
        // In case of test failures, you might want to inspect the database.
        // For clean runs, uncomment the line below.
        // std::filesystem::remove(dbPath);
    }
}

void printTestSummary(const std::string& testSuiteName) {
    std::cout << "\n"
              << COLOR_BLUE << "---------- Test Summary for " << testSuiteName << " ----------"
              << COLOR_RESET << std::endl;
    std::cout << COLOR_GREEN << "  PASSED: " << TestGlobals::g_testsPassed << COLOR_RESET
              << std::endl;
    std::cout << COLOR_RED << "  FAILED: " << TestGlobals::g_testsFailed << COLOR_RESET
              << std::endl;
    std::cout << COLOR_BLUE << "  TOTAL:  " << TestGlobals::g_testsRun << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "----------------------------------------------------" << COLOR_RESET
              << std::endl;
}

std::string getWritableTestPath(const std::string& filename) {
    try {
        std::filesystem::path testDir;
#ifdef _WIN32
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData) {
            testDir = std::filesystem::path(localAppData) / "CriptoGualet" / "Tests";
        } else {
            testDir = std::filesystem::temp_directory_path() / "CriptoGualetTests";
        }
#else
        testDir = std::filesystem::temp_directory_path() / "CriptoGualetTests";
#endif
        std::error_code ec;
        if (!std::filesystem::exists(testDir, ec)) {
            std::filesystem::create_directories(testDir, ec);
        }

        // If we still can't create/access the directory, fallback to current dir
        // (though this is what failed in the first place)
        if (ec) {
            return filename;
        }

        return (testDir / filename).string();
    } catch (...) {
        return filename;  // Ultimate fallback
    }
}

int createTestUser(Repository::UserRepository& userRepo, const std::string& username) {
    auto userResult = userRepo.createUser(username, "SecurePass123!");
    if (!userResult.hasValue()) {
        return -1;
    }
    return userResult->id;
}

int createTestUserWithWallet(Repository::UserRepository& userRepo,
                             Repository::WalletRepository& walletRepo,
                             const std::string& username) {
    auto userResult = userRepo.createUser(username, "SecurePass123!");
    if (!userResult.hasValue()) {
        return -1;
    }
    auto walletResult = walletRepo.createWallet(userResult->id, "My Test Wallet", "bitcoin");
    if (!walletResult.hasValue()) {
        return -1;
    }
    return walletResult->id;
}

void waitForUser() {
    // Check if we are running in an interactive terminal
    bool isInteractive = false;
#ifdef _WIN32
    isInteractive = _isatty(_fileno(stdin));
#else
    isInteractive = isatty(fileno(stdin));
#endif

    // Also check for CI environment variable
    if (std::getenv("CI") || std::getenv("GITHUB_ACTIONS")) {
        isInteractive = false;
    }

    if (isInteractive) {
        std::cout << "\nPress Enter to exit..." << std::endl;
        std::cin.get();
    } else {
        std::cout << "\nNon-interactive environment detected, skipping wait." << std::endl;
    }
}
}  // namespace TestUtils

// Mock time management
std::chrono::steady_clock::time_point MockTime::mockTime_;
bool MockTime::useMockTime_ = false;

void MockTime::enable() {
    useMockTime_ = true;
    mockTime_ = std::chrono::steady_clock::now();
}

void MockTime::disable() {
    useMockTime_ = false;
}

std::chrono::steady_clock::time_point MockTime::now() {
    return useMockTime_ ? mockTime_ : std::chrono::steady_clock::now();
}

void MockTime::advance(std::chrono::minutes minutes) {
    mockTime_ += minutes;
}

void MockTime::reset() {
    mockTime_ = std::chrono::steady_clock::now();
    useMockTime_ = false;
}
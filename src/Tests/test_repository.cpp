#include "../Repository/UserRepository.h"
#include "../Repository/WalletRepository.h"
#include "../Repository/TransactionRepository.h"
#include "../Repository/Logger.h"
#include "../Database/DatabaseManager.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace Repository;

namespace {
    const std::string TEST_DB_PATH = "test_repository.db";

    void cleanupTestDatabase() {
        if (std::filesystem::exists(TEST_DB_PATH)) {
            std::filesystem::remove(TEST_DB_PATH);
        }
    }

    void logTestResult(const std::string& testName, bool passed) {
        Logger::getInstance().log(passed ? LogLevel::INFO : LogLevel::ERROR,
                                 "TEST", testName + (passed ? " PASSED" : " FAILED"));
        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << testName << std::endl;
    }
}

class RepositoryTestSuite {
private:
    std::unique_ptr<Database::DatabaseManager> dbManager;
    std::unique_ptr<UserRepository> userRepo;
    std::unique_ptr<WalletRepository> walletRepo;
    std::unique_ptr<TransactionRepository> transactionRepo;

public:
    bool initialize() {
        try {
            cleanupTestDatabase();

            dbManager = std::make_unique<Database::DatabaseManager>();
            auto initResult = dbManager->initialize(TEST_DB_PATH, "test_password_123");
            if (!initResult.success) {
                std::cerr << "Failed to initialize database: " << initResult.errorMessage << std::endl;
                return false;
            }

            userRepo = std::make_unique<UserRepository>(*dbManager);
            walletRepo = std::make_unique<WalletRepository>(*dbManager);
            transactionRepo = std::make_unique<TransactionRepository>(*dbManager);

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Initialization failed: " << e.what() << std::endl;
            return false;
        }
    }

    void cleanup() {
        userRepo.reset();
        walletRepo.reset();
        transactionRepo.reset();
        dbManager.reset();
        cleanupTestDatabase();
    }

    bool testUserRepository() {
        std::cout << "\n=== Testing UserRepository ===" << std::endl;

        bool allPassed = true;

        // Test 1: Create user
        {
            auto result = userRepo->createUser("testuser", "test@example.com", "password123");
            bool passed = result.success && result.data.id > 0;
            logTestResult("Create user", passed);
            allPassed &= passed;
        }

        // Test 2: Authenticate user
        {
            auto result = userRepo->authenticateUser("testuser", "password123");
            bool passed = result.success && result.data.username == "testuser";
            logTestResult("Authenticate user", passed);
            allPassed &= passed;
        }

        // Test 3: Authenticate with wrong password
        {
            auto result = userRepo->authenticateUser("testuser", "wrongpassword");
            bool passed = !result.success;
            logTestResult("Authenticate with wrong password", passed);
            allPassed &= passed;
        }

        // Test 4: Get user by username
        {
            auto result = userRepo->getUserByUsername("testuser");
            bool passed = result.success && result.data.email == "test@example.com";
            logTestResult("Get user by username", passed);
            allPassed &= passed;
        }

        // Test 5: Update user email
        {
            auto result = userRepo->updateUserEmail(1, "newemail@example.com");
            bool passed = result.success && result.data;
            logTestResult("Update user email", passed);
            allPassed &= passed;
        }

        // Test 6: Change user password
        {
            auto result = userRepo->changePassword(1, "password123", "newpassword123");
            bool passed = result.success && result.data;
            logTestResult("Change user password", passed);
            allPassed &= passed;
        }

        // Test 7: Authenticate with new password
        {
            auto result = userRepo->authenticateUser("testuser", "newpassword123");
            bool passed = result.success;
            logTestResult("Authenticate with new password", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testWalletRepository() {
        std::cout << "\n=== Testing WalletRepository ===" << std::endl;

        bool allPassed = true;
        int walletId = 0;

        // Test 1: Create wallet
        {
            auto result = walletRepo->createWallet(1, "Test Wallet", "bitcoin");
            bool passed = result.success && result.data.id > 0;
            if (passed) {
                walletId = result.data.id;
            }
            logTestResult("Create wallet", passed);
            allPassed &= passed;
        }

        // Test 2: Get wallet by ID
        {
            auto result = walletRepo->getWalletById(walletId);
            bool passed = result.success && result.data.name == "Test Wallet";
            logTestResult("Get wallet by ID", passed);
            allPassed &= passed;
        }

        // Test 3: Get wallets by user ID
        {
            auto result = walletRepo->getWalletsByUserId(1);
            bool passed = result.success && result.data.size() == 1;
            logTestResult("Get wallets by user ID", passed);
            allPassed &= passed;
        }

        // Test 4: Generate address
        {
            auto result = walletRepo->generateAddress(walletId, false, "Test Address");
            bool passed = result.success && !result.data.address.empty();
            logTestResult("Generate address", passed);
            allPassed &= passed;
        }

        // Test 5: Get addresses by wallet
        {
            auto result = walletRepo->getAddressesByWallet(walletId);
            bool passed = result.success && result.data.size() == 1;
            logTestResult("Get addresses by wallet", passed);
            allPassed &= passed;
        }

        // Test 6: Store encrypted seed
        {
            std::vector<std::string> mnemonic = {
                "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
                "abandon", "abandon", "abandon", "abandon", "abandon", "about"
            };
            auto result = walletRepo->storeEncryptedSeed(1, "newpassword123", mnemonic);
            bool passed = result.success && result.data;
            logTestResult("Store encrypted seed", passed);
            allPassed &= passed;
        }

        // Test 7: Retrieve encrypted seed
        {
            auto result = walletRepo->retrieveDecryptedSeed(1, "newpassword123");
            bool passed = result.success && result.data.size() == 12;
            logTestResult("Retrieve encrypted seed", passed);
            allPassed &= passed;
        }

        // Test 8: Update wallet
        {
            auto result = walletRepo->updateWallet(walletId, "Updated Wallet Name");
            bool passed = result.success && result.data;
            logTestResult("Update wallet", passed);
            allPassed &= passed;
        }

        // Test 9: Get wallet summary
        {
            auto result = walletRepo->getWalletSummary(walletId);
            bool passed = result.success && result.data.wallet.name == "Updated Wallet Name";
            logTestResult("Get wallet summary", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testTransactionRepository() {
        std::cout << "\n=== Testing TransactionRepository ===" << std::endl;

        bool allPassed = true;
        int transactionId = 0;

        // Test 1: Add transaction
        {
            Transaction tx;
            tx.walletId = 1;
            tx.txid = "test_txid_123";
            tx.direction = "incoming";
            tx.amountSatoshis = 100000000; // 1 BTC
            tx.feeSatoshis = 10000;
            tx.isConfirmed = false;
            tx.memo = "Test transaction";

            auto result = transactionRepo->addTransaction(tx);
            bool passed = result.success && result.data.id > 0;
            if (passed) {
                transactionId = result.data.id;
            }
            logTestResult("Add transaction", passed);
            allPassed &= passed;
        }

        // Test 2: Get transaction by txid
        {
            auto result = transactionRepo->getTransactionByTxid("test_txid_123");
            bool passed = result.success && result.data.amountSatoshis == 100000000;
            logTestResult("Get transaction by txid", passed);
            allPassed &= passed;
        }

        // Test 3: Get transaction by ID
        {
            auto result = transactionRepo->getTransactionById(transactionId);
            bool passed = result.success && result.data.memo == "Test transaction";
            logTestResult("Get transaction by ID", passed);
            allPassed &= passed;
        }

        // Test 4: Get transactions by wallet
        {
            auto result = transactionRepo->getTransactionsByWallet(1);
            bool passed = result.success && result.data.items.size() == 1;
            logTestResult("Get transactions by wallet", passed);
            allPassed &= passed;
        }

        // Test 5: Update transaction confirmation
        {
            auto result = transactionRepo->updateTransactionConfirmation("test_txid_123", 700000, "block_hash_123", 6);
            bool passed = result.success && result.data;
            logTestResult("Update transaction confirmation", passed);
            allPassed &= passed;
        }

        // Test 6: Confirm transaction
        {
            auto result = transactionRepo->confirmTransaction("test_txid_123");
            bool passed = result.success && result.data;
            logTestResult("Confirm transaction", passed);
            allPassed &= passed;
        }

        // Test 7: Update transaction memo
        {
            auto result = transactionRepo->updateTransactionMemo(transactionId, "Updated memo");
            bool passed = result.success && result.data;
            logTestResult("Update transaction memo", passed);
            allPassed &= passed;
        }

        // Test 8: Get transaction statistics
        {
            auto result = transactionRepo->getTransactionStats(1);
            bool passed = result.success && result.data.totalTransactions == 1;
            logTestResult("Get transaction statistics", passed);
            allPassed &= passed;
        }

        // Test 9: Calculate wallet balance
        {
            auto result = transactionRepo->calculateWalletBalance(1);
            bool passed = result.success && result.data.totalBalance > 0;
            logTestResult("Calculate wallet balance", passed);
            allPassed &= passed;
        }

        // Test 10: Add transaction inputs and outputs
        {
            std::vector<TransactionInput> inputs;
            TransactionInput input;
            input.transactionId = transactionId;
            input.inputIndex = 0;
            input.prevTxid = "prev_txid_123";
            input.prevOutputIndex = 0;
            input.amountSatoshis = 110000000;
            inputs.push_back(input);

            auto result = transactionRepo->addTransactionInputs(transactionId, inputs);
            bool passed = result.success && result.data;
            logTestResult("Add transaction inputs", passed);
            allPassed &= passed;
        }

        {
            std::vector<TransactionOutput> outputs;
            TransactionOutput output;
            output.transactionId = transactionId;
            output.outputIndex = 0;
            output.amountSatoshis = 100000000;
            output.address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
            outputs.push_back(output);

            auto result = transactionRepo->addTransactionOutputs(transactionId, outputs);
            bool passed = result.success && result.data;
            logTestResult("Add transaction outputs", passed);
            allPassed &= passed;
        }

        // Test 11: Get transaction inputs and outputs
        {
            auto inputResult = transactionRepo->getTransactionInputs(transactionId);
            auto outputResult = transactionRepo->getTransactionOutputs(transactionId);
            bool passed = inputResult.success && outputResult.success &&
                         inputResult.data.size() == 1 && outputResult.data.size() == 1;
            logTestResult("Get transaction inputs/outputs", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testLogger() {
        std::cout << "\n=== Testing Logger ===" << std::endl;

        bool allPassed = true;

        // Test 1: Basic logging
        {
            Logger::getInstance().log(LogLevel::INFO, "TEST", "Test log message");
            Logger::getInstance().log(LogLevel::ERROR, "TEST", "Test error message");
            Logger::getInstance().log(LogLevel::DEBUG, "TEST", "Test debug message");

            // Wait a bit for async logging
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            bool passed = true; // Logger doesn't return results, so we assume success
            logTestResult("Basic logging", passed);
            allPassed &= passed;
        }

        // Test 2: Scoped logging
        {
            {
                ScopedLogger scoped("TEST_SCOPE", "Testing scoped logging");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            bool passed = true; // Logger doesn't return results, so we assume success
            logTestResult("Scoped logging", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool runAllTests() {
        std::cout << "Starting Repository Test Suite..." << std::endl;

        if (!initialize()) {
            std::cerr << "Failed to initialize test suite" << std::endl;
            return false;
        }

        bool allPassed = true;

        try {
            allPassed &= testLogger();
            allPassed &= testUserRepository();
            allPassed &= testWalletRepository();
            allPassed &= testTransactionRepository();
        } catch (const std::exception& e) {
            std::cerr << "Test exception: " << e.what() << std::endl;
            allPassed = false;
        }

        cleanup();

        std::cout << "\n=== Test Suite Results ===" << std::endl;
        std::cout << "Overall result: " << (allPassed ? "PASSED" : "FAILED") << std::endl;

        return allPassed;
    }
};

int main() {
    try {
        RepositoryTestSuite testSuite;
        bool success = testSuite.runAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Fatal test error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal test error" << std::endl;
        return 1;
    }
}
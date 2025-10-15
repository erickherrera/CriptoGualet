/**
 * @file test_transaction_repository.cpp
 * @brief Unit tests for TransactionRepository
 *
 * Tests transaction storage, retrieval, and balance calculation.
 */

#include "TestUtils.h"
#include "Repository/TransactionRepository.h"
#include "Repository/WalletRepository.h"
#include "Repository/UserRepository.h"
#include "Repository/Logger.h"
#include "Database/DatabaseManager.h"
#include <iostream>

const std::string TEST_DB_PATH = "test_tx_repo.db";

// Helper: Create test wallet with user
int createTestWallet(Repository::UserRepository& userRepo, Repository::WalletRepository& walletRepo,
                     const std::string& username) {
    return TestUtils::createTestUserWithWallet(userRepo, walletRepo, username);
}

// ============================================================================
// Test Cases
// ============================================================================

bool testAddTransaction(Repository::TransactionRepository& txRepo,
                       Repository::WalletRepository& walletRepo,
                       Repository::UserRepository& userRepo) {
    TEST_START("Add Transaction");

    int walletId = createTestWallet(userRepo, walletRepo, "txuser1");
    TEST_ASSERT(walletId > 0, "Wallet creation should succeed");

    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    tx.amountSatoshis = 100000000; // 1 BTC
    tx.feeSatoshis = 10000;
    tx.direction = "incoming";
    tx.toAddress = "bc1qtest123";
    tx.confirmationCount = 3;
    tx.isConfirmed = false;

    auto result = txRepo.addTransaction(tx);
    TEST_ASSERT(result.hasValue(), "Transaction addition should succeed");
    TEST_ASSERT(result->id > 0, "Transaction should have ID");
    TEST_ASSERT(result->txid == tx.txid, "TXID should match");

    TEST_PASS();
}

bool testGetTransactionByTxid(Repository::TransactionRepository& txRepo,
                               Repository::WalletRepository& walletRepo,
                               Repository::UserRepository& userRepo) {
    TEST_START("Get Transaction By TXID");

    int walletId = createTestWallet(userRepo, walletRepo, "txuser2");

    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "test_txid_12345";
    tx.amountSatoshis = 50000000;
    tx.feeSatoshis = 5000;
    tx.direction = "outgoing";

    auto addResult = txRepo.addTransaction(tx);
    TEST_ASSERT(addResult.hasValue(), "Transaction addition should succeed");

    auto getResult = txRepo.getTransactionByTxid("test_txid_12345");
    TEST_ASSERT(getResult.hasValue(), "Get transaction should succeed");
    TEST_ASSERT(getResult->txid == "test_txid_12345", "TXID should match");
    TEST_ASSERT(getResult->amountSatoshis == 50000000, "Amount should match");

    TEST_PASS();
}

bool testGetTransactionById(Repository::TransactionRepository& txRepo,
                            Repository::WalletRepository& walletRepo,
                            Repository::UserRepository& userRepo) {
    TEST_START("Get Transaction By ID");

    int walletId = createTestWallet(userRepo, walletRepo, "txuser3");

    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "test_txid_67890";
    tx.amountSatoshis = 25000000;
    tx.feeSatoshis = 2500;
    tx.direction = "incoming";

    auto addResult = txRepo.addTransaction(tx);
    TEST_ASSERT(addResult.hasValue(), "Transaction addition should succeed");

    auto getResult = txRepo.getTransactionById(addResult->id);
    TEST_ASSERT(getResult.hasValue(), "Get transaction should succeed");
    TEST_ASSERT(getResult->id == addResult->id, "ID should match");

    TEST_PASS();
}

bool testGetTransactionsByWallet(Repository::TransactionRepository& txRepo,
                                  Repository::WalletRepository& walletRepo,
                                  Repository::UserRepository& userRepo) {
    TEST_START("Get Transactions By Wallet");

    int walletId = createTestWallet(userRepo, walletRepo, "txuser4");

    // Add multiple transactions
    for (int i = 0; i < 5; i++) {
        Repository::Transaction tx;
        tx.walletId = walletId;
        tx.txid = "txid_" + std::to_string(i);
        tx.amountSatoshis = (i + 1) * 10000000;
        tx.feeSatoshis = 1000;
        tx.direction = (i % 2 == 0) ? "incoming" : "outgoing";
        txRepo.addTransaction(tx);
    }

    Repository::PaginationParams params;
    params.limit = 10;
    params.offset = 0;

    auto result = txRepo.getTransactionsByWallet(walletId, params);
    TEST_ASSERT(result.hasValue(), "Get transactions should succeed");
    TEST_ASSERT(result->items.size() == 5, "Should have 5 transactions");

    TEST_PASS();
}

bool testGetTransactionStats(Repository::TransactionRepository& txRepo,
                              Repository::WalletRepository& walletRepo,
                              Repository::UserRepository& userRepo) {
    TEST_START("Get Transaction Stats");

    int walletId = createTestWallet(userRepo, walletRepo, "txuser5");

    // Add transactions
    Repository::Transaction tx1;
    tx1.walletId = walletId;
    tx1.txid = "incoming_tx";
    tx1.amountSatoshis = 100000000;
    tx1.feeSatoshis = 0;
    tx1.direction = "incoming";
    tx1.isConfirmed = true;
    txRepo.addTransaction(tx1);

    Repository::Transaction tx2;
    tx2.walletId = walletId;
    tx2.txid = "outgoing_tx";
    tx2.amountSatoshis = 50000000;
    tx2.feeSatoshis = 10000;
    tx2.direction = "outgoing";
    tx2.isConfirmed = false;
    txRepo.addTransaction(tx2);

    auto statsResult = txRepo.getTransactionStats(walletId);
    TEST_ASSERT(statsResult.hasValue(), "Get stats should succeed");
    TEST_ASSERT(statsResult->totalTransactions == 2, "Should have 2 transactions");
    TEST_ASSERT(statsResult->confirmedTransactions == 1, "Should have 1 confirmed");
    TEST_ASSERT(statsResult->pendingTransactions == 1, "Should have 1 pending");

    TEST_PASS();
}

bool testCalculateWalletBalance(Repository::TransactionRepository& txRepo,
                                 Repository::WalletRepository& walletRepo,
                                 Repository::UserRepository& userRepo) {
    TEST_START("Calculate Wallet Balance");

    int walletId = createTestWallet(userRepo, walletRepo, "txuser6");

    // Add incoming transaction
    Repository::Transaction txIn;
    txIn.walletId = walletId;
    txIn.txid = "balance_in";
    txIn.amountSatoshis = 200000000; // 2 BTC
    txIn.feeSatoshis = 0;
    txIn.direction = "incoming";
    txIn.isConfirmed = true;
    txRepo.addTransaction(txIn);

    // Add outgoing transaction
    Repository::Transaction txOut;
    txOut.walletId = walletId;
    txOut.txid = "balance_out";
    txOut.amountSatoshis = 50000000; // 0.5 BTC
    txOut.feeSatoshis = 10000;
    txOut.direction = "outgoing";
    txOut.isConfirmed = true;
    txRepo.addTransaction(txOut);

    auto balanceResult = txRepo.calculateWalletBalance(walletId);
    TEST_ASSERT(balanceResult.hasValue(), "Calculate balance should succeed");
    // Confirmed: 2 BTC - (0.5 BTC + fee) = 1.4999 BTC
    TEST_ASSERT(balanceResult->confirmedBalance == 149990000, "Confirmed balance should be correct");

    TEST_PASS();
}

bool testUpdateTransactionConfirmation(Repository::TransactionRepository& txRepo,
                                        Repository::WalletRepository& walletRepo,
                                        Repository::UserRepository& userRepo) {
    TEST_START("Update Transaction Confirmation");

    int walletId = createTestWallet(userRepo, walletRepo, "txuser7");

    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "confirm_test";
    tx.amountSatoshis = 100000000;
    tx.feeSatoshis = 10000;
    tx.direction = "incoming";
    tx.confirmationCount = 0;
    tx.isConfirmed = false;

    txRepo.addTransaction(tx);

    auto updateResult = txRepo.updateTransactionConfirmation("confirm_test", 123456, "blockhash123", 6);
    TEST_ASSERT(updateResult.hasValue(), "Update confirmation should succeed");

    auto getResult = txRepo.getTransactionByTxid("confirm_test");
    TEST_ASSERT(getResult.hasValue(), "Get transaction should succeed");
    TEST_ASSERT(getResult->confirmationCount == 6, "Confirmation count should be 6");

    TEST_PASS();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    TestUtils::printTestHeader("TransactionRepository Unit Tests");

    Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
    TestUtils::initializeTestLogger("test_tx_repo.log");

    if (!TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY)) {
        std::cerr << COLOR_RED << "Failed to initialize test environment" << COLOR_RESET << std::endl;
        return 1;
    }

    Repository::UserRepository userRepo(dbManager);
    Repository::WalletRepository walletRepo(dbManager);
    Repository::TransactionRepository txRepo(dbManager);

    // Run tests
    testAddTransaction(txRepo, walletRepo, userRepo);
    testGetTransactionByTxid(txRepo, walletRepo, userRepo);
    testGetTransactionById(txRepo, walletRepo, userRepo);
    testGetTransactionsByWallet(txRepo, walletRepo, userRepo);
    testGetTransactionStats(txRepo, walletRepo, userRepo);
    testCalculateWalletBalance(txRepo, walletRepo, userRepo);
    testUpdateTransactionConfirmation(txRepo, walletRepo, userRepo);

    // Print summary
    TestUtils::printTestSummary("Test");

    // Cleanup
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

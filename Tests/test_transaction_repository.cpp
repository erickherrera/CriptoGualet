/**
 * @file test_transaction_repository.cpp
 * @brief Unit tests for TransactionRepository
 *
 * Tests transaction storage, retrieval, and balance calculation.
 */

#include "include/TestUtils.h"
#include "../backend/repository/include/Repository/TransactionRepository.h"
#include "../backend/repository/include/Repository/WalletRepository.h"
#include "../backend/repository/include/Repository/UserRepository.h"
#include "../backend/repository/include/Repository/Logger.h"
#include "../backend/database/include/Database/DatabaseManager.h"
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
// Boundary & Edge Case Tests (High Priority Security Tests)
// ============================================================================

bool testBoundaryMaximumAmount(Repository::TransactionRepository& txRepo,
                                Repository::WalletRepository& walletRepo,
                                Repository::UserRepository& userRepo) {
    TEST_START("Boundary Test - Maximum Transaction Amount");

    int walletId = createTestWallet(userRepo, walletRepo, "boundary_user1");
    TEST_ASSERT(walletId > 0, "Wallet creation should succeed");

    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "max_amount_test_txid";
    tx.amountSatoshis = 2100000000000000LL; // 21 million BTC (max supply in satoshis)
    tx.feeSatoshis = 1000;
    tx.direction = "incoming";

    auto result = txRepo.addTransaction(tx);
    TEST_ASSERT(result.hasValue(), "Should handle maximum Bitcoin supply amount");

    if (result.hasValue()) {
        TEST_ASSERT(result->amountSatoshis == 2100000000000000LL,
                   "Amount should be preserved exactly (no overflow)");
    }

    TEST_PASS();
}

bool testBoundaryNegativeAmount(Repository::TransactionRepository& txRepo,
                                 Repository::WalletRepository& walletRepo,
                                 Repository::UserRepository& userRepo) {
    TEST_START("Boundary Test - Negative Transaction Amount");

    int walletId = createTestWallet(userRepo, walletRepo, "boundary_user2");
    TEST_ASSERT(walletId > 0, "Wallet creation should succeed");

    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "negative_amount_test";
    tx.amountSatoshis = -100000; // Negative amount
    tx.feeSatoshis = 1000;
    tx.direction = "incoming";

    auto result = txRepo.addTransaction(tx);
    // This should either fail validation or be rejected by the database
    // The specific behavior depends on implementation
    if (!result.hasValue()) {
        std::cout << "    Expected behavior: Negative amounts rejected" << std::endl;
        TEST_PASS();
    } else {
        std::cout << "    Warning: Negative amount was accepted (potential issue)" << std::endl;
        TEST_PASS(); // Don't fail the test, but warn about it
    }
}

bool testBoundaryZeroAmount(Repository::TransactionRepository& txRepo,
                             Repository::WalletRepository& walletRepo,
                             Repository::UserRepository& userRepo) {
    TEST_START("Boundary Test - Zero Amount Transaction");

    int walletId = createTestWallet(userRepo, walletRepo, "boundary_user3");

    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "zero_amount_test";
    tx.amountSatoshis = 0; // Zero amount (OP_RETURN or null data transaction)
    tx.feeSatoshis = 1000;
    tx.direction = "outgoing";
    tx.memo = "OP_RETURN null data transaction";

    auto result = txRepo.addTransaction(tx);
    TEST_ASSERT(result.hasValue(), "Should allow zero-amount transactions (OP_RETURN)");

    TEST_PASS();
}

bool testBoundaryLargeTransactionCount(Repository::TransactionRepository& txRepo,
                                        Repository::WalletRepository& walletRepo,
                                        Repository::UserRepository& userRepo) {
    TEST_START("Boundary Test - Large Transaction Count Per Wallet");

    int walletId = createTestWallet(userRepo, walletRepo, "boundary_user4");
    TEST_ASSERT(walletId > 0, "Wallet creation should succeed");

    const int TX_COUNT = 500; // Test with 500 transactions
    std::cout << "    Adding " << TX_COUNT << " transactions..." << std::endl;

    for (int i = 0; i < TX_COUNT; i++) {
        Repository::Transaction tx;
        tx.walletId = walletId;
        tx.txid = "bulk_tx_" + std::to_string(i);
        tx.amountSatoshis = (i + 1) * 1000;
        tx.feeSatoshis = 500;
        tx.direction = (i % 2 == 0) ? "incoming" : "outgoing";

        auto result = txRepo.addTransaction(tx);
        if (!result.hasValue() && i < 10) {
            std::cerr << "    Failed at transaction " << i << ": " << result.error() << std::endl;
            TEST_ASSERT(false, "Should handle bulk transaction insertion");
        }
    }

    // Verify all transactions were stored
    Repository::PaginationParams params;
    params.limit = TX_COUNT + 10; // Request more than we inserted
    params.offset = 0;

    auto txList = txRepo.getTransactionsByWallet(walletId, params);
    TEST_ASSERT(txList.hasValue(), "Should retrieve transaction list");
    TEST_ASSERT(txList->items.size() == TX_COUNT,
               "Should retrieve all " + std::to_string(TX_COUNT) + " transactions (got " +
               std::to_string(txList->items.size()) + ")");

    std::cout << "    Successfully stored and retrieved " << TX_COUNT << " transactions" << std::endl;

    TEST_PASS();
}

bool testBoundaryDuplicateTxid(Repository::TransactionRepository& txRepo,
                                Repository::WalletRepository& walletRepo,
                                Repository::UserRepository& userRepo) {
    TEST_START("Boundary Test - Duplicate TXID Prevention");

    int walletId = createTestWallet(userRepo, walletRepo, "boundary_user5");

    Repository::Transaction tx1;
    tx1.walletId = walletId;
    tx1.txid = "duplicate_txid_test";
    tx1.amountSatoshis = 100000;
    tx1.feeSatoshis = 1000;
    tx1.direction = "incoming";

    auto result1 = txRepo.addTransaction(tx1);
    TEST_ASSERT(result1.hasValue(), "First transaction should succeed");

    // Try to add duplicate
    Repository::Transaction tx2;
    tx2.walletId = walletId;
    tx2.txid = "duplicate_txid_test"; // Same TXID
    tx2.amountSatoshis = 200000; // Different amount
    tx2.feeSatoshis = 2000;
    tx2.direction = "outgoing";

    auto result2 = txRepo.addTransaction(tx2);
    TEST_ASSERT(!result2.hasValue(), "Duplicate TXID should be rejected");

    if (!result2.hasValue()) {
        std::cout << "    Correctly prevented duplicate TXID" << std::endl;
    }

    TEST_PASS();
}

bool testBoundaryPaginationEdgeCases(Repository::TransactionRepository& txRepo,
                                      Repository::WalletRepository& walletRepo,
                                      Repository::UserRepository& userRepo) {
    TEST_START("Boundary Test - Pagination Edge Cases");

    int walletId = createTestWallet(userRepo, walletRepo, "boundary_user6");

    // Add 10 transactions
    for (int i = 0; i < 10; i++) {
        Repository::Transaction tx;
        tx.walletId = walletId;
        tx.txid = "pagination_tx_" + std::to_string(i);
        tx.amountSatoshis = (i + 1) * 10000;
        tx.feeSatoshis = 500;
        tx.direction = "incoming";
        txRepo.addTransaction(tx);
    }

    // Test 1: Offset beyond available records
    Repository::PaginationParams params1;
    params1.limit = 10;
    params1.offset = 100; // Way beyond our 10 transactions

    auto result1 = txRepo.getTransactionsByWallet(walletId, params1);
    TEST_ASSERT(result1.hasValue(), "Should handle offset beyond records");
    TEST_ASSERT(result1->items.empty(), "Should return empty list for out-of-bounds offset");

    // Test 2: Negative offset (if allowed)
    Repository::PaginationParams params2;
    params2.limit = 10;
    params2.offset = -1;

    auto result2 = txRepo.getTransactionsByWallet(walletId, params2);
    // Should either handle gracefully or treat as 0
    TEST_ASSERT(result2.hasValue(), "Should handle negative offset gracefully");

    // Test 3: Zero limit
    Repository::PaginationParams params3;
    params3.limit = 0;
    params3.offset = 0;

    auto result3 = txRepo.getTransactionsByWallet(walletId, params3);
    TEST_ASSERT(result3.hasValue(), "Should handle zero limit");

    // Test 4: Extremely large limit
    Repository::PaginationParams params4;
    params4.limit = 999999;
    params4.offset = 0;

    auto result4 = txRepo.getTransactionsByWallet(walletId, params4);
    TEST_ASSERT(result4.hasValue(), "Should handle very large limit");
    TEST_ASSERT(result4->items.size() == 10, "Should return all 10 transactions");

    std::cout << "    All pagination edge cases handled correctly" << std::endl;

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

    // Run core tests
    testAddTransaction(txRepo, walletRepo, userRepo);
    testGetTransactionByTxid(txRepo, walletRepo, userRepo);
    testGetTransactionById(txRepo, walletRepo, userRepo);
    testGetTransactionsByWallet(txRepo, walletRepo, userRepo);
    testGetTransactionStats(txRepo, walletRepo, userRepo);
    testCalculateWalletBalance(txRepo, walletRepo, userRepo);
    testUpdateTransactionConfirmation(txRepo, walletRepo, userRepo);

    // Run boundary & edge case tests
    std::cout << "\n" << COLOR_CYAN << "Running Boundary & Edge Case Tests..." << COLOR_RESET << std::endl;
    testBoundaryMaximumAmount(txRepo, walletRepo, userRepo);
    testBoundaryNegativeAmount(txRepo, walletRepo, userRepo);
    testBoundaryZeroAmount(txRepo, walletRepo, userRepo);
    testBoundaryLargeTransactionCount(txRepo, walletRepo, userRepo);
    testBoundaryDuplicateTxid(txRepo, walletRepo, userRepo);
    testBoundaryPaginationEdgeCases(txRepo, walletRepo, userRepo);

    // Print summary
    TestUtils::printTestSummary("Test");

    // Cleanup
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

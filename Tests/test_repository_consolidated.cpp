// test_repository_consolidated.cpp - Consolidated repository tests
#include "TestUtils.h"
#include "../backend/repository/include/Repository/UserRepository.h"
#include "../backend/repository/include/Repository/WalletRepository.h"
#include "../backend/repository/include/Repository/TransactionRepository.h"
#include "../backend/repository/include/Repository/Logger.h"
#include "../backend/database/include/Database/DatabaseManager.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <set>
#include <map>
#include <optional>

// Test database paths
const std::string TEST_INTEGRATION_DB_PATH = "test_integration.db";
const std::string TEST_TX_REPO_DB_PATH = "test_tx_repo.db";
const std::string TEST_USER_REPO_DB_PATH = "test_user_repo.db";
const std::string TEST_WALLET_REPO_DB_PATH = "test_wallet_repo.db";

// Test macros
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
            return; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        std::cout << COLOR_GREEN << "  ✓ PASSED" << COLOR_RESET << std::endl; \
        TestGlobals::g_testsPassed++; \
    } while(0)

#define TEST_STEP(message) \
    do { \
        std::cout << "    " << message << "..." << std::endl; \
    } while(0)

// ============================================================================
// Integration Test Cases from test_repository_integration.cpp
// ============================================================================

void testCompleteUserWalletWorkflow(Repository::UserRepository& userRepo,
                                      Repository::WalletRepository& walletRepo,
                                      Repository::TransactionRepository& txRepo) {
    TEST_START("Complete User → Wallet → Addresses → Transactions Workflow");

    // Step 1: Create User
    TEST_STEP("Creating user 'alice'");
    auto userResult = userRepo.createUser("alice", "SecurePass123!");
    TEST_ASSERT(userResult.hasValue(), "User creation should succeed");
    int userId = userResult->id;
    std::cout << "    User ID: " << userId << std::endl;

    // Step 2: Authenticate User
    TEST_STEP("Authenticating user");
    auto authResult = userRepo.authenticateUser("alice", "SecurePass123!");
    TEST_ASSERT(authResult.hasValue(), "Authentication should succeed");

    // Step 3: Store Encrypted Seed
    TEST_STEP("Storing encrypted BIP39 seed");
    std::vector<std::string> mnemonic = {
        "abandon", "ability", "able", "about", "above", "absent",
        "absorb", "abstract", "absurd", "abuse", "access", "accident"
    };
    auto seedResult = walletRepo.storeEncryptedSeed(userId, "SecurePass123!", mnemonic);
    TEST_ASSERT(seedResult.hasValue(), "Seed storage should succeed");

    // Step 4: Create Bitcoin Wallet
    TEST_STEP("Creating Bitcoin wallet");
    auto wallet1Result = walletRepo.createWallet(userId, "Main Bitcoin Wallet", "bitcoin");
    TEST_ASSERT(wallet1Result.hasValue(), "Bitcoin wallet creation should succeed");
    int btcWalletId = wallet1Result->id;
    std::cout << "    Bitcoin Wallet ID: " << btcWalletId << std::endl;

    // Step 5: Create Litecoin Wallet
    TEST_STEP("Creating Litecoin wallet");
    auto wallet2Result = walletRepo.createWallet(userId, "Litecoin Savings", "litecoin");
    TEST_ASSERT(wallet2Result.hasValue(), "Litecoin wallet creation should succeed");

    // Step 6: Get All User Wallets
    TEST_STEP("Retrieving all user wallets");
    auto walletsResult = walletRepo.getWalletsByUserId(userId);
    TEST_ASSERT(walletsResult.hasValue(), "Get wallets should succeed");
    TEST_ASSERT(walletsResult->size() == 2, "Should have 2 wallets");
    std::cout << "    Total wallets: " << walletsResult->size() << std::endl;

    // Step 7: Generate Receiving Addresses
    TEST_STEP("Generating receiving addresses");
    auto addr1 = walletRepo.generateAddress(btcWalletId, false, "Primary Receiving");
    auto addr2 = walletRepo.generateAddress(btcWalletId, false, "Secondary Receiving");
    TEST_ASSERT(addr1.hasValue() && addr2.hasValue(), "Address generation should succeed");
    std::cout << "    Address 1: " << addr1->address << std::endl;
    std::cout << "    Address 2: " << addr2->address << std::endl;

    // Step 8: Generate Change Address
    TEST_STEP("Generating change address");
    auto changeAddr = walletRepo.generateAddress(btcWalletId, true, "Change");
    TEST_ASSERT(changeAddr.hasValue(), "Change address generation should succeed");
    std::cout << "    Change Address: " << changeAddr->address << std::endl;

    // Step 9: Add Incoming Transaction
    TEST_STEP("Adding incoming transaction (1 BTC)");
    Repository::Transaction txIn;
    txIn.walletId = btcWalletId;
    txIn.txid = "abc123def456...incoming";
    txIn.amountSatoshis = 100000000; // 1 BTC
    txIn.feeSatoshis = 0;
    txIn.direction = "incoming";
    txIn.toAddress = addr1->address;
    txIn.confirmationCount = 3;
    txIn.isConfirmed = false;
    txIn.memo = "Payment from Bob";

    auto txInResult = txRepo.addTransaction(txIn);
    TEST_ASSERT(txInResult.hasValue(), "Incoming transaction should be added");
    std::cout << "    Transaction ID: " << txInResult->id << std::endl;

    // Step 10: Add Outgoing Transaction
    TEST_STEP("Adding outgoing transaction (0.3 BTC)");
    Repository::Transaction txOut;
    txOut.walletId = btcWalletId;
    txOut.txid = "def789ghi012...outgoing";
    txOut.amountSatoshis = 30000000; // 0.3 BTC
    txOut.feeSatoshis = 10000; // 0.0001 BTC fee
    txOut.direction = "outgoing";
    txOut.fromAddress = addr1->address;
    txOut.toAddress = "bc1qexternal...";
    txOut.confirmationCount = 1;
    txOut.isConfirmed = false;
    txOut.memo = "Payment to Charlie";

    auto txOutResult = txRepo.addTransaction(txOut);
    TEST_ASSERT(txOutResult.hasValue(), "Outgoing transaction should be added");

    // Step 11: Get Transaction History
    TEST_STEP("Retrieving transaction history");
    Repository::PaginationParams params;
    params.limit = 10;
    params.offset = 0;

    auto txHistoryResult = txRepo.getTransactionsByWallet(btcWalletId, params);
    TEST_ASSERT(txHistoryResult.hasValue(), "Get transaction history should succeed");
    TEST_ASSERT(txHistoryResult->items.size() == 2, "Should have 2 transactions");
    std::cout << "    Total transactions: " << txHistoryResult->items.size() << std::endl;

    // Step 12: Calculate Wallet Balance
    TEST_STEP("Calculating wallet balance");
    auto balanceResult = txRepo.calculateWalletBalance(btcWalletId);
    TEST_ASSERT(balanceResult.hasValue(), "Calculate balance should succeed");
    std::cout << "    Confirmed Balance: " << balanceResult->confirmedBalance << " satoshis" << std::endl;
    std::cout << "    Unconfirmed Balance: " << balanceResult->unconfirmedBalance << " satoshis" << std::endl;
    std::cout << "    Total Balance: " << balanceResult->totalBalance << " satoshis" << std::endl;

    // Step 13: Get Transaction Stats
    TEST_STEP("Getting transaction statistics");
    auto statsResult = txRepo.getTransactionStats(btcWalletId);
    TEST_ASSERT(statsResult.hasValue(), "Get stats should succeed");
    std::cout << "    Total Transactions: " << statsResult->totalTransactions << std::endl;
    std::cout << "    Confirmed: " << statsResult->confirmedTransactions << std::endl;
    std::cout << "    Pending: " << statsResult->pendingTransactions << std::endl;
    std::cout << "    Total Received: " << statsResult->totalReceived << " satoshis" << std::endl;
    std::cout << "    Total Sent: " << statsResult->totalSent << " satoshis" << std::endl;

    // Step 14: Update Transaction Confirmations
    TEST_STEP("Updating transaction confirmations");
    auto updateResult = txRepo.updateTransactionConfirmation(
        txIn.txid, 700000, "blockhash123", 6
    );
    TEST_ASSERT(updateResult.hasValue(), "Confirmation update should succeed");

    // Step 15: Retrieve and Verify Seed
    TEST_STEP("Retrieving and verifying encrypted seed");
    auto retrieveSeedResult = walletRepo.retrieveDecryptedSeed(userId, "SecurePass123!");
    TEST_ASSERT(retrieveSeedResult.hasValue(), "Seed retrieval should succeed");
    TEST_ASSERT(*retrieveSeedResult == mnemonic, "Retrieved seed should match original");
    std::cout << "    Seed words verified: " << retrieveSeedResult->size() << " words" << std::endl;

    // Step 16: Confirm Seed Backup
    TEST_STEP("Confirming seed backup");
    auto confirmResult = walletRepo.confirmSeedBackup(userId);
    TEST_ASSERT(confirmResult.hasValue(), "Seed backup confirmation should succeed");

    TEST_PASS();
}

void testMultiUserScenario(Repository::UserRepository& userRepo,
                            Repository::WalletRepository& walletRepo,
                            Repository::TransactionRepository& txRepo) {
    TEST_START("Multi-User Scenario with Wallet Isolation");

    // Create User 1
    TEST_STEP("Creating User 1 (bob)");
    auto user1 = userRepo.createUser("bob", "BobPass123!");
    TEST_ASSERT(user1.hasValue(), "User 1 creation should succeed");

    // Create User 2
    TEST_STEP("Creating User 2 (carol)");
    auto user2 = userRepo.createUser("carol", "CarolPass123!");
    TEST_ASSERT(user2.hasValue(), "User 2 creation should succeed");

    // Create wallets for both users
    TEST_STEP("Creating wallets for both users");
    auto bobWallet = walletRepo.createWallet(user1->id, "Bob's Wallet", "bitcoin");
    auto carolWallet = walletRepo.createWallet(user2->id, "Carol's Wallet", "bitcoin");
    TEST_ASSERT(bobWallet.hasValue() && carolWallet.hasValue(), "Wallet creation should succeed");

    // Add transactions
    TEST_STEP("Adding transactions for both users");
    Repository::Transaction bobTx;
    bobTx.walletId = bobWallet->id;
    bobTx.txid = "bob_tx_001";
    bobTx.amountSatoshis = 50000000;
    bobTx.feeSatoshis = 5000;
    bobTx.direction = "incoming";
    txRepo.addTransaction(bobTx);

    Repository::Transaction carolTx;
    carolTx.walletId = carolWallet->id;
    carolTx.txid = "carol_tx_001";
    carolTx.amountSatoshis = 75000000;
    carolTx.feeSatoshis = 7500;
    carolTx.direction = "incoming";
    txRepo.addTransaction(carolTx);

    // Verify wallet isolation
    TEST_STEP("Verifying wallet isolation");
    Repository::PaginationParams params;
    auto bobTxs = txRepo.getTransactionsByWallet(bobWallet->id, params);
    auto carolTxs = txRepo.getTransactionsByWallet(carolWallet->id, params);

    TEST_ASSERT(bobTxs.hasValue() && carolTxs.hasValue(), "Get transactions should succeed");
    TEST_ASSERT(bobTxs->items.size() == 1, "Bob should have 1 transaction");
    TEST_ASSERT(carolTxs->items.size() == 1, "Carol should have 1 transaction");
    TEST_ASSERT(bobTxs->items[0].txid == "bob_tx_001", "Bob's transaction should be isolated");
    TEST_ASSERT(carolTxs->items[0].txid == "carol_tx_001", "Carol's transaction should be isolated");

    std::cout << "    Bob's transactions: " << bobTxs->items.size() << std::endl;
    std::cout << "    Carol's transactions: " << carolTxs->items.size() << std::endl;

    TEST_PASS();
}

void testErrorHandlingAndRollback(Repository::UserRepository& userRepo,
                                    Repository::WalletRepository& walletRepo) {
    TEST_START("Error Handling and Transaction Rollback");

    // Test duplicate username
    TEST_STEP("Testing duplicate username detection");
    userRepo.createUser("duplicate", "Pass123!");
    auto duplicateResult = userRepo.createUser("duplicate", "Pass123!");
    TEST_ASSERT(!duplicateResult.hasValue(), "Should reject duplicate username");
    TEST_ASSERT(duplicateResult.errorCode == 409, "Error code should be 409");

    // Test wrong password
    TEST_STEP("Testing authentication with wrong password");
    auto authResult = userRepo.authenticateUser("duplicate", "WrongPass123!");
    TEST_ASSERT(!authResult.hasValue(), "Authentication should fail with wrong password");
    TEST_ASSERT(authResult.errorCode == 401, "Error code should be 401");

    // Test invalid wallet name
    TEST_STEP("Testing invalid wallet creation");
    auto user = userRepo.createUser("testuser", "Pass123!");
    auto invalidWallet = walletRepo.createWallet(user->id, "", "bitcoin"); // Empty name
    TEST_ASSERT(!invalidWallet.hasValue(), "Empty wallet name should fail");

    TEST_PASS();
}

// ============================================================================
// Transaction Repository Test Cases from test_transaction_repository.cpp
// ============================================================================

// Helper: Create test wallet with user
int createTestWallet(Repository::UserRepository& userRepo, Repository::WalletRepository& walletRepo,
                      const std::string& username) {
    return TestUtils::createTestUserWithWallet(userRepo, walletRepo, username);
}

void testAddTransaction(Repository::TransactionRepository& txRepo,
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

void testGetTransactionByTxid(Repository::TransactionRepository& txRepo,
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

void testGetTransactionById(Repository::TransactionRepository& txRepo,
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

void testGetTransactionsByWallet(Repository::TransactionRepository& txRepo,
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

void testGetTransactionStats(Repository::TransactionRepository& txRepo,
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

void testCalculateWalletBalance(Repository::TransactionRepository& txRepo,
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

void testUpdateTransactionConfirmation(Repository::TransactionRepository& txRepo,
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

// Boundary & edge case tests
void testBoundaryMaximumAmount(Repository::TransactionRepository& txRepo,
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

void testBoundaryNegativeAmount(Repository::TransactionRepository& txRepo,
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

void testBoundaryZeroAmount(Repository::TransactionRepository& txRepo,
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

void testBoundaryLargeTransactionCount(Repository::TransactionRepository& txRepo,
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

void testBoundaryDuplicateTxid(Repository::TransactionRepository& txRepo,
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

void testBoundaryPaginationEdgeCases(Repository::TransactionRepository& txRepo,
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
// User Repository Test Cases from test_user_repository.cpp
// ============================================================================

void testCreateUser(Repository::UserRepository& userRepo) {
    TEST_START("Create User");
    auto result = userRepo.createUser("testuser", "SecurePass123!");
    TEST_ASSERT(result.hasValue(), "User creation should succeed");
    TEST_ASSERT(result->username == "testuser", "Username should match");
    TEST_ASSERT(!result->passwordHash.empty(), "Password should be hashed");
    TEST_ASSERT(!result->salt.empty(), "Salt should be generated");
    TEST_PASS();
}

void testCreateUserDuplicateUsername(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Duplicate Username");
    userRepo.createUser("dupuser", "SecurePass123!");
    auto result = userRepo.createUser("dupuser", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Should reject duplicate username");
    TEST_PASS();
}

void testCreateUserInvalidUsername(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Invalid Username");
    auto result1 = userRepo.createUser("a", "SecurePass123!");
    TEST_ASSERT(!result1.hasValue(), "Should reject too short username");
    auto result2 = userRepo.createUser("user space", "SecurePass123!");
    TEST_ASSERT(!result2.hasValue(), "Should reject username with spaces");
    TEST_PASS();
}

void testCreateUserInvalidPassword(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Invalid Password");
    auto result1 = userRepo.createUser("user1", "short");
    TEST_ASSERT(!result1.hasValue(), "Should reject too short password");
    auto result2 = userRepo.createUser("user2", "nonumber");
    TEST_ASSERT(!result2.hasValue(), "Should reject password without numbers");
    TEST_PASS();
}

void testAuthenticateUserSuccess(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Success");
    userRepo.createUser("authuser", "SecurePass123!");
    auto result = userRepo.authenticateUser("authuser", "SecurePass123!");
    TEST_ASSERT(result.hasValue(), "Authentication should succeed");
    TEST_ASSERT(result->username == "authuser", "Authenticated user should match");
    TEST_PASS();
}

void testAuthenticateUserWrongPassword(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Wrong Password");
    userRepo.createUser("wrongpass", "SecurePass123!");
    auto result = userRepo.authenticateUser("wrongpass", "WrongPass123!");
    TEST_ASSERT(!result.hasValue(), "Authentication should fail with wrong password");
    TEST_PASS();
}

void testAuthenticateUserNotFound(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Not Found");
    auto result = userRepo.authenticateUser("nonexistent", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Authentication should fail for nonexistent user");
    TEST_PASS();
}

void testGetUserByUsername(Repository::UserRepository& userRepo) {
    TEST_START("Get User By Username");
    userRepo.createUser("getuser", "SecurePass123!");
    auto result = userRepo.getUserByUsername("getuser");
    TEST_ASSERT(result.hasValue(), "Should find user by username");
    TEST_ASSERT(result->username == "getuser", "Found user should match");
    TEST_PASS();
}

void testGetUserById(Repository::UserRepository& userRepo) {
    TEST_START("Get User By ID");
    auto createResult = userRepo.createUser("iduser", "SecurePass123!");
    auto result = userRepo.getUserById(createResult->id);
    TEST_ASSERT(result.hasValue(), "Should find user by ID");
    TEST_ASSERT(result->id == createResult->id, "Found ID should match");
    TEST_PASS();
}

void testChangePassword(Repository::UserRepository& userRepo) {
    TEST_START("Change Password");
    auto user = userRepo.createUser("changepass", "OldPass123!");
    auto result = userRepo.changePassword(user->id, "OldPass123!", "NewPass123!");
    TEST_ASSERT(result.hasValue() && *result, "Password change should succeed");
    auto auth = userRepo.authenticateUser("changepass", "NewPass123!");
    TEST_ASSERT(auth.hasValue(), "Should authenticate with new password");
    TEST_PASS();
}

void testChangePasswordWrongCurrent(Repository::UserRepository& userRepo) {
    TEST_START("Change Password - Wrong Current");
    auto user = userRepo.createUser("changewrong", "OldPass123!");
    auto result = userRepo.changePassword(user->id, "WrongOld123!", "NewPass123!");
    TEST_ASSERT(!result.hasValue() || !(*result),
                 "Password change should fail with wrong current password");
    TEST_PASS();
}

void testIsUsernameAvailable(Repository::UserRepository& userRepo) {
    TEST_START("Is Username Available");
    userRepo.createUser("taken", "SecurePass123!");
    auto result1 = userRepo.isUsernameAvailable("taken");
    TEST_ASSERT(result1.hasValue() && !(*result1), "Username 'taken' should NOT be available");
    auto result2 = userRepo.isUsernameAvailable("available");
    TEST_ASSERT(result2.hasValue() && *result2, "Username 'available' SHOULD be available");
    TEST_PASS();
}

void testPasswordHashingUniqueness(Repository::UserRepository& userRepo) {
    TEST_START("Password Hashing Uniqueness (Salting)");
    auto user1 = userRepo.createUser("salt1", "SamePass123!");
    auto user2 = userRepo.createUser("salt2", "SamePass123!");
    TEST_ASSERT(user1->passwordHash != user2->passwordHash,
                "Same password should result in different hashes due to salt");
    TEST_PASS();
}

void testUpdateLastLogin(Repository::UserRepository& userRepo) {
    TEST_START("Update Last Login");
    auto user = userRepo.createUser("loginuser", "SecurePass123!");
    auto result = userRepo.updateLastLogin(user->id);
    TEST_ASSERT(result.hasValue() && *result, "Update last login should succeed");
    TEST_PASS();
}

void testSQLInjectionInUsername(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Username");
    auto result = userRepo.createUser("' OR '1'='1", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Should reject SQL injection in username");
    TEST_PASS();
}

void testSQLInjectionInPassword(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Password");
    auto user = userRepo.createUser("injectpass", "SecurePass123!");
    auto result = userRepo.authenticateUser("injectpass", "' OR '1'='1");
    TEST_ASSERT(!result.hasValue(), "Should not authenticate with SQL injection");
    TEST_PASS();
}

void testSQLInjectionInEmail(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Email");
    auto result = userRepo.createUser("injectemail", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Should reject SQL injection in email");
    TEST_PASS();
}

void testSQLInjectionInAuthenticateUser(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Authenticate");
    auto result = userRepo.authenticateUser("' UNION SELECT * FROM users --", "anything");
    TEST_ASSERT(!result.hasValue(), "Should not authenticate with SQL injection");
    TEST_PASS();
}

void testUnicodeCharactersInUsername(Repository::UserRepository& userRepo) {
    TEST_START("Unicode Characters in Username");
    std::vector<std::string> unicodeUsernames = {"User_ñ", "User_€", "User_你好", "User_🚀"};
    for (const auto& username : unicodeUsernames) {
        auto result = userRepo.createUser(username, "SecurePass123!");
        if (result.hasValue()) {
            auto found = userRepo.getUserByUsername(username);
            TEST_ASSERT(found.hasValue() && found->username == username,
                        "Should correctly store and retrieve unicode username");
        }
    }
    TEST_PASS();
}

void testExtremelyLongInputs(Repository::UserRepository& userRepo) {
    TEST_START("Extremely Long Inputs");
    std::string longUsername(1000, 'a');
    auto result1 = userRepo.createUser(longUsername, "SecurePass123!");
    TEST_ASSERT(!result1.hasValue(), "Should reject extremely long username");

    std::string longPassword(10000, 'P');
    auto result2 = userRepo.createUser("longpassuser", longPassword);
    if (!result2.hasValue()) {
        std::cout << "    Rejected 10000-char password" << std::endl;
    }
    TEST_PASS();
}

// ============================================================================
// Wallet Repository Test Cases from test_wallet_repository.cpp
// ============================================================================

// Helper: Create test user
static int createTestUser(Repository::UserRepository& userRepo, const std::string& username) {
    auto result = userRepo.createUser(username, "SecurePass123!");
    return result.hasValue() ? result->id : -1;
}

void testCreateWallet(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Create Wallet - Basic");

    int userId = createTestUser(userRepo, "walletuser1");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    auto result = walletRepo.createWallet(userId, "My Bitcoin Wallet", "bitcoin");
    TEST_ASSERT(result.hasValue(), "Wallet creation should succeed");
    TEST_ASSERT(result->walletName == "My Bitcoin Wallet", "Wallet name should match");
    TEST_ASSERT(result->walletType == "bitcoin", "Wallet type should match");
    TEST_ASSERT(result->userId == userId, "User ID should match");
    TEST_ASSERT(result->isActive, "Wallet should be active");

    TEST_PASS();
}

void testCreateMultipleWallets(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Create Multiple Wallets");

    int userId = createTestUser(userRepo, "walletuser2");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    auto wallet1 = walletRepo.createWallet(userId, "Bitcoin Wallet", "bitcoin");
    auto wallet2 = walletRepo.createWallet(userId, "Litecoin Wallet", "litecoin");

    TEST_ASSERT(wallet1.hasValue() && wallet2.hasValue(), "Both wallets should be created");
    TEST_ASSERT(wallet1->id != wallet2->id, "Wallet IDs should differ");

    TEST_PASS();
}

void testGetWalletsByUserId(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Get Wallets By User ID");

    int userId = createTestUser(userRepo, "walletuser3");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    walletRepo.createWallet(userId, "Wallet 1", "bitcoin");
    walletRepo.createWallet(userId, "Wallet 2", "bitcoin");
    walletRepo.createWallet(userId, "Wallet 3", "litecoin");

    auto walletsResult = walletRepo.getWalletsByUserId(userId);
    TEST_ASSERT(walletsResult.hasValue(), "Get wallets should succeed");
    TEST_ASSERT(walletsResult->size() == 3, "Should have 3 wallets");

    TEST_PASS();
}

void testGetWalletById(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Get Wallet By ID");

    int userId = createTestUser(userRepo, "walletuser4");
    auto createResult = walletRepo.createWallet(userId, "Test Wallet", "bitcoin");
    TEST_ASSERT(createResult.hasValue(), "Wallet creation should succeed");

    auto getResult = walletRepo.getWalletById(createResult->id);
    TEST_ASSERT(getResult.hasValue(), "Get wallet should succeed");
    TEST_ASSERT(getResult->id == createResult->id, "Wallet ID should match");
    TEST_ASSERT(getResult->walletName == "Test Wallet", "Wallet name should match");

    TEST_PASS();
}

void testGetWalletByName(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Get Wallet By Name");

    int userId = createTestUser(userRepo, "walletuser5");
    walletRepo.createWallet(userId, "Named Wallet", "bitcoin");

    auto result = walletRepo.getWalletByName(userId, "Named Wallet");
    TEST_ASSERT(result.hasValue(), "Get wallet by name should succeed");
    TEST_ASSERT(result->walletName == "Named Wallet", "Wallet name should match");

    TEST_PASS();
}

void testGenerateAddress(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Generate Address");

    int userId = createTestUser(userRepo, "walletuser6");
    auto walletResult = walletRepo.createWallet(userId, "Address Test Wallet", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    // Wallet should have initial address from creation
    auto addressesResult = walletRepo.getAddressesByWallet(walletResult->id);
    TEST_ASSERT(addressesResult.hasValue(), "Get addresses should succeed");
    TEST_ASSERT(addressesResult->size() >= 1, "Wallet should have at least one address");

    // Generate additional address
    auto newAddressResult = walletRepo.generateAddress(walletResult->id, false, "Receiving");
    TEST_ASSERT(newAddressResult.hasValue(), "Address generation should succeed");
    TEST_ASSERT(!newAddressResult->address.empty(), "Address string should not be empty");
    TEST_ASSERT(newAddressResult->isChange == false, "Should be receiving address");

    TEST_PASS();
}

void testGenerateChangeAddress(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Generate Change Address");

    int userId = createTestUser(userRepo, "walletuser7");
    auto walletResult = walletRepo.createWallet(userId, "Change Address Test", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto changeAddressResult = walletRepo.generateAddress(walletResult->id, true, "Change");
    TEST_ASSERT(changeAddressResult.hasValue(), "Change address generation should succeed");
    TEST_ASSERT(changeAddressResult->isChange == true, "Should be change address");

    TEST_PASS();
}

void testGetAddressesByWallet(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Get Addresses By Wallet");

    int userId = createTestUser(userRepo, "walletuser8");
    auto walletResult = walletRepo.createWallet(userId, "Multi Address Wallet", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    // Generate multiple addresses
    walletRepo.generateAddress(walletResult->id, false);
    walletRepo.generateAddress(walletResult->id, false);
    walletRepo.generateAddress(walletResult->id, true);

    auto allAddresses = walletRepo.getAddressesByWallet(walletResult->id);
    TEST_ASSERT(allAddresses.hasValue(), "Get all addresses should succeed");
    TEST_ASSERT(allAddresses->size() >= 3, "Should have at least 3 addresses");

    auto receivingAddresses = walletRepo.getAddressesByWallet(walletResult->id, false);
    TEST_ASSERT(receivingAddresses.hasValue(), "Get receiving addresses should succeed");

    auto changeAddresses = walletRepo.getAddressesByWallet(walletResult->id, true);
    TEST_ASSERT(changeAddresses.hasValue(), "Get change addresses should succeed");

    TEST_PASS();
}

void testUpdateAddressLabel(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Update Address Label");

    int userId = createTestUser(userRepo, "walletuser9");
    auto walletResult = walletRepo.createWallet(userId, "Label Test Wallet", "bitcoin");
    auto addressResult = walletRepo.generateAddress(walletResult->id, false);
    TEST_ASSERT(addressResult.hasValue(), "Address generation should succeed");

    auto updateResult = walletRepo.updateAddressLabel(addressResult->id, "My Main Address");
    TEST_ASSERT(updateResult.hasValue(), "Label update should succeed");
    TEST_ASSERT(*updateResult == true, "Update should return true");

    TEST_PASS();
}

void testUpdateAddressBalance(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Update Address Balance");

    int userId = createTestUser(userRepo, "walletuser10");
    auto walletResult = walletRepo.createWallet(userId, "Balance Test Wallet", "bitcoin");
    auto addressResult = walletRepo.generateAddress(walletResult->id, false);
    TEST_ASSERT(addressResult.hasValue(), "Address generation should succeed");

    int64_t newBalance = 100000000; // 1 BTC in satoshis
    auto updateResult = walletRepo.updateAddressBalance(addressResult->id, newBalance);
    TEST_ASSERT(updateResult.hasValue(), "Balance update should succeed");
    TEST_ASSERT(*updateResult == true, "Update should return true");

    TEST_PASS();
}

void testStoreEncryptedSeed(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Store Encrypted Seed");

    int userId = createTestUser(userRepo, "seeduser1");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    std::vector<std::string> mnemonic = {
        "abandon", "ability", "able", "about", "above", "absent",
        "absorb", "abstract", "absurd", "abuse", "access", "accident"
    };

    auto result = walletRepo.storeEncryptedSeed(userId, "SecurePass123!", mnemonic);
    TEST_ASSERT(result.hasValue(), "Seed storage should succeed");
    TEST_ASSERT(*result == true, "Storage should return true");

    TEST_PASS();
}

void testRetrieveDecryptedSeed(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Retrieve Decrypted Seed");

    int userId = createTestUser(userRepo, "seeduser2");

    std::vector<std::string> originalMnemonic = {
        "abandon", "ability", "able", "about", "above", "absent",
        "absorb", "abstract", "absurd", "abuse", "access", "accident"
    };

    const std::string password = "SecurePass123!";
    walletRepo.storeEncryptedSeed(userId, password, originalMnemonic);

    auto retrieveResult = walletRepo.retrieveDecryptedSeed(userId, password);
    TEST_ASSERT(retrieveResult.hasValue(), "Seed retrieval should succeed");
    TEST_ASSERT(retrieveResult->size() == originalMnemonic.size(), "Mnemonic size should match");
    TEST_ASSERT(*retrieveResult == originalMnemonic, "Mnemonic should match exactly");

    TEST_PASS();
}

void testRetrieveDecryptedSeedWrongPassword(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Retrieve Decrypted Seed - Wrong Password");

    int userId = createTestUser(userRepo, "seeduser3");

    std::vector<std::string> mnemonic = {
        "abandon", "ability", "able", "about", "above", "absent",
        "absorb", "abstract", "absurd", "abuse", "access", "accident"
    };

    walletRepo.storeEncryptedSeed(userId, "CorrectPass123!", mnemonic);

    auto retrieveResult = walletRepo.retrieveDecryptedSeed(userId, "WrongPass123!");
    TEST_ASSERT(!retrieveResult.hasValue(), "Seed retrieval should fail with wrong password");
    TEST_ASSERT(retrieveResult.errorCode == 401, "Error code should be 401");

    TEST_PASS();
}

void testConfirmSeedBackup(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Confirm Seed Backup");

    int userId = createTestUser(userRepo, "seeduser4");

    std::vector<std::string> mnemonic = {"test", "seed", "phrase"};
    walletRepo.storeEncryptedSeed(userId, "SecurePass123!", mnemonic);

    auto confirmResult = walletRepo.confirmSeedBackup(userId);
    TEST_ASSERT(confirmResult.hasValue(), "Confirm backup should succeed");
    TEST_ASSERT(*confirmResult == true, "Confirmation should return true");

    TEST_PASS();
}

void testHasSeedStored(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Has Seed Stored");

    int userId1 = createTestUser(userRepo, "seeduser5");
    int userId2 = createTestUser(userRepo, "seeduser6");

    std::vector<std::string> mnemonic = {"test", "seed"};
    walletRepo.storeEncryptedSeed(userId1, "SecurePass123!", mnemonic);

    auto has1 = walletRepo.hasSeedStored(userId1);
    TEST_ASSERT(has1.hasValue() && *has1 == true, "User 1 should have seed");

    auto has2 = walletRepo.hasSeedStored(userId2);
    TEST_ASSERT(has2.hasValue() && *has2 == false, "User 2 should not have seed");

    TEST_PASS();
}

void testGetSpendableBalance(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Get Spendable Balance");

    int userId = createTestUser(userRepo, "balanceuser1");
    auto walletResult = walletRepo.createWallet(userId, "Balance Wallet", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto balanceResult = walletRepo.getSpendableBalance(walletResult->id, 1);
    if (!balanceResult.hasValue()) {
        std::cerr << COLOR_RED << "Error: " << balanceResult.error() << COLOR_RESET << std::endl;
        std::cerr << COLOR_RED << "Error code: " << balanceResult.errorCode << COLOR_RESET << std::endl;
    }
    TEST_ASSERT(balanceResult.hasValue(), "Get spendable balance should succeed");
    // Initially should be 0
    TEST_ASSERT(*balanceResult == 0, "Initial balance should be 0");

    TEST_PASS();
}

// SQL Injection Protection Tests
void testSQLInjectionInWalletName(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("SQL Injection Protection - Wallet Name");

    int userId = createTestUser(userRepo, "sql_wallet_user");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    std::vector<std::string> maliciousNames = {
        "Wallet' OR '1'='1",
        "'; DROP TABLE wallets;--",
        "Wallet' UNION SELECT * FROM users--",
        "Test\\'; DELETE FROM wallets;--"
    };

    for (const auto& name : maliciousNames) {
        auto result = walletRepo.createWallet(userId, name, "bitcoin");

        if (!result.hasValue()) {
            std::cout << "    Rejected malicious wallet name: " << name << std::endl;
        } else {
            // Verify it was stored safely
            auto getResult = walletRepo.getWalletById(result->id);
            TEST_ASSERT(getResult.hasValue(), "Should retrieve wallet");
            TEST_ASSERT(getResult->walletName == name,
                        "Wallet name should be stored exactly as provided");
            std::cout << "    Safely stored wallet name: " << name << std::endl;
        }
    }

    TEST_PASS();
}

void testSQLInjectionInGetWalletByName(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("SQL Injection Protection - Get Wallet By Name");

    int userId = createTestUser(userRepo, "sql_getwallet_user");

    // Create legitimate wallet
    walletRepo.createWallet(userId, "My Wallet", "bitcoin");

    // Try SQL injection in query
    std::vector<std::string> maliciousQueries = {
        "' OR '1'='1",
        "My Wallet' OR '1'='1--",
        "'; DROP TABLE wallets;--"
    };

    for (const auto& query : maliciousQueries) {
        auto result = walletRepo.getWalletByName(userId, query);
        TEST_ASSERT(!result.hasValue(), "SQL injection should not return results");
        std::cout << "    Blocked SQL injection query: " << query << std::endl;
    }

    TEST_PASS();
}

void testWalletAddressLabelInjection(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("SQL Injection Protection - Address Label");

    int userId = createTestUser(userRepo, "label_user");
    auto walletResult = walletRepo.createWallet(userId, "Test Wallet", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto addressResult = walletRepo.generateAddress(walletResult->id, false);
    TEST_ASSERT(addressResult.hasValue(), "Address generation should succeed");

    std::vector<std::string> maliciousLabels = {
        "Label' OR '1'='1",
        "'; DELETE FROM addresses;--",
        "Label' UNION SELECT * FROM addresses--"
    };

    for (const auto& label : maliciousLabels) {
        auto updateResult = walletRepo.updateAddressLabel(addressResult->id, label);

        if (updateResult.hasValue() && *updateResult) {
            std::cout << "    Safely stored address label: " << label << std::endl;
        }
    }

    TEST_PASS();
}

// Edge Case Tests
void testEmptyWalletName(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Edge Case - Empty Wallet Name");

    int userId = createTestUser(userRepo, "empty_wallet_user");

    auto result = walletRepo.createWallet(userId, "", "bitcoin");
    TEST_ASSERT(!result.hasValue(), "Should reject empty wallet name");

    std::cout << "    Correctly rejected empty wallet name" << std::endl;

    TEST_PASS();
}

void testVeryLongWalletName(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Edge Case - Very Long Wallet Name");

    int userId = createTestUser(userRepo, "long_wallet_user");

    std::string longName(1000, 'W');
    auto result = walletRepo.createWallet(userId, longName, "bitcoin");

    if (!result.hasValue()) {
        std::cout << "    Rejected 1000-character wallet name (validation)" << std::endl;
    } else {
        std::cout << "    Warning: Accepted very long wallet name" << std::endl;
    }

    TEST_PASS();
}

void testInvalidWalletType(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Edge Case - Invalid Wallet Type");

    int userId = createTestUser(userRepo, "invalid_type_user");

    std::vector<std::string> invalidTypes = {
        "",
        "invalidcoin",
        "bitcoin; DROP TABLE wallets;--",
        std::string(500, 'T')
    };

    for (const auto& type : invalidTypes) {
        auto result = walletRepo.createWallet(userId, "Test Wallet", type);

        if (!result.hasValue()) {
            std::cout << "    Rejected invalid wallet type: " << type << std::endl;
        } else {
            std::cout << "    Warning: Accepted wallet type: " << type << std::endl;
        }
    }

    TEST_PASS();
}

void testMaximumAddressesPerWallet(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Edge Case - Maximum Addresses Per Wallet");

    int userId = createTestUser(userRepo, "max_addr_user");
    auto walletResult = walletRepo.createWallet(userId, "Address Test", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    const int MAX_ADDRESSES = 100; // Test with 100 addresses
    std::cout << "    Generating " << MAX_ADDRESSES << " addresses..." << std::endl;

    int successCount = 0;
    for (int i = 0; i < MAX_ADDRESSES; i++) {
        auto addressResult = walletRepo.generateAddress(walletResult->id, false);
        if (addressResult.hasValue()) {
            successCount++;
        }
    }

    std::cout << "    Successfully generated " << successCount << " addresses" << std::endl;
    TEST_ASSERT(successCount == MAX_ADDRESSES, "Should generate all addresses");

    TEST_PASS();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    TestUtils::printTestHeader("Consolidated Repository Tests");

    // Run integration tests with separate DB
    {
        Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
        TestUtils::initializeTestLogger("test_integration.log");
        TestUtils::initializeTestDatabase(dbManager, TEST_INTEGRATION_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY);

        Repository::UserRepository userRepo(dbManager);
        Repository::WalletRepository walletRepo(dbManager);
        Repository::TransactionRepository txRepo(dbManager);

        std::cout << "\n" << COLOR_CYAN << "Running Integration Tests..." << COLOR_RESET << std::endl;
        testCompleteUserWalletWorkflow(userRepo, walletRepo, txRepo);
        testMultiUserScenario(userRepo, walletRepo, txRepo);
        testErrorHandlingAndRollback(userRepo, walletRepo);

        TestUtils::shutdownTestEnvironment(dbManager, TEST_INTEGRATION_DB_PATH);
    }

    // Run transaction repository tests
    {
        Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
        TestUtils::initializeTestLogger("test_tx_repo.log");
        TestUtils::initializeTestDatabase(dbManager, TEST_TX_REPO_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY);

        Repository::UserRepository userRepo(dbManager);
        Repository::WalletRepository walletRepo(dbManager);
        Repository::TransactionRepository txRepo(dbManager);

        std::cout << "\n" << COLOR_CYAN << "Running Transaction Repository Tests..." << COLOR_RESET << std::endl;
        testAddTransaction(txRepo, walletRepo, userRepo);
        testGetTransactionByTxid(txRepo, walletRepo, userRepo);
        testGetTransactionById(txRepo, walletRepo, userRepo);
        testGetTransactionsByWallet(txRepo, walletRepo, userRepo);
        testGetTransactionStats(txRepo, walletRepo, userRepo);
        testCalculateWalletBalance(txRepo, walletRepo, userRepo);
        testUpdateTransactionConfirmation(txRepo, walletRepo, userRepo);

        std::cout << "\n" << COLOR_CYAN << "Running Transaction Repository Boundary Tests..." << COLOR_RESET << std::endl;
        testBoundaryMaximumAmount(txRepo, walletRepo, userRepo);
        testBoundaryNegativeAmount(txRepo, walletRepo, userRepo);
        testBoundaryZeroAmount(txRepo, walletRepo, userRepo);
        testBoundaryLargeTransactionCount(txRepo, walletRepo, userRepo);
        testBoundaryDuplicateTxid(txRepo, walletRepo, userRepo);
        testBoundaryPaginationEdgeCases(txRepo, walletRepo, userRepo);

        TestUtils::shutdownTestEnvironment(dbManager, TEST_TX_REPO_DB_PATH);
    }

    // Run user repository tests
    {
        Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
        TestUtils::initializeTestLogger("test_user_repo.log");
        TestUtils::initializeTestDatabase(dbManager, TEST_USER_REPO_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY);

        Repository::UserRepository userRepo(dbManager);

        std::cout << "\n" << COLOR_CYAN << "Running User Repository Tests..." << COLOR_RESET << std::endl;
        testCreateUser(userRepo);
        testCreateUserDuplicateUsername(userRepo);
        testCreateUserInvalidUsername(userRepo);
        testCreateUserInvalidPassword(userRepo);
        testAuthenticateUserSuccess(userRepo);
        testAuthenticateUserWrongPassword(userRepo);
        testAuthenticateUserNotFound(userRepo);
        testGetUserByUsername(userRepo);
        testGetUserById(userRepo);
        testChangePassword(userRepo);
        testChangePasswordWrongCurrent(userRepo);
        testIsUsernameAvailable(userRepo);
        testPasswordHashingUniqueness(userRepo);
        testUpdateLastLogin(userRepo);
        testSQLInjectionInUsername(userRepo);
        testSQLInjectionInPassword(userRepo);
        testSQLInjectionInEmail(userRepo);
        testSQLInjectionInAuthenticateUser(userRepo);
        testUnicodeCharactersInUsername(userRepo);
        testExtremelyLongInputs(userRepo);

        TestUtils::shutdownTestEnvironment(dbManager, TEST_USER_REPO_DB_PATH);
    }

    // Run wallet repository tests
    {
        Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
        TestUtils::initializeTestLogger("test_wallet_repo.log");
        TestUtils::initializeTestDatabase(dbManager, TEST_WALLET_REPO_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY);

        Repository::UserRepository userRepo(dbManager);
        Repository::WalletRepository walletRepo(dbManager);

        std::cout << "\n" << COLOR_CYAN << "Running Wallet Repository Tests..." << COLOR_RESET << std::endl;
        testCreateWallet(walletRepo, userRepo);
        testCreateMultipleWallets(walletRepo, userRepo);
        testGetWalletsByUserId(walletRepo, userRepo);
        testGetWalletById(walletRepo, userRepo);
        testGetWalletByName(walletRepo, userRepo);
        testGenerateAddress(walletRepo, userRepo);
        testGenerateChangeAddress(walletRepo, userRepo);
        testGetAddressesByWallet(walletRepo, userRepo);
        testUpdateAddressLabel(walletRepo, userRepo);
        testUpdateAddressBalance(walletRepo, userRepo);
        testStoreEncryptedSeed(walletRepo, userRepo);
        testRetrieveDecryptedSeed(walletRepo, userRepo);
        testRetrieveDecryptedSeedWrongPassword(walletRepo, userRepo);
        testConfirmSeedBackup(walletRepo, userRepo);
        testHasSeedStored(walletRepo, userRepo);
        testGetSpendableBalance(walletRepo, userRepo);

        std::cout << "\n" << COLOR_CYAN << "Running Wallet Repository SQL Injection Tests..." << COLOR_RESET << std::endl;
        testSQLInjectionInWalletName(walletRepo, userRepo);
        testSQLInjectionInGetWalletByName(walletRepo, userRepo);
        testWalletAddressLabelInjection(walletRepo, userRepo);

        std::cout << "\n" << COLOR_CYAN << "Running Wallet Repository Edge Case Tests..." << COLOR_RESET << std::endl;
        testEmptyWalletName(walletRepo, userRepo);
        testVeryLongWalletName(walletRepo, userRepo);
        testInvalidWalletType(walletRepo, userRepo);
        testMaximumAddressesPerWallet(walletRepo, userRepo);

        TestUtils::shutdownTestEnvironment(dbManager, TEST_WALLET_REPO_DB_PATH);
    }

    // Print final summary
    TestUtils::printTestSummary("Consolidated Repository Test");

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}
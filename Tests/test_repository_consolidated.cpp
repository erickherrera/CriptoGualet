// test_repository_consolidated.cpp - Core Repository Tests
#include "../backend/core/include/Auth.h"
#include "../backend/database/include/Database/DatabaseManager.h"
#include "../backend/repository/include/Repository/Logger.h"
#include "../backend/repository/include/Repository/TransactionRepository.h"
#include "../backend/repository/include/Repository/UserRepository.h"
#include "../backend/repository/include/Repository/WalletRepository.h"
#include "include/TestUtils.h"
#include <chrono>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

const std::string TEST_DB_PATH = "test_repo_consolidated.db";

// ============================================================================
// Core Repository Tests
// ============================================================================

void testUserCRUDAndAuth(Repository::UserRepository& userRepo) {
    TEST_START("User CRUD and Authentication");

    // Create
    auto createResult = userRepo.createUser("alice", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");
    int userId = createResult->id;

    // Authenticate (success)
    auto authResult = userRepo.authenticateUser("alice", "SecurePass123!");
    TEST_ASSERT(authResult.hasValue(), "Authentication should succeed with correct password");

    // Authenticate (failure)
    auto failAuth = userRepo.authenticateUser("alice", "WrongPass123!");
    TEST_ASSERT(!failAuth.hasValue(), "Authentication should fail with wrong password");

    // Duplicate username
    auto dupResult = userRepo.createUser("alice", "AnotherPass123!");
    TEST_ASSERT(!dupResult.hasValue(), "Duplicate username should be rejected");

    // Change password
    auto changeResult = userRepo.changePassword(userId, "SecurePass123!", "NewPass123!");
    TEST_ASSERT(changeResult.hasValue() && *changeResult, "Password change should succeed");

    // Verify new password works
    auto newAuth = userRepo.authenticateUser("alice", "NewPass123!");
    TEST_ASSERT(newAuth.hasValue(), "Should authenticate with new password");

    // SQL injection attempt
    auto injectResult = userRepo.createUser("' OR '1'='1", "SecurePass123!");
    TEST_ASSERT(!injectResult.hasValue(), "SQL injection in username should be rejected");

    TEST_PASS();
}

void testWalletAndAddressManagement(Repository::WalletRepository& walletRepo,
                                    Repository::UserRepository& userRepo) {
    TEST_START("Wallet and Address Management");

    // Create user
    auto userResult = userRepo.createUser("bob", "SecurePass123!");
    TEST_ASSERT(userResult.hasValue(), "User creation should succeed");
    int userId = userResult->id;

    // Create wallets
    auto btcWallet = walletRepo.createWallet(userId, "Bitcoin Wallet", "bitcoin");
    auto ltcWallet = walletRepo.createWallet(userId, "Litecoin Wallet", "litecoin");
    TEST_ASSERT(btcWallet.hasValue() && ltcWallet.hasValue(), "Wallet creation should succeed");
    TEST_ASSERT(btcWallet->id != ltcWallet->id, "Wallets should have unique IDs");

    // Get wallets by user
    auto walletsResult = walletRepo.getWalletsByUserId(userId);
    TEST_ASSERT(walletsResult.hasValue() && walletsResult->size() == 2,
                "Should retrieve 2 wallets for user");

    // Generate addresses
    auto addr1 = walletRepo.generateAddress(btcWallet->id, false, "Primary");
    auto addr2 = walletRepo.generateAddress(btcWallet->id, false, "Secondary");
    auto changeAddr = walletRepo.generateAddress(btcWallet->id, true, "Change");
    TEST_ASSERT(addr1.hasValue() && addr2.hasValue() && changeAddr.hasValue(),
                "Address generation should succeed");
    TEST_ASSERT(!addr1->address.empty() && addr1->address != addr2->address,
                "Addresses should be unique");

    // Update address label
    auto labelResult = walletRepo.updateAddressLabel(addr1->id, "Updated Label");
    TEST_ASSERT(labelResult.hasValue() && *labelResult, "Label update should succeed");

    TEST_PASS();
}

void testTransactionManagement(Repository::TransactionRepository& txRepo,
                               Repository::WalletRepository& walletRepo,
                               Repository::UserRepository& userRepo) {
    TEST_START("Transaction Management and Balance");

    // Setup
    auto userResult = userRepo.createUser("carol", "SecurePass123!");
    auto walletResult = walletRepo.createWallet(userResult->id, "Test Wallet", "bitcoin");
    int walletId = walletResult->id;

    // Add incoming transaction
    Repository::Transaction txIn;
    txIn.walletId = walletId;
    txIn.txid = "incoming_tx_001";
    txIn.amountSatoshis = 100000000;  // 1 BTC
    txIn.feeSatoshis = 0;
    txIn.direction = "incoming";
    txIn.toAddress = "bc1qtest123";
    txIn.confirmationCount = 6;
    txIn.isConfirmed = true;

    auto inResult = txRepo.addTransaction(txIn);
    TEST_ASSERT(inResult.hasValue(), "Incoming transaction should be added");

    // Add outgoing transaction
    Repository::Transaction txOut;
    txOut.walletId = walletId;
    txOut.txid = "outgoing_tx_001";
    txOut.amountSatoshis = 30000000;  // 0.3 BTC
    txOut.feeSatoshis = 10000;
    txOut.direction = "outgoing";
    txOut.fromAddress = "bc1qtest123";
    txOut.confirmationCount = 3;
    txOut.isConfirmed = true;

    auto outResult = txRepo.addTransaction(txOut);
    TEST_ASSERT(outResult.hasValue(), "Outgoing transaction should be added");

    // Get transaction by TXID
    auto getResult = txRepo.getTransactionByTxid("incoming_tx_001");
    TEST_ASSERT(getResult.hasValue() && getResult->txid == "incoming_tx_001",
                "Should retrieve transaction by TXID");

    // Get transactions by wallet
    Repository::PaginationParams params;
    params.limit = 10;
    params.offset = 0;
    auto txList = txRepo.getTransactionsByWallet(walletId, params);
    TEST_ASSERT(txList.hasValue() && txList->items.size() == 2,
                "Should retrieve 2 transactions");

    // Calculate balance
    auto balanceResult = txRepo.calculateWalletBalance(walletId);
    TEST_ASSERT(balanceResult.hasValue(), "Balance calculation should succeed");
    // 1 BTC - (0.3 BTC + 0.0001 BTC fee) = 0.6999 BTC = 69990000 satoshis
    TEST_ASSERT(balanceResult->confirmedBalance == 69990000,
                "Confirmed balance should be correct");

    // Get stats
    auto statsResult = txRepo.getTransactionStats(walletId);
    TEST_ASSERT(statsResult.hasValue() && statsResult->totalTransactions == 2,
                "Should have 2 total transactions");

    // Update confirmation
    auto updateResult = txRepo.updateTransactionConfirmation("incoming_tx_001", 700000, "blockhash", 10);
    TEST_ASSERT(updateResult.hasValue(), "Confirmation update should succeed");

    // Duplicate TXID should fail
    Repository::Transaction dupTx;
    dupTx.walletId = walletId;
    dupTx.txid = "incoming_tx_001";  // Same TXID
    dupTx.amountSatoshis = 50000000;
    auto dupResult = txRepo.addTransaction(dupTx);
    TEST_ASSERT(!dupResult.hasValue(), "Duplicate TXID should be rejected");

    TEST_PASS();
}

void testSeedEncryption(Repository::WalletRepository& walletRepo,
                        Repository::UserRepository& userRepo) {
    TEST_START("Seed Encryption and Decryption");

    auto userResult = userRepo.createUser("seeduser", "SecurePass123!");
    int userId = userResult->id;

    // Store seed
    std::vector<std::string> mnemonic = {"abandon", "ability", "able",   "about",
                                         "above",   "absent",  "absorb", "abstract",
                                         "absurd",  "abuse",   "access", "accident"};

    auto storeResult = walletRepo.storeEncryptedSeed(userId, "SecurePass123!", mnemonic);
    TEST_ASSERT(storeResult.hasValue() && *storeResult, "Seed storage should succeed");

    // Retrieve seed
    auto retrieveResult = walletRepo.retrieveDecryptedSeed(userId, "SecurePass123!");
    TEST_ASSERT(retrieveResult.hasValue(), "Seed retrieval should succeed");
    TEST_ASSERT(*retrieveResult == mnemonic, "Retrieved seed should match original");

    // Wrong password should fail
    auto wrongResult = walletRepo.retrieveDecryptedSeed(userId, "WrongPass123!");
    TEST_ASSERT(!wrongResult.hasValue(), "Retrieval with wrong password should fail");

    // Confirm backup
    auto confirmResult = walletRepo.confirmSeedBackup(userId);
    TEST_ASSERT(confirmResult.hasValue() && *confirmResult, "Backup confirmation should succeed");

    // Check if seed exists
    auto hasSeed = walletRepo.hasSeedStored(userId);
    TEST_ASSERT(hasSeed.hasValue() && *hasSeed, "Should detect stored seed");

    TEST_PASS();
}

void testEndToEndWorkflow(Repository::UserRepository& userRepo,
                          Repository::WalletRepository& walletRepo,
                          Repository::TransactionRepository& txRepo) {
    TEST_START("End-to-End User Workflow");

    // 1. Create user
    auto userResult = userRepo.createUser("workflow", "SecurePass123!");
    TEST_ASSERT(userResult.hasValue(), "User creation should succeed");
    int userId = userResult->id;

    // 2. Store seed
    std::vector<std::string> mnemonic = {"test", "seed", "phrase", "words", "here", "for",
                                         "wallet", "creation", "process", "testing", "end", "to"};
    auto seedResult = walletRepo.storeEncryptedSeed(userId, "SecurePass123!", mnemonic);
    TEST_ASSERT(seedResult.hasValue(), "Seed storage should succeed");

    // 3. Create wallet
    auto walletResult = walletRepo.createWallet(userId, "Main Wallet", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");
    int walletId = walletResult->id;

    // 4. Generate addresses
    auto recvAddr = walletRepo.generateAddress(walletId, false, "Receiving");
    auto changeAddr = walletRepo.generateAddress(walletId, true, "Change");
    TEST_ASSERT(recvAddr.hasValue() && changeAddr.hasValue(), "Address generation should succeed");

    // 5. Add incoming transaction
    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "workflow_incoming_tx";
    tx.amountSatoshis = 50000000;  // 0.5 BTC
    tx.feeSatoshis = 0;
    tx.direction = "incoming";
    tx.toAddress = recvAddr->address;
    tx.confirmationCount = 6;
    tx.isConfirmed = true;
    tx.memo = "Initial deposit";

    auto txResult = txRepo.addTransaction(tx);
    TEST_ASSERT(txResult.hasValue(), "Transaction should be added");

    // 6. Verify balance
    auto balanceResult = txRepo.calculateWalletBalance(walletId);
    TEST_ASSERT(balanceResult.hasValue() && balanceResult->confirmedBalance == 50000000,
                "Balance should be 0.5 BTC");

    // 7. Retrieve and verify seed
    auto retrieveResult = walletRepo.retrieveDecryptedSeed(userId, "SecurePass123!");
    TEST_ASSERT(retrieveResult.hasValue() && *retrieveResult == mnemonic,
                "Seed should be retrievable and match");

    // 8. Confirm backup
    auto confirmResult = walletRepo.confirmSeedBackup(userId);
    TEST_ASSERT(confirmResult.hasValue(), "Backup confirmation should succeed");

    TEST_PASS();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    TestUtils::printTestHeader("Repository Core Tests");

    // Setup
    Database::DatabaseManager dbManager;
    TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH,
                                        TestUtils::STANDARD_TEST_ENCRYPTION_KEY);

    Repository::UserRepository userRepo(dbManager);
    Repository::WalletRepository walletRepo(dbManager);
    Repository::TransactionRepository txRepo(dbManager);

    // Run tests
    testUserCRUDAndAuth(userRepo);
    testWalletAndAddressManagement(walletRepo, userRepo);
    testTransactionManagement(txRepo, walletRepo, userRepo);
    testSeedEncryption(walletRepo, userRepo);
    testEndToEndWorkflow(userRepo, walletRepo, txRepo);

    // Cleanup
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);
    TestUtils::printTestSummary("Repository Tests");

    return (TestGlobals::g_testsFailed > 0) ? 1 : 0;
}

/**
 * @file test_repository_integration.cpp
 * @brief Integration tests for Repository layer
 *
 * Tests complete workflows: User → Wallet → Addresses → Transactions
 */

#include "TestUtils.h"
#include "Repository/UserRepository.h"
#include "Repository/WalletRepository.h"
#include "Repository/TransactionRepository.h"

const std::string TEST_DB_PATH = "test_integration.db";

// ============================================================================
// Integration Test Cases
// ============================================================================

bool testCompleteUserWalletWorkflow(Repository::UserRepository& userRepo,
                                     Repository::WalletRepository& walletRepo,
                                     Repository::TransactionRepository& txRepo) {
    TEST_START("Complete User → Wallet → Addresses → Transactions Workflow");

    // Step 1: Create User
    TEST_STEP("Creating user 'alice'");
    auto userResult = userRepo.createUser("alice", "alice@example.com", "SecurePass123!");
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

bool testMultiUserScenario(Repository::UserRepository& userRepo,
                           Repository::WalletRepository& walletRepo,
                           Repository::TransactionRepository& txRepo) {
    TEST_START("Multi-User Scenario with Wallet Isolation");

    // Create User 1
    TEST_STEP("Creating User 1 (bob)");
    auto user1 = userRepo.createUser("bob", "bob@example.com", "BobPass123!");
    TEST_ASSERT(user1.hasValue(), "User 1 creation should succeed");

    // Create User 2
    TEST_STEP("Creating User 2 (carol)");
    auto user2 = userRepo.createUser("carol", "carol@example.com", "CarolPass123!");
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

bool testErrorHandlingAndRollback(Repository::UserRepository& userRepo,
                                   Repository::WalletRepository& walletRepo) {
    TEST_START("Error Handling and Transaction Rollback");

    // Test duplicate username
    TEST_STEP("Testing duplicate username detection");
    userRepo.createUser("duplicate", "duplicate@example.com", "Pass123!");
    auto duplicateResult = userRepo.createUser("duplicate", "different@example.com", "Pass123!");
    TEST_ASSERT(!duplicateResult.hasValue(), "Duplicate username should fail");
    TEST_ASSERT(duplicateResult.errorCode == 409, "Error code should be 409");

    // Test wrong password
    TEST_STEP("Testing authentication with wrong password");
    auto authResult = userRepo.authenticateUser("duplicate", "WrongPass123!");
    TEST_ASSERT(!authResult.hasValue(), "Wrong password should fail");
    TEST_ASSERT(authResult.errorCode == 401, "Error code should be 401");

    // Test invalid wallet name
    TEST_STEP("Testing invalid wallet creation");
    auto user = userRepo.createUser("testuser", "test@example.com", "Pass123!");
    auto invalidWallet = walletRepo.createWallet(user->id, "", "bitcoin"); // Empty name
    TEST_ASSERT(!invalidWallet.hasValue(), "Empty wallet name should fail");

    TEST_PASS();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    TestUtils::printTestHeader("Repository Integration Tests");

    Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
    TestUtils::initializeTestLogger("test_integration.log");
    TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY);

    Repository::UserRepository userRepo(dbManager);
    Repository::WalletRepository walletRepo(dbManager);
    Repository::TransactionRepository txRepo(dbManager);

    // Run integration tests
    testCompleteUserWalletWorkflow(userRepo, walletRepo, txRepo);
    testMultiUserScenario(userRepo, walletRepo, txRepo);
    testErrorHandlingAndRollback(userRepo, walletRepo);

    // Print summary and cleanup
    TestUtils::printTestSummary("Integration Test");
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

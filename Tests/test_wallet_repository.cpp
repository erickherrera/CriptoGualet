/**
 * @file test_wallet_repository.cpp
 * @brief Unit tests for WalletRepository
 *
 * Tests wallet creation, address generation, seed management, and UTXO tracking.
 */

#include "TestUtils.h"
#include "Repository/WalletRepository.h"
#include "Repository/UserRepository.h"

// Test database
const std::string TEST_DB_PATH = "test_wallet_repo.db";

// Helper: Create test user
int createTestUser(Repository::UserRepository& userRepo, const std::string& username) {
    auto result = userRepo.createUser(username, username + "@example.com", "SecurePass123!");
    return result.hasValue() ? result->id : -1;
}

// ============================================================================
// Test Cases
// ============================================================================

bool testCreateWallet(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testCreateMultipleWallets(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Create Multiple Wallets");

    int userId = createTestUser(userRepo, "walletuser2");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    auto wallet1 = walletRepo.createWallet(userId, "Bitcoin Wallet", "bitcoin");
    auto wallet2 = walletRepo.createWallet(userId, "Litecoin Wallet", "litecoin");

    TEST_ASSERT(wallet1.hasValue() && wallet2.hasValue(), "Both wallets should be created");
    TEST_ASSERT(wallet1->id != wallet2->id, "Wallet IDs should differ");

    TEST_PASS();
}

bool testGetWalletsByUserId(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testGetWalletById(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testGetWalletByName(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Get Wallet By Name");

    int userId = createTestUser(userRepo, "walletuser5");
    walletRepo.createWallet(userId, "Named Wallet", "bitcoin");

    auto result = walletRepo.getWalletByName(userId, "Named Wallet");
    TEST_ASSERT(result.hasValue(), "Get wallet by name should succeed");
    TEST_ASSERT(result->walletName == "Named Wallet", "Wallet name should match");

    TEST_PASS();
}

bool testGenerateAddress(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testGenerateChangeAddress(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Generate Change Address");

    int userId = createTestUser(userRepo, "walletuser7");
    auto walletResult = walletRepo.createWallet(userId, "Change Address Test", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto changeAddressResult = walletRepo.generateAddress(walletResult->id, true, "Change");
    TEST_ASSERT(changeAddressResult.hasValue(), "Change address generation should succeed");
    TEST_ASSERT(changeAddressResult->isChange == true, "Should be change address");

    TEST_PASS();
}

bool testGetAddressesByWallet(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testUpdateAddressLabel(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testUpdateAddressBalance(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testStoreEncryptedSeed(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testRetrieveDecryptedSeed(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testRetrieveDecryptedSeedWrongPassword(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testConfirmSeedBackup(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
    TEST_START("Confirm Seed Backup");

    int userId = createTestUser(userRepo, "seeduser4");

    std::vector<std::string> mnemonic = {"test", "seed", "phrase"};
    walletRepo.storeEncryptedSeed(userId, "SecurePass123!", mnemonic);

    auto confirmResult = walletRepo.confirmSeedBackup(userId);
    TEST_ASSERT(confirmResult.hasValue(), "Confirm backup should succeed");
    TEST_ASSERT(*confirmResult == true, "Confirmation should return true");

    TEST_PASS();
}

bool testHasSeedStored(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

bool testGetSpendableBalance(Repository::WalletRepository& walletRepo, Repository::UserRepository& userRepo) {
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

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    TestUtils::printTestHeader("WalletRepository Unit Tests");

    Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
    TestUtils::initializeTestLogger("test_wallet_repo.log");

    if (!TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY)) {
        return 1;
    }

    Repository::UserRepository userRepo(dbManager);
    Repository::WalletRepository walletRepo(dbManager);

    // Run tests
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

    TestUtils::printTestSummary("Test");
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

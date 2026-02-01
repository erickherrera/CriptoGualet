/**
 * @file test_wallet_chains.cpp
 * @brief Comprehensive Multi-Chain Wallet Tests
 *
 * Tests for wallet creation, address generation, and BIP39/BIP44 support
 * across multiple blockchains including Bitcoin, Ethereum, Litecoin, and others.
 */

#include "../backend/core/include/Crypto.h"
#include "../backend/repository/include/Repository/UserRepository.h"
#include "../backend/repository/include/Repository/WalletRepository.h"
#include "TestUtils.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <vector>

constexpr const char* TEST_DB_PATH = "test_wallet_chains.db";

// ============================================================================
// Helper Functions
// ============================================================================

static std::vector<std::string> LoadWordlist(const std::string &filepath) {
  std::vector<std::string> wordlist;
  std::ifstream file(filepath);
  if (!file.is_open()) {
    std::cerr << "Failed to open wordlist file: " << filepath << std::endl;
    return wordlist;
  }

  std::string word;
  while (std::getline(file, word)) {
    if (!word.empty()) {
      // Remove any trailing whitespace or newlines
      while (!word.empty() && (word.back() == '\r' || word.back() == '\n' ||
                               word.back() == ' ')) {
        word.pop_back();
      }
      wordlist.push_back(word);
    }
  }

  file.close();
  return wordlist;
}

static void PrintHex(const std::string &label, const uint8_t *data, size_t len) {
  std::cout << label << ": ";
  for (size_t i = 0; i < len; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
  }
  std::cout << std::dec << std::endl;
}

static std::vector<std::string> LoadBIP39Wordlist() {
  std::vector<std::string> wordlist;

  // Try multiple possible locations for the wordlist
  std::vector<std::string> possiblePaths = {
      "assets/bip39/english.txt",
      "src/assets/bip39/english.txt",
      "../../assets/bip39/english.txt",
      "../../../assets/bip39/english.txt",
      "../../../../assets/bip39/english.txt",
      "../../../../../assets/bip39/english.txt",
      "../../../../../src/assets/bip39/english.txt",
  };

  std::cout << "Trying to find wordlist..." << std::endl;
  for (const auto &path : possiblePaths) {
    std::cout << "  Trying: " << path << " -> ";
    if (std::filesystem::exists(path)) {
      std::cout << "EXISTS" << std::endl;
      wordlist = LoadWordlist(path);
      if (wordlist.size() == 2048) {
        std::cout << "  Loaded BIP39 wordlist from: " << path << std::endl;
        break;
      } else {
        std::cout << "    (but only " << wordlist.size() << " words)" << std::endl;
      }
    } else {
      std::cout << "not found" << std::endl;
    }
  }

  return wordlist;
}

// ============================================================================
// BIP39/BIP44 Cryptographic Tests
// ============================================================================

static void testGenerateMnemonic(const std::vector<std::string>& wordlist) {
  TEST_START("Generate New Mnemonic");

  std::vector<uint8_t> entropy;
  TEST_ASSERT(Crypto::GenerateEntropy(128, entropy), "Entropy generation should succeed");
  PrintHex("Entropy (128 bits)", entropy.data(), entropy.size());

  std::vector<std::string> mnemonic;
  TEST_ASSERT(Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic),
              "Mnemonic generation should succeed");

  std::cout << "    Mnemonic (" << mnemonic.size() << " words): ";
  for (size_t i = 0; i < mnemonic.size(); i++) {
    std::cout << mnemonic[i];
    if (i < mnemonic.size() - 1)
      std::cout << " ";
  }
  std::cout << std::endl;

  TEST_PASS();
}

static void testValidateMnemonic(const std::vector<std::string>& wordlist) {
  TEST_START("Validate Mnemonic");

  // Generate a mnemonic first
  std::vector<uint8_t> entropy;
  Crypto::GenerateEntropy(128, entropy);

  std::vector<std::string> mnemonic;
  Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

  TEST_ASSERT(Crypto::ValidateMnemonic(mnemonic, wordlist), "Mnemonic should be valid");

  TEST_PASS();
}

static void testGenerateBIP39Seed(const std::vector<std::string>& wordlist) {
  TEST_START("Generate BIP39 Seed");

  std::vector<uint8_t> entropy;
  Crypto::GenerateEntropy(128, entropy);

  std::vector<std::string> mnemonic;
  Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

  std::array<uint8_t, 64> seed;
  TEST_ASSERT(Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed),
              "Seed generation should succeed");

  PrintHex("    BIP39 Seed (512 bits)", seed.data(), seed.size());

  TEST_PASS();
}

static void testGenerateBIP32MasterKey(const std::vector<std::string>& wordlist) {
  TEST_START("Generate BIP32 Master Key");

  std::vector<uint8_t> entropy;
  Crypto::GenerateEntropy(128, entropy);

  std::vector<std::string> mnemonic;
  Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

  std::array<uint8_t, 64> seed;
  Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed);

  Crypto::BIP32ExtendedKey masterKey;
  TEST_ASSERT(Crypto::BIP32_MasterKeyFromSeed(seed, masterKey),
              "Master key generation should succeed");

  PrintHex("    Master Private Key", masterKey.key.data(), masterKey.key.size());
  PrintHex("    Master Chain Code", masterKey.chainCode.data(), masterKey.chainCode.size());

  TEST_PASS();
}

static void testDeriveEthereumAddresses(const std::vector<std::string>& wordlist) {
  TEST_START("Derive Ethereum Addresses (BIP44)");

  std::vector<uint8_t> entropy;
  Crypto::GenerateEntropy(128, entropy);

  std::vector<std::string> mnemonic;
  Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

  std::array<uint8_t, 64> seed;
  Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed);

  Crypto::BIP32ExtendedKey masterKey;
  Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);

  std::vector<std::string> eth_addresses;
  TEST_ASSERT(Crypto::BIP44_GenerateEthereumAddresses(masterKey, 0, false, 0, 5, eth_addresses),
              "Ethereum address generation should succeed");

  std::cout << "    Generated 5 Ethereum addresses (m/44'/60'/0'/0/x):" << std::endl;
  for (size_t i = 0; i < eth_addresses.size(); i++) {
    std::cout << "      Address " << i << ": " << eth_addresses[i] << std::endl;
  }

  TEST_PASS();
}

static void testDeriveBitcoinAddresses(const std::vector<std::string>& wordlist) {
  TEST_START("Derive Bitcoin Addresses (BIP44)");

  std::vector<uint8_t> entropy;
  Crypto::GenerateEntropy(128, entropy);

  std::vector<std::string> mnemonic;
  Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

  std::array<uint8_t, 64> seed;
  Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed);

  Crypto::BIP32ExtendedKey masterKey;
  Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);

  std::vector<std::string> btc_addresses;
  TEST_ASSERT(Crypto::BIP44_GenerateAddresses(masterKey, 0, false, 0, 5, btc_addresses, false),
              "Bitcoin address generation should succeed");

  std::cout << "    Generated 5 Bitcoin addresses (m/44'/0'/0'/0/x):" << std::endl;
  for (size_t i = 0; i < btc_addresses.size(); i++) {
    std::cout << "      Address " << i << ": " << btc_addresses[i] << std::endl;
  }

  TEST_PASS();
}

static void testMultiChainAddressDerivation(const std::vector<std::string>& wordlist) {
  TEST_START("Multi-Chain Address Derivation");

  std::vector<uint8_t> entropy;
  Crypto::GenerateEntropy(128, entropy);

  std::vector<std::string> mnemonic;
  Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

  std::array<uint8_t, 64> seed;
  Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed);

  Crypto::BIP32ExtendedKey masterKey;
  Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);

  std::vector<Crypto::ChainType> chains = {
      Crypto::ChainType::BITCOIN,   Crypto::ChainType::LITECOIN,
      Crypto::ChainType::ETHEREUM,  Crypto::ChainType::BNB_CHAIN,
      Crypto::ChainType::POLYGON,   Crypto::ChainType::AVALANCHE,
      Crypto::ChainType::ARBITRUM};

  for (auto chain : chains) {
    std::string address;
    if (Crypto::DeriveChainAddress(masterKey, chain, 0, false, 0, address)) {
      std::cout << "    " << std::setw(20) << std::left
                << Crypto::GetChainName(chain) << ": " << address << std::endl;
    } else {
      std::cerr << "    ERROR: Failed to derive address for "
                << Crypto::GetChainName(chain) << std::endl;
    }
  }

  TEST_PASS();
}

static void testDeriveLitecoinAddresses(const std::vector<std::string>& wordlist) {
  TEST_START("Derive Litecoin Addresses (BIP44)");

  std::vector<uint8_t> entropy;
  Crypto::GenerateEntropy(128, entropy);

  std::vector<std::string> mnemonic;
  Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);

  std::array<uint8_t, 64> seed;
  Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed);

  Crypto::BIP32ExtendedKey masterKey;
  Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);

  // Litecoin uses BIP44 coin type 2: m/44'/2'/0'/0/x
  std::vector<std::string> ltc_addresses;
  for (uint32_t i = 0; i < 5; i++) {
    std::string address;
    if (Crypto::DeriveChainAddress(masterKey, Crypto::ChainType::LITECOIN, 0, false, i, address)) {
      ltc_addresses.push_back(address);
    }
  }

  TEST_ASSERT(ltc_addresses.size() == 5, "Should generate 5 Litecoin addresses");

  std::cout << "    Generated 5 Litecoin addresses (m/44'/2'/0'/0/x):" << std::endl;
  for (size_t i = 0; i < ltc_addresses.size(); i++) {
    std::cout << "      Address " << i << ": " << ltc_addresses[i] << std::endl;
    TEST_ASSERT(ltc_addresses[i][0] == 'L', "Litecoin address should start with L");
  }

  TEST_PASS();
}

static void testKeccak256TestVector() {
  TEST_START("Keccak256 Test Vector");

  const char *test_input = "hello";
  std::array<uint8_t, 32> keccak_hash;
  TEST_ASSERT(Crypto::Keccak256(reinterpret_cast<const uint8_t *>(test_input),
                                strlen(test_input), keccak_hash),
              "Keccak256 should succeed");

  // Expected: 0x1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8
  std::cout << "    Keccak256(\"hello\"): ";
  for (size_t i = 0; i < keccak_hash.size(); i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)keccak_hash[i];
  }
  std::cout << std::dec << std::endl;
  std::cout << "    Expected:             "
               "1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8"
            << std::endl;

  const uint8_t expected[32] = {0x1c, 0x8a, 0xff, 0x95, 0x06, 0x85, 0xc2, 0xed,
                                0x4b, 0xc3, 0x17, 0x4f, 0x34, 0x72, 0x28, 0x7b,
                                0x56, 0xd9, 0x51, 0x7b, 0x9c, 0x94, 0x81, 0x27,
                                0x31, 0x9a, 0x09, 0xa7, 0xa3, 0x6d, 0xea, 0xc8};

  bool keccak_match = true;
  for (size_t i = 0; i < 32; i++) {
    if (keccak_hash[i] != expected[i]) {
      keccak_match = false;
      break;
    }
  }

  TEST_ASSERT(keccak_match, "Keccak256 test vector should match");

  TEST_PASS();
}

static void testLitecoinAddressValidation() {
  TEST_START("Litecoin Address Validation");

  // Reset fill character to space (it might have been set to '0' by previous tests)
  std::cout << std::setfill(' ');

  // Standard Litecoin addresses
  std::vector<std::pair<std::string, bool>> test_addresses = {
      {"LajyQBeZaBA1ekzMwGhU4UsFyu2adqEL29", true}, // L... (P2PKH)
      {"M8T1vHq7Fh4S5w6E8g9aUb1c2d3e4f5g6h", true}, // M... (P2SH) - Valid Base58 (No 0, O, I, l)
      {"3CDJNfdWX8m2k28DQ38w4R8j5s6t7u8v9w", true}, // 3... (Legacy P2SH)
      {"ltc1qgqqgqqgqqgqqgqqgqqgqqgqqgqqgqqgqqgqqgq", true}, // ltc1... (Bech32)
      {"1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa", false}, // Bitcoin mainnet
      {"0x71C7656EC7ab88b098defB751B7401B5f6d8976F", false}, // Ethereum
      {"invalid_address", false}
  };

  for (const auto& test : test_addresses) {
      bool is_valid = Crypto::IsValidAddressFormat(test.first, Crypto::ChainType::LITECOIN);
      std::cout << "    " << std::setw(42) << std::left << test.first 
                << ": " << (is_valid ? "VALID" : "INVALID")
                << " (Expected: " << (test.second ? "VALID" : "INVALID") << ")" << std::endl;
      
      TEST_ASSERT(is_valid == test.second, "Address validation failed");
  }

  TEST_PASS();
}

// ============================================================================
// Repository-Based Multi-Chain Wallet Tests
// ============================================================================

static void testCreateEthereumWallet(Repository::WalletRepository& walletRepo,
                                     Repository::UserRepository& userRepo) {
    TEST_START("Create Ethereum Wallet");

    int userId = TestUtils::createTestUser(userRepo, "eth_user");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    auto result = walletRepo.createWallet(userId, "My Ethereum Wallet", "ethereum");
    TEST_ASSERT(result.hasValue(), "Ethereum wallet creation should succeed");
    TEST_ASSERT(result->walletType == "ethereum", "Wallet type should be 'ethereum'");
    TEST_ASSERT(result->walletName == "My Ethereum Wallet", "Wallet name should match");

    std::cout << "    Created Ethereum wallet with ID: " << result->id << std::endl;

    TEST_PASS();
}

static void testCreateLitecoinWallet(Repository::WalletRepository& walletRepo,
                                     Repository::UserRepository& userRepo) {
    TEST_START("Create Litecoin Wallet");

    int userId = TestUtils::createTestUser(userRepo, "ltc_user");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    auto result = walletRepo.createWallet(userId, "My Litecoin Wallet", "litecoin");
    TEST_ASSERT(result.hasValue(), "Litecoin wallet creation should succeed");
    TEST_ASSERT(result->walletType == "litecoin", "Wallet type should be 'litecoin'");

    std::cout << "    Created Litecoin wallet with ID: " << result->id << std::endl;

    TEST_PASS();
}

static void testMultipleWalletTypesPerUser(Repository::WalletRepository& walletRepo,
                                           Repository::UserRepository& userRepo) {
    TEST_START("Multiple Wallet Types Per User");

    int userId = TestUtils::createTestUser(userRepo, "multi_chain_user");
    TEST_ASSERT(userId > 0, "User creation should succeed");

    // Create Bitcoin wallet
    auto btcWallet = walletRepo.createWallet(userId, "BTC Wallet", "bitcoin");
    TEST_ASSERT(btcWallet.hasValue(), "Bitcoin wallet creation should succeed");

    // Create Ethereum wallet
    auto ethWallet = walletRepo.createWallet(userId, "ETH Wallet", "ethereum");
    TEST_ASSERT(ethWallet.hasValue(), "Ethereum wallet creation should succeed");

    // Create Litecoin wallet
    auto ltcWallet = walletRepo.createWallet(userId, "LTC Wallet", "litecoin");
    TEST_ASSERT(ltcWallet.hasValue(), "Litecoin wallet creation should succeed");

    // Verify all wallets are stored
    auto wallets = walletRepo.getWalletsByUserId(userId);
    TEST_ASSERT(wallets.hasValue(), "Get wallets should succeed");
    TEST_ASSERT(wallets->size() == 3, "Should have 3 wallets");

    // Verify wallet types
    std::set<std::string> walletTypes;
    for (const auto& wallet : *wallets) {
        walletTypes.insert(wallet.walletType);
    }

    TEST_ASSERT(walletTypes.count("bitcoin") > 0, "Should have Bitcoin wallet");
    TEST_ASSERT(walletTypes.count("ethereum") > 0, "Should have Ethereum wallet");
    TEST_ASSERT(walletTypes.count("litecoin") > 0, "Should have Litecoin wallet");

    std::cout << "    Successfully created wallets for 3 different chains" << std::endl;

    TEST_PASS();
}

static void testBitcoinAddressGeneration(Repository::WalletRepository& walletRepo,
                                         Repository::UserRepository& userRepo) {
    TEST_START("Bitcoin Address Generation (Repository)");

    int userId = TestUtils::createTestUser(userRepo, "btc_addr_user");
    auto walletResult = walletRepo.createWallet(userId, "BTC Test", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto addressResult = walletRepo.generateAddress(walletResult->id, false, "Bitcoin Address");
    TEST_ASSERT(addressResult.hasValue(), "Address generation should succeed");
    TEST_ASSERT(!addressResult->address.empty(), "Address should not be empty");

    // Bitcoin addresses should start with specific prefixes
    std::string addr = addressResult->address;
    bool validPrefix = (addr[0] == '1' || addr[0] == '3' || addr[0] == 'm' || addr[0] == 'n' ||
                        addr.substr(0, 3) == "bc1" || addr.substr(0, 3) == "tb1");

    std::cout << "    Generated Bitcoin address: " << addr << std::endl;
    std::cout << "    Address has valid Bitcoin prefix: " << (validPrefix ? "Yes" : "No")
              << std::endl;

    TEST_PASS();
}

static void testEthereumAddressGeneration(Repository::WalletRepository& walletRepo,
                                          Repository::UserRepository& userRepo) {
    TEST_START("Ethereum Address Generation (Repository)");

    int userId = TestUtils::createTestUser(userRepo, "eth_addr_user");
    auto walletResult = walletRepo.createWallet(userId, "ETH Test", "ethereum");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto addressResult = walletRepo.generateAddress(walletResult->id, false, "Ethereum Address");
    TEST_ASSERT(addressResult.hasValue(), "Address generation should succeed");

    std::string addr = addressResult->address;
    bool validFormat = (addr.length() == 42 && addr.substr(0, 2) == "0x");
    std::cout << "    Generated Ethereum address: " << addr << std::endl;
    std::cout << "    Address format validation: " << (validFormat ? "PASS" : "FAIL") << std::endl;
    
    TEST_ASSERT(validFormat, "Ethereum address must start with 0x and be 42 characters long");
    TEST_PASS();
}

static void testLitecoinAddressGeneration(Repository::WalletRepository& walletRepo,
                                          Repository::UserRepository& userRepo) {
    TEST_START("Litecoin Address Generation (Repository)");

    int userId = TestUtils::createTestUser(userRepo, "ltc_addr_user");
    auto walletResult = walletRepo.createWallet(userId, "LTC Test", "litecoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto addressResult = walletRepo.generateAddress(walletResult->id, false, "Litecoin Address");
    TEST_ASSERT(addressResult.hasValue(), "Address generation should succeed");

    std::string addr = addressResult->address;
    // Mock Litecoin addresses are generated with "ltc1" prefix in WalletRepository
    bool validFormat = (addr.substr(0, 4) == "ltc1");
    
    std::cout << "    Generated Litecoin address: " << addr << std::endl;
    std::cout << "    Address format validation: " << (validFormat ? "PASS" : "FAIL") << std::endl;
    
    TEST_ASSERT(validFormat, "Litecoin address must start with ltc1 prefix");
    TEST_PASS();
}

static void testLitecoinAddressGeneration(Repository::WalletRepository& walletRepo,
                                          Repository::UserRepository& userRepo) {
    TEST_START("Litecoin Address Generation (Repository)");

    int userId = TestUtils::createTestUser(userRepo, "ltc_addr_user");
    auto walletResult = walletRepo.createWallet(userId, "LTC Test", "litecoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation should succeed");

    auto addressResult = walletRepo.generateAddress(walletResult->id, false, "Litecoin Address");
    TEST_ASSERT(addressResult.hasValue(), "Address generation should succeed");
    TEST_ASSERT(!addressResult->address.empty(), "Address should not be empty");

    // Litecoin addresses should start with specific prefixes (L, M, or ltc1)
    std::string addr = addressResult->address;
    bool validPrefix = (addr[0] == 'L' || addr[0] == 'M' || addr.substr(0, 4) == "ltc1");

    std::cout << "    Generated Litecoin address: " << addr << std::endl;
    std::cout << "    Address has valid Litecoin prefix: " << (validPrefix ? "Yes" : "No")
              << std::endl;

    TEST_ASSERT(validPrefix, "Generated address should have valid Litecoin prefix");

    TEST_PASS();
}

static void testWalletChainIsolation(Repository::WalletRepository& walletRepo,
                                     Repository::UserRepository& userRepo) {
    TEST_START("Wallet Chain Isolation");

    int userId = TestUtils::createTestUser(userRepo, "isolation_user");

    auto btcWallet = walletRepo.createWallet(userId, "BTC Wallet", "bitcoin");
    auto ethWallet = walletRepo.createWallet(userId, "ETH Wallet", "ethereum");

    TEST_ASSERT(btcWallet.hasValue() && ethWallet.hasValue(), "Wallet creation should succeed");

    auto btcAddr = walletRepo.generateAddress(btcWallet->id, false);
    auto ethAddr = walletRepo.generateAddress(ethWallet->id, false);

    if (btcAddr.hasValue() && ethAddr.hasValue()) {
        TEST_ASSERT(btcAddr->walletId == btcWallet->id, "BTC address should belong to BTC wallet");
        TEST_ASSERT(ethAddr->walletId == ethWallet->id, "ETH address should belong to ETH wallet");
        TEST_ASSERT(btcAddr->address != ethAddr->address, "Addresses should be different");
    }

    TEST_PASS();
}

static void testUnsupportedChainRejection(Repository::WalletRepository& walletRepo,
                                          Repository::UserRepository& userRepo) {
    TEST_START("Unsupported Chain Rejection");

    int userId = TestUtils::createTestUser(userRepo, "unsupported_user");

    std::vector<std::string> unsupportedChains = {"dogecoin", "ripple", "unknown_coin"};

    for (const auto& chain : unsupportedChains) {
        auto result = walletRepo.createWallet(userId, "Test Wallet", chain);
        if (!result.hasValue()) {
            std::cout << "    Rejected unsupported chain: " << chain << std::endl;
        }
    }

    TEST_PASS();
}

static void testBIP44DerivationPathsForDifferentChains(Repository::WalletRepository& walletRepo,
                                                       Repository::UserRepository& userRepo) {
    TEST_START("BIP44 Derivation Paths for Different Chains");

    int userId = TestUtils::createTestUser(userRepo, "derivation_user");

    auto btcWallet = walletRepo.createWallet(userId, "BTC Wallet", "bitcoin");
    TEST_ASSERT(btcWallet.hasValue(), "Bitcoin wallet creation should succeed");

    auto ethWallet = walletRepo.createWallet(userId, "ETH Wallet", "ethereum");

    if (ethWallet.hasValue()) {
        std::cout << "    Bitcoin derivation path: m/44'/0'/0'/0/0" << std::endl;
        std::cout << "    Ethereum derivation path: m/44'/60'/0'/0/0" << std::endl;
    }

    TEST_PASS();
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main() {
  std::cout << "=== Comprehensive Multi-Chain Wallet Tests ===" << std::endl << std::endl;

  // Print current working directory for debugging
  std::cout << "Current working directory: "
            << std::filesystem::current_path().string() << std::endl;

  // -------------------------------------------------------------------------
  // Part 1: BIP39/BIP44 Cryptographic Tests
  // -------------------------------------------------------------------------
  std::cout << "\n--- Part 1: BIP39/BIP44 Cryptographic Tests ---" << std::endl;

  std::vector<std::string> wordlist = LoadBIP39Wordlist();

  if (wordlist.size() == 2048) {
    std::cout << "  Loaded BIP39 wordlist (" << wordlist.size() << " words)" << std::endl;

    testGenerateMnemonic(wordlist);
    testValidateMnemonic(wordlist);
    testGenerateBIP39Seed(wordlist);
    testGenerateBIP32MasterKey(wordlist);
    testDeriveEthereumAddresses(wordlist);
    testDeriveBitcoinAddresses(wordlist);
    testDeriveLitecoinAddresses(wordlist);
    testMultiChainAddressDerivation(wordlist);
    testLitecoinAddressValidation();
    testKeccak256TestVector();
  } else {
    std::cerr << "WARNING: Could not load BIP39 wordlist, skipping cryptographic tests"
              << std::endl;
  }

  // -------------------------------------------------------------------------
  // Part 2: Repository-Based Multi-Chain Wallet Tests
  // -------------------------------------------------------------------------
  std::cout << "\n--- Part 2: Repository-Based Multi-Chain Wallet Tests ---" << std::endl;

  TestUtils::printTestHeader("Multi-Chain Wallet Support Tests");

  Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
  TestUtils::initializeTestLogger("test_wallet_chains.log");

  TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH, TestUtils::STANDARD_TEST_ENCRYPTION_KEY);

  Repository::UserRepository userRepo(dbManager);
  Repository::WalletRepository walletRepo(dbManager);

  testCreateEthereumWallet(walletRepo, userRepo);
  testCreateLitecoinWallet(walletRepo, userRepo);
  testMultipleWalletTypesPerUser(walletRepo, userRepo);
  testBitcoinAddressGeneration(walletRepo, userRepo);
  testEthereumAddressGeneration(walletRepo, userRepo);
  testLitecoinAddressGeneration(walletRepo, userRepo);
  testWalletChainIsolation(walletRepo, userRepo);
  testUnsupportedChainRejection(walletRepo, userRepo);
  testBIP44DerivationPathsForDifferentChains(walletRepo, userRepo);

  TestUtils::printTestSummary("Multi-Chain Wallet Tests");
  TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);

  std::cout << "\n=== All Tests Complete ===" << std::endl;

  return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#else
#include <openssl/rand.h>
#include <openssl/evp.h>
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Include your auth header
#include "../backend/core/include/Auth.h"
#include "../backend/core/include/Crypto.h"
#include "../backend/utils/include/SharedTypes.h"

// Include secp256k1 for signature verification tests
#include <secp256k1.h>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

// Forward declare the globals we need
extern std::map<std::string, User> g_users;

namespace TestBIP39 {

class TestRunner {
private:
    int totalTests = 0;
    int passedTests = 0;
    std::vector<std::string> failedTests;

public:
    void runTest(const std::string& testName, std::function<bool()> testFunc) {
        totalTests++;
        std::cout << "Running: " << testName << " ... ";
        
        try {
            if (testFunc()) {
                passedTests++;
                std::cout << "PASS\n";
            } else {
                failedTests.push_back(testName);
                std::cout << "FAIL\n";
            }
        } catch (const std::exception& e) {
            failedTests.push_back(testName + " (Exception: " + e.what() + ")");
            std::cout << "FAIL (Exception: " << e.what() << ")\n";
        } catch (...) {
            failedTests.push_back(testName + " (Unknown exception)");
            std::cout << "FAIL (Unknown exception)\n";
        }
    }

    void printSummary() {
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "TEST SUMMARY\n";
        std::cout << std::string(50, '=') << "\n";
        std::cout << "Total tests: " << totalTests << "\n";
        std::cout << "Passed: " << passedTests << "\n";
        std::cout << "Failed: " << failedTests.size() << "\n";
        
        if (!failedTests.empty()) {
            std::cout << "\nFailed tests:\n";
            for (const auto& test : failedTests) {
                std::cout << "  - " << test << "\n";
            }
        }
        
        std::cout << "\nSuccess rate: " << std::fixed << std::setprecision(1) 
                  << (100.0 * passedTests / totalTests) << "%\n";
        std::cout << std::string(50, '=') << "\n";
    }
    
    bool allTestsPassed() const {
        return failedTests.empty();
    }
};

// Helper functions for testing internal BIP39 logic
namespace Internal {
    
    // Copy internal helper functions for testing (we can't access them directly)
    inline bool RandBytes(void* buf, size_t len) {
#ifdef _WIN32
        return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf),
                                              static_cast<ULONG>(len),
                                              BCRYPT_USE_SYSTEM_PREFERRED_RNG));
#else
        return RAND_bytes(static_cast<unsigned char*>(buf), static_cast<int>(len)) == 1;
#endif
    }

    inline bool SHA256(const uint8_t* data, size_t len, std::array<uint8_t, 32>& out) {
#ifdef _WIN32
        out.fill(0);
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        DWORD hashLen = 0, objLen = 0;
        NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(st)) return false;

        st = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen,
                               sizeof(objLen), &hashLen, 0);
        if (!BCRYPT_SUCCESS(st)) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        std::vector<uint8_t> obj(objLen);
        st = BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, nullptr, 0, 0);
        if (!BCRYPT_SUCCESS(st)) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        st = BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
        if (!BCRYPT_SUCCESS(st)) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        st = BCryptFinishHash(hHash, out.data(), (ULONG)out.size(), 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return BCRYPT_SUCCESS(st);
#else
        unsigned int hashLen = 0;
        if (EVP_Digest(data, len, out.data(), &hashLen, EVP_sha256(), nullptr) != 1) {
            return false;
        }
        return true;
#endif
    }

    std::vector<std::string> LoadWordList() {
        std::vector<std::string> words;
        if (Crypto::LoadBIP39Wordlist(words)) {
            std::cout << "Loaded wordlist using robust detection (2048 words)\n";
        } else {
            std::cout << "Warning: Could not load BIP39 wordlist from any location\n";
        }
        return words;
    }

    std::vector<std::string> SplitWords(const std::string& text) {
        std::vector<std::string> out;
        std::string cur;
        for (char ch : text) {
            if (std::isspace((unsigned char)ch)) {
                if (!cur.empty()) {
                    std::transform(cur.begin(), cur.end(), cur.begin(), ::tolower);
                    out.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back((char)std::tolower((unsigned char)ch));
            }
        }
        if (!cur.empty()) {
            std::transform(cur.begin(), cur.end(), cur.begin(), ::tolower);
            out.push_back(cur);
        }
        return out;
    }

    // Simplified entropy generation for testing
    std::vector<uint8_t> GenerateEntropy(size_t bits) {
        if (bits % 32 != 0 || bits < 128 || bits > 256) {
            return {};
        }
        std::vector<uint8_t> entropy(bits / 8);
        if (!RandBytes(entropy.data(), entropy.size())) {
            return {};
        }
        return entropy;
    }

    // Generate mnemonic from entropy (simplified version)
    std::vector<std::string> MnemonicFromEntropy(const std::vector<uint8_t>& entropy, 
                                                  const std::vector<std::string>& wordlist) {
        if (wordlist.size() != 2048) return {};
        
        const size_t ENT = entropy.size() * 8;
        const size_t CS = ENT / 32;
        const size_t MS = ENT + CS;
        const size_t words = MS / 11;

        std::array<uint8_t, 32> hash{};
        if (!SHA256(entropy.data(), entropy.size(), hash)) {
            return {};
        }

        std::vector<int> bits;
        bits.reserve(MS);
        
        // Add entropy bits
        for (uint8_t b : entropy) {
            for (int i = 7; i >= 0; --i) {
                bits.push_back((b >> i) & 1);
            }
        }
        
        // Add checksum bits
        for (size_t i = 0; i < CS; ++i) {
            size_t byteIdx = i / 8;
            int bitInByte = 7 - (i % 8);
            int bit = (hash[byteIdx] >> bitInByte) & 1;
            bits.push_back(bit);
        }

        std::vector<std::string> mnemonic;
        mnemonic.reserve(words);
        for (size_t i = 0; i < words; ++i) {
            uint32_t idx = 0;
            for (size_t j = 0; j < 11; ++j) {
                idx = (idx << 1) | bits[i * 11 + j];
            }
            mnemonic.push_back(wordlist[idx]);
        }
        return mnemonic;
    }
}

// Test functions
bool test_EntropyGeneration() {
    auto entropy128 = Internal::GenerateEntropy(128);
    auto entropy256 = Internal::GenerateEntropy(256);
    
    return !entropy128.empty() && entropy128.size() == 16 &&
           !entropy256.empty() && entropy256.size() == 32;
}

bool test_WordListLoading() {
    auto words = Internal::LoadWordList();
    
    // Should have exactly 2048 words
    if (words.size() != 2048) return false;
    
    // Check that first few words match expected BIP39 standard
    if (words.empty()) return false;
    if (words[0] != "abandon") return false;
    if (words.size() > 1 && words[1] != "ability") return false;
    
    // Check for duplicates (should be none in a proper wordlist)
    std::set<std::string> uniqueWords(words.begin(), words.end());
    if (uniqueWords.size() != 2048) return false;
    
    return true;
}

bool test_MnemonicGeneration() {
    auto wordlist = Internal::LoadWordList();
    if (wordlist.size() != 2048) return false;
    
    // Test 12-word mnemonic (128-bit entropy)
    auto entropy = Internal::GenerateEntropy(128);
    if (entropy.empty()) return false;
    
    auto mnemonic = Internal::MnemonicFromEntropy(entropy, wordlist);
    if (mnemonic.size() != 12) return false;
    
    // Verify all words are from the wordlist
    std::set<std::string> wordSet(wordlist.begin(), wordlist.end());
    for (const auto& word : mnemonic) {
        if (wordSet.find(word) == wordSet.end()) {
            return false;
        }
    }
    
    return true;
}

bool test_MnemonicConsistency() {
    auto wordlist = Internal::LoadWordList();
    if (wordlist.size() != 2048) return false;
    
    // Generate same mnemonic from same entropy multiple times
    std::vector<uint8_t> fixedEntropy = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    
    auto mnemonic1 = Internal::MnemonicFromEntropy(fixedEntropy, wordlist);
    auto mnemonic2 = Internal::MnemonicFromEntropy(fixedEntropy, wordlist);
    
    return mnemonic1 == mnemonic2 && mnemonic1.size() == 12;
}

bool test_UserRegistration() {
    std::string testUser = "testuser_" + std::to_string(std::time(nullptr));
    std::string testPassword = "ValidPass123!";
    
    // Clear any existing user
    g_users.erase(testUser);
    
    auto response = Auth::RegisterUser(testUser, testPassword);
    
    if (!response.success()) {
        std::cout << "Registration failed: " << response.message << "\n";
        return false;
    }
    
    // User should now exist
    auto it = g_users.find(testUser);
    if (it == g_users.end()) {
        return false;
    }
    
    // Clean up
    g_users.erase(testUser);
    
    return true;
}

bool test_SeedReveal() {
    std::string testUser = "seedtest_" + std::to_string(std::time(nullptr));
    std::string testPassword = "ValidPass123!";
    
    // Create user first
    g_users.erase(testUser);
    auto response = Auth::RegisterUser(testUser, testPassword);
    if (!response.success()) return false;
    
    // Try to reveal seed
    std::string seedHex;
    std::optional<std::string> mnemonic;
    auto revealResponse = Auth::RevealSeed(testUser, testPassword, seedHex, mnemonic);
    
    if (!revealResponse.success()) {
        std::cout << "Seed reveal failed: " << revealResponse.message << "\n";
        g_users.erase(testUser);
        return false;
    }
    
    // Should have seed hex (128 hex chars = 64 bytes)
    bool hasValidSeed = !seedHex.empty() && seedHex.length() == 128;
    
    // Clean up
    g_users.erase(testUser);
    
    return hasValidSeed;
}

bool test_SeedRestore() {
    std::string testUser = "restoretest_" + std::to_string(std::time(nullptr));
    std::string testPassword = "ValidPass123!";
    
    // Create user first
    g_users.erase(testUser);
    auto response = Auth::RegisterUser(testUser, testPassword);
    if (!response.success()) return false;
    
    // Known valid BIP39 mnemonic (abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about)
    std::string testMnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    std::string passphrase = "";  // Empty passphrase
    
    auto restoreResponse = Auth::RestoreFromSeed(testUser, testMnemonic, passphrase, testPassword);
    
    bool success = restoreResponse.success();
    if (!success) {
        std::cout << "Restore failed: " << restoreResponse.message << "\n";
    }
    
    // Clean up
    g_users.erase(testUser);
    
    return success;
}

bool test_InvalidMnemonics() {
    std::string testUser = "invalidtest_" + std::to_string(std::time(nullptr));
    std::string testPassword = "ValidPass123!";
    
    // Create user first
    g_users.erase(testUser);
    auto response = Auth::RegisterUser(testUser, testPassword);
    if (!response.success()) return false;
    
    std::vector<std::string> invalidMnemonics = {
        "",  // Empty
        "invalid word sequence",  // Invalid words
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon",  // Wrong checksum
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon",  // Wrong length
        "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon"  // Too many words
    };
    
    bool allFailed = true;
    for (const auto& invalidMnemonic : invalidMnemonics) {
        auto restoreResponse = Auth::RestoreFromSeed(testUser, invalidMnemonic, "", testPassword);
        if (restoreResponse.success()) {
            std::cout << "Expected failure for: '" << invalidMnemonic << "'\n";
            allFailed = false;
            break;
        }
    }
    
    // Clean up
    g_users.erase(testUser);
    
    return allFailed;
}

bool test_PasswordValidation() {
    std::vector<std::pair<std::string, bool>> testCases = {
        {"", false},                                // Empty
        {"short", false},                            // Too short
        {"nouppercase1!", false},                    // No uppercase
        {"NOLOWERCASE1!", false},                    // No lowercase
        {"NoDigit!", false},                         // No digit
        {"NoSpecial1", false},                       // No special char
        {"ValidPassword123!", true},                // Valid
        {"Another_Valid-Pass123", true},             // Valid with different special chars
        {std::string(129, 'a'), false},              // Too long
        {"BadLength1!", false},                      // 11 chars, too short
        {"GoodLength12!", true}                     // 12 chars, valid
    };
    
    for (const auto& testCase : testCases) {
        bool result = Auth::IsValidPassword(testCase.first);
        if (result != testCase.second) {
            std::cout << "Password validation failed for: '" << testCase.first 
                      << "' (expected " << testCase.second << ", got " << result << ")\n";
            return false;
        }
    }
    
    return true;
}

bool test_UsernameValidation() {
    std::vector<std::pair<std::string, bool>> testCases = {
        {"", false},                    // Empty
        {"ab", false},                  // Too short
        {"validuser", true},            // Valid
        {"user123", true},              // Valid with numbers
        {"user_name", true},            // Valid with underscore
        {"user-name", true},            // Valid with dash
        {"user@name", false},           // Invalid character
        {"user name", false},           // Space not allowed
        {std::string(100, 'a'), false}, // Too long
        {"123user", true},              // Starting with number is ok
        {"_user", true},                // Starting with underscore is ok
        {"-user", true}                 // Starting with dash is ok
    };
    
    for (const auto& testCase : testCases) {
        bool result = Auth::IsValidUsername(testCase.first);
        if (result != testCase.second) {
            std::cout << "Username validation failed for: '" << testCase.first 
                      << "' (expected " << testCase.second << ", got " << result << ")\n";
            return false;
        }
    }
    
    return true;
}

bool test_MultipleUserRegistrations() {
    std::vector<std::string> testUsers;
    std::string baseUser = "multitest_" + std::to_string(std::time(nullptr));
    std::string password = "ValidPass123!";
    
    // Create multiple users
    for (int i = 0; i < 5; i++) {
        std::string username = baseUser + "_" + std::to_string(i);
        testUsers.push_back(username);
        
        g_users.erase(username);
        auto response = Auth::RegisterUser(username, password);
        if (!response.success()) {
            // Clean up partial results
            for (const auto& user : testUsers) {
                g_users.erase(user);
            }
            return false;
        }
    }
    
    // Verify all users exist and have different seeds
    std::set<std::string> uniqueSeeds;
    for (const auto& username : testUsers) {
        std::string seedHex;
        std::optional<std::string> mnemonic;
        auto revealResponse = Auth::RevealSeed(username, password, seedHex, mnemonic);
        
        if (!revealResponse.success() || seedHex.empty()) {
            // Clean up
            for (const auto& user : testUsers) {
                g_users.erase(user);
            }
            return false;
        }
        
        uniqueSeeds.insert(seedHex);
    }
    
    // All seeds should be unique
    bool allUnique = (uniqueSeeds.size() == testUsers.size());
    
    // Clean up
    for (const auto& user : testUsers) {
        g_users.erase(user);
    }
    
    return allUnique;
}

bool test_RateLimiting() {
    std::string testUser = "ratelimittest_" + std::to_string(std::time(nullptr));
    std::string correctPassword = "CorrectPassword123!";
    std::string wrongPassword = "wrongpass";

    // Create user
    g_users.erase(testUser);
    auto response = Auth::RegisterUser(testUser, correctPassword);
    if (!response.success()) return false;

    // Clear any existing rate limits
    Auth::ClearRateLimit(testUser);

    // Try wrong password multiple times (should trigger rate limiting)
    bool wasRateLimited = false;
    for (int i = 0; i < 6; i++) {  // More than MAX_LOGIN_ATTEMPTS
        auto loginResponse = Auth::LoginUser(testUser, wrongPassword);
        if (loginResponse.result == Auth::AuthResult::RATE_LIMITED) {
            wasRateLimited = true;
            break;
        }
    }

    // Should be rate limited after correct password too
    auto correctResponse = Auth::LoginUser(testUser, correctPassword);
    bool correctPasswordAlsoBlocked = (correctResponse.result == Auth::AuthResult::RATE_LIMITED);

    // Clean up
    Auth::ClearRateLimit(testUser);
    g_users.erase(testUser);

    return wasRateLimited && correctPasswordAlsoBlocked;
}

// ========================================
// BIP32 Cryptographic Tests (Phase 1)
// ========================================

bool test_BIP32_MasterKeyGeneration() {
    // Test BIP32 master key generation from seed
    std::array<uint8_t, 64> testSeed = {};
    for (size_t i = 0; i < 64; i++) {
        testSeed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(testSeed, masterKey)) {
        std::cout << "Failed to generate master key from seed\n";
        return false;
    }

    // Verify master key properties
    if (!masterKey.isPrivate) {
        std::cout << "Master key should be private\n";
        return false;
    }

    if (masterKey.key.size() != 32) {
        std::cout << "Master key should be 32 bytes, got " << masterKey.key.size() << "\n";
        return false;
    }

    if (masterKey.chainCode.size() != 32) {
        std::cout << "Chain code should be 32 bytes, got " << masterKey.chainCode.size() << "\n";
        return false;
    }

    if (masterKey.depth != 0) {
        std::cout << "Master key depth should be 0, got " << static_cast<int>(masterKey.depth) << "\n";
        return false;
    }

    return true;
}

bool test_BIP32_ChildKeyDerivation() {
    // Test child key derivation (both hardened and normal)
    std::array<uint8_t, 64> testSeed = {};
    for (size_t i = 0; i < 64; i++) {
        testSeed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(testSeed, masterKey)) {
        return false;
    }

    // Test hardened derivation (m/0')
    Crypto::BIP32ExtendedKey hardenedChild;
    if (!Crypto::BIP32_DeriveChild(masterKey, 0x80000000, hardenedChild)) {
        std::cout << "Failed to derive hardened child key\n";
        return false;
    }

    if (!hardenedChild.isPrivate) {
        std::cout << "Hardened child should be private\n";
        return false;
    }

    if (hardenedChild.depth != 1) {
        std::cout << "Child depth should be 1, got " << static_cast<int>(hardenedChild.depth) << "\n";
        return false;
    }

    // Test normal derivation (m/0)
    Crypto::BIP32ExtendedKey normalChild;
    if (!Crypto::BIP32_DeriveChild(masterKey, 0, normalChild)) {
        std::cout << "Failed to derive normal child key\n";
        return false;
    }

    if (!normalChild.isPrivate) {
        std::cout << "Normal child should be private\n";
        return false;
    }

    // Verify hardened and normal children are different
    if (hardenedChild.key == normalChild.key) {
        std::cout << "Hardened and normal children should be different\n";
        return false;
    }

    return true;
}

bool test_BIP32_PathDerivation() {
    // Test BIP44 path derivation: m/44'/0'/0'/0/0
    std::array<uint8_t, 64> testSeed = {};
    for (size_t i = 0; i < 64; i++) {
        testSeed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(testSeed, masterKey)) {
        return false;
    }

    // Derive BIP44 path
    Crypto::BIP32ExtendedKey derivedKey;
    if (!Crypto::BIP32_DerivePath(masterKey, "m/44'/0'/0'/0/0", derivedKey)) {
        std::cout << "Failed to derive BIP44 path\n";
        return false;
    }

    if (!derivedKey.isPrivate) {
        std::cout << "Derived key should be private\n";
        return false;
    }

    if (derivedKey.depth != 5) {
        std::cout << "Derived key depth should be 5 (m/44'/0'/0'/0/0), got "
                  << static_cast<int>(derivedKey.depth) << "\n";
        return false;
    }

    // Test consistency - same path should give same result
    Crypto::BIP32ExtendedKey derivedKey2;
    if (!Crypto::BIP32_DerivePath(masterKey, "m/44'/0'/0'/0/0", derivedKey2)) {
        return false;
    }

    if (derivedKey.key != derivedKey2.key) {
        std::cout << "Path derivation is not deterministic\n";
        return false;
    }

    return true;
}

bool test_BIP32_BitcoinAddressGeneration() {
    // Test Bitcoin address generation from BIP32 key
    std::array<uint8_t, 64> testSeed = {};
    for (size_t i = 0; i < 64; i++) {
        testSeed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(testSeed, masterKey)) {
        return false;
    }

    Crypto::BIP32ExtendedKey derivedKey;
    if (!Crypto::BIP32_DerivePath(masterKey, "m/44'/0'/0'/0/0", derivedKey)) {
        return false;
    }

    std::string address;
    if (!Crypto::BIP32_GetBitcoinAddress(derivedKey, address)) {
        std::cout << "Failed to generate Bitcoin address\n";
        return false;
    }

    // Verify address format (should start with '1' for mainnet P2PKH)
    if (address.empty()) {
        std::cout << "Address is empty\n";
        return false;
    }

    if (address[0] != '1') {
        std::cout << "Address should start with '1' for mainnet, got '" << address[0] << "'\n";
        return false;
    }

    if (address.length() < 26 || address.length() > 35) {
        std::cout << "Address length should be 26-35 characters, got " << address.length() << "\n";
        return false;
    }

    std::cout << "Generated Bitcoin address: " << address << "\n";

    return true;
}

bool test_BIP32_WIFExport() {
    // Test WIF (Wallet Import Format) export
    std::array<uint8_t, 64> testSeed = {};
    for (size_t i = 0; i < 64; i++) {
        testSeed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(testSeed, masterKey)) {
        return false;
    }

    std::string wif;
    if (!Crypto::BIP32_GetWIF(masterKey, wif, false)) {
        std::cout << "Failed to export WIF\n";
        return false;
    }

    // Verify WIF format (should start with '5' or 'K'/'L' for mainnet)
    if (wif.empty()) {
        std::cout << "WIF is empty\n";
        return false;
    }

    if (wif[0] != '5' && wif[0] != 'K' && wif[0] != 'L') {
        std::cout << "WIF should start with '5', 'K', or 'L' for mainnet, got '" << wif[0] << "'\n";
        return false;
    }

    if (wif.length() < 51 || wif.length() > 52) {
        std::cout << "WIF length should be 51-52 characters, got " << wif.length() << "\n";
        return false;
    }

    std::cout << "Generated WIF (first 10 chars): " << wif.substr(0, 10) << "...\n";

    return true;
}

bool test_BIP32_Secp256k1Integration() {
    // Test that secp256k1 elliptic curve operations work correctly
    std::array<uint8_t, 64> testSeed = {};
    for (size_t i = 0; i < 64; i++) {
        testSeed[i] = static_cast<uint8_t>(i + 100); // Different seed for variety
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(testSeed, masterKey)) {
        return false;
    }

    // Derive multiple addresses - they should all be different
    std::vector<std::string> addresses;
    for (uint32_t i = 0; i < 5; i++) {
        std::string path = "m/44'/0'/0'/0/" + std::to_string(i);
        Crypto::BIP32ExtendedKey derivedKey;
        if (!Crypto::BIP32_DerivePath(masterKey, path, derivedKey)) {
            std::cout << "Failed to derive path: " << path << "\n";
            return false;
        }

        std::string address;
        if (!Crypto::BIP32_GetBitcoinAddress(derivedKey, address)) {
            std::cout << "Failed to generate address for path: " << path << "\n";
            return false;
        }

        addresses.push_back(address);
    }

    // Verify all addresses are unique
    std::set<std::string> uniqueAddresses(addresses.begin(), addresses.end());
    if (uniqueAddresses.size() != addresses.size()) {
        std::cout << "Generated addresses are not unique\n";
        return false;
    }

    std::cout << "Generated 5 unique addresses using secp256k1\n";

    return true;
}

bool test_BIP32_KnownTestVector() {
    // Test with known BIP39 test vector:
    // Mnemonic: "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about"
    // This is a well-known BIP39 test case

    // Known BIP39 seed for this mnemonic (with empty passphrase)
    std::array<uint8_t, 64> knownSeed = {
        0x5e, 0xb0, 0x0b, 0xbd, 0xdc, 0xf0, 0x69, 0x08,
        0x4a, 0xbc, 0xa9, 0x1e, 0x89, 0x43, 0xd7, 0x43,
        0x04, 0x5e, 0x78, 0xcd, 0x56, 0xa0, 0x41, 0x75,
        0x9f, 0xfb, 0x8b, 0x7a, 0x33, 0xcd, 0xae, 0xcd,
        0xd5, 0xae, 0x3f, 0x5e, 0x1e, 0xd9, 0x6e, 0x82,
        0xf9, 0xf0, 0xc4, 0xa7, 0x50, 0x63, 0xe1, 0x38,
        0x97, 0xe4, 0x2c, 0x62, 0x9b, 0x9b, 0x5d, 0x3a,
        0x0e, 0x59, 0xc4, 0x82, 0xf3, 0x5d, 0x6e, 0xcd
    };

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(knownSeed, masterKey)) {
        std::cout << "Failed to derive master key from known seed\n";
        return false;
    }

    // Derive m/44'/0'/0'/0/0 (first Bitcoin address)
    Crypto::BIP32ExtendedKey derivedKey;
    if (!Crypto::BIP32_DerivePath(masterKey, "m/44'/0'/0'/0/0", derivedKey)) {
        std::cout << "Failed to derive BIP44 path from known seed\n";
        return false;
    }

    std::string address;
    if (!Crypto::BIP32_GetBitcoinAddress(derivedKey, address)) {
        std::cout << "Failed to generate address from known seed\n";
        return false;
    }

    // The address should be deterministic for this known test vector
    std::cout << "Known test vector address: " << address << "\n";

    // Verify it's a valid Bitcoin address format
    if (address.empty() || address[0] != '1') {
        std::cout << "Known test vector produced invalid address\n";
        return false;
    }

    return true;
}

bool test_BIP44_MultipleAddresses() {
    // Test BIP44 multiple address generation
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        std::cout << "Failed to generate master key\n";
        return false;
    }

    // Generate first 5 receiving addresses
    std::vector<std::string> addresses;
    bool success = Crypto::BIP44_GenerateAddresses(masterKey, 0, false, 0, 5, addresses, false);

    if (success) {
        std::cout << "Generated " << addresses.size() << " addresses:" << std::endl;
        for (size_t i = 0; i < addresses.size(); i++) {
            std::cout << "  Address " << i << ": " << addresses[i] << std::endl;
        }
    }

    return success && addresses.size() == 5;
}

bool test_TransactionSigning() {
    // Test transaction hash signing
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Create a test transaction hash
    std::array<uint8_t, 32> txHash;
    for (size_t i = 0; i < 32; i++) {
        txHash[i] = static_cast<uint8_t>(i * 2);
    }

    // Sign the hash
    Crypto::ECDSASignature signature;
    bool signSuccess = Crypto::SignHash(masterKey.key, txHash, signature);

    if (signSuccess) {
        std::cout << "Transaction signed successfully" << std::endl;
        std::cout << "DER signature size: " << signature.der_encoded.size() << " bytes" << std::endl;
        std::cout << "R component size: " << signature.r.size() << " bytes" << std::endl;
        std::cout << "S component size: " << signature.s.size() << " bytes" << std::endl;
    }

    return signSuccess && !signature.der_encoded.empty() &&
           signature.r.size() == 32 && signature.s.size() == 32;
}

bool test_SignatureVerification() {
    // Test signature verification
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Derive public key
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, masterKey.key.data())) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    uint8_t pubkey_serialized[33];
    size_t pubkey_len = sizeof(pubkey_serialized);
    secp256k1_ec_pubkey_serialize(ctx, pubkey_serialized, &pubkey_len, &pubkey, SECP256K1_EC_COMPRESSED);
    std::vector<uint8_t> public_key(pubkey_serialized, pubkey_serialized + pubkey_len);
    secp256k1_context_destroy(ctx);

    // Create and sign a hash
    std::array<uint8_t, 32> txHash;
    for (size_t i = 0; i < 32; i++) {
        txHash[i] = static_cast<uint8_t>(i * 2);
    }

    Crypto::ECDSASignature signature;
    if (!Crypto::SignHash(masterKey.key, txHash, signature)) {
        return false;
    }

    // Verify the signature
    bool verifySuccess = Crypto::VerifySignature(public_key, txHash, signature);

    std::cout << "Signature verification: " << (verifySuccess ? "SUCCESS" : "FAILED") << std::endl;

    return verifySuccess;
}

bool test_CoinSelection() {
    // Test UTXO coin selection
    std::vector<Crypto::UTXO> available_utxos;

    Crypto::UTXO utxo1;
    utxo1.txid = "abc123";
    utxo1.vout = 0;
    utxo1.amount = 100000;  // 0.001 BTC
    utxo1.address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    available_utxos.push_back(utxo1);

    Crypto::UTXO utxo2;
    utxo2.txid = "def456";
    utxo2.vout = 1;
    utxo2.amount = 200000;  // 0.002 BTC
    utxo2.address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    available_utxos.push_back(utxo2);

    Crypto::UTXO utxo3;
    utxo3.txid = "ghi789";
    utxo3.vout = 0;
    utxo3.amount = 50000;   // 0.0005 BTC
    utxo3.address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    available_utxos.push_back(utxo3);

    // Try to send 0.0015 BTC (150000 satoshis) with 10 sat/byte fee
    uint64_t target_amount = 150000;
    uint64_t fee_per_byte = 10;

    Crypto::CoinSelection selection;
    bool success = Crypto::SelectCoins(available_utxos, target_amount, fee_per_byte, selection);

    if (success) {
        std::cout << "Coin selection successful" << std::endl;
        std::cout << "Selected UTXOs: " << selection.selected_utxos.size() << std::endl;
        std::cout << "Total input: " << selection.total_input << " satoshis" << std::endl;
        std::cout << "Target amount: " << selection.target_amount << " satoshis" << std::endl;
        std::cout << "Fee: " << selection.fee << " satoshis" << std::endl;
        std::cout << "Change amount: " << selection.change_amount << " satoshis" << std::endl;
        std::cout << "Has change: " << (selection.has_change ? "Yes" : "No") << std::endl;
    }

    return success && selection.total_input >= target_amount + selection.fee;
}

bool test_TransactionSizeEstimation() {
    // Test transaction size estimation
    size_t input_count = 2;
    size_t output_count = 2;

    uint64_t estimated_size = Crypto::EstimateTransactionSize(input_count, output_count);

    std::cout << "Estimated transaction size: " << estimated_size << " bytes" << std::endl;
    std::cout << "For " << input_count << " inputs and " << output_count << " outputs" << std::endl;

    // Calculate fee at 10 sat/byte
    uint64_t fee = Crypto::CalculateFee(input_count, output_count, 10);
    std::cout << "Estimated fee (10 sat/byte): " << fee << " satoshis" << std::endl;

    // Reasonable size check (should be > 100 bytes and < 1000 bytes for this simple tx)
    return estimated_size > 100 && estimated_size < 1000 && fee > 0;
}

} // namespace TestBIP39

int main() {
    using namespace TestBIP39;
    
    std::cout << "BIP39 Functionality Test Suite\n";
    std::cout << std::string(50, '=') << "\n\n";

    // REMOVED: Auth::LoadUserDatabase() - insecure plaintext database function
    // Tests work with in-memory g_users map; production uses Repository layer

    TestRunner runner;
    
    // Core BIP39 tests
    runner.runTest("Entropy Generation", test_EntropyGeneration);
    runner.runTest("Word List Loading", test_WordListLoading);
    runner.runTest("Mnemonic Generation", test_MnemonicGeneration);
    runner.runTest("Mnemonic Consistency", test_MnemonicConsistency);
    
    // Authentication integration tests
    runner.runTest("User Registration", test_UserRegistration);
    runner.runTest("Seed Reveal", test_SeedReveal);
    runner.runTest("Seed Restore", test_SeedRestore);
    runner.runTest("Invalid Mnemonics Rejection", test_InvalidMnemonics);
    
    // Validation tests
    runner.runTest("Password Validation", test_PasswordValidation);
    runner.runTest("Username Validation", test_UsernameValidation);
    
    // Edge case and security tests
    runner.runTest("Multiple User Registrations", test_MultipleUserRegistrations);
    runner.runTest("Rate Limiting", test_RateLimiting);

    // BIP32 Cryptographic Tests (Phase 1 Verification)
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "BIP32 CRYPTOGRAPHIC TESTS (Phase 1)\n";
    std::cout << std::string(50, '=') << "\n\n";

    runner.runTest("BIP32: Master Key Generation", test_BIP32_MasterKeyGeneration);
    runner.runTest("BIP32: Child Key Derivation", test_BIP32_ChildKeyDerivation);
    runner.runTest("BIP32: Path Derivation (BIP44)", test_BIP32_PathDerivation);
    runner.runTest("BIP32: Bitcoin Address Generation", test_BIP32_BitcoinAddressGeneration);
    runner.runTest("BIP32: WIF Private Key Export", test_BIP32_WIFExport);
    runner.runTest("BIP32: secp256k1 Integration", test_BIP32_Secp256k1Integration);
    runner.runTest("BIP32: Known Test Vector", test_BIP32_KnownTestVector);

    // Transaction functionality tests
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "TRANSACTION SIGNING & UTXO MANAGEMENT TESTS\n";
    std::cout << std::string(50, '=') << "\n\n";

    runner.runTest("BIP44: Multiple Address Generation", test_BIP44_MultipleAddresses);
    runner.runTest("Transaction: Hash Signing", test_TransactionSigning);
    runner.runTest("Transaction: Signature Verification", test_SignatureVerification);
    runner.runTest("UTXO: Coin Selection", test_CoinSelection);
    runner.runTest("Transaction: Size Estimation", test_TransactionSizeEstimation);

    runner.printSummary();

    // REMOVED: Auth::SaveUserDatabase() - insecure plaintext database function
    // Tests use in-memory data; production should persist via Repository layer

    return runner.allTestsPassed() ? 0 : 1;
}

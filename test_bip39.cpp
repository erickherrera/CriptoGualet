#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

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
#include "include/Auth.h"
#include "include/CriptoGualet.h"

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
        return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf),
                                              static_cast<ULONG>(len),
                                              BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    }

    inline bool SHA256(const uint8_t* data, size_t len, std::array<uint8_t, 32>& out) {
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
    }

    std::vector<std::string> LoadWordList() {
        std::vector<std::string> words;
        
        // Try multiple possible locations for the wordlist
        std::vector<std::string> possiblePaths = {
            "src/assets/bip39/english.txt",
            "assets/bip39/english.txt",
            "../src/assets/bip39/english.txt", 
            "../assets/bip39/english.txt"
        };
        
        for (const auto& path : possiblePaths) {
            std::ifstream f(path, std::ios::binary);
            if (f.is_open()) {
                std::string line;
                while (std::getline(f, line)) {
                    // Trim whitespace
                    size_t start = line.find_first_not_of(" \t\r\n");
                    if (start != std::string::npos) {
                        size_t end = line.find_last_not_of(" \t\r\n");
                        words.push_back(line.substr(start, end - start + 1));
                    }
                }
                f.close();
                if (words.size() == 2048) {
                    std::cout << "Loaded wordlist from: " << path << "\n";
                    return words;
                }
                words.clear();
            }
        }
        
        std::cout << "Warning: Could not load BIP39 wordlist from any location\n";
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
    std::string testPassword = "testpass123";
    
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
    std::string testPassword = "testpass123";
    
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
    std::string testPassword = "testpass123";
    
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
    std::string testPassword = "testpass123";
    
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
        {"", false},                    // Empty
        {"123", false},                 // Too short
        {"abc", false},                 // Too short, no digits
        {"123456", false},              // No letters
        {"abcdef", false},              // No digits
        {"abc123", true},               // Valid: has letters and digits
        {"Password123", true},          // Valid
        {"Test1", false},               // Too short
        {"VeryLongPasswordWith123Numbers", true}  // Valid long password
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
    std::string password = "testpass123";
    
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
    std::string correctPassword = "testpass123";
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

} // namespace TestBIP39

int main() {
    using namespace TestBIP39;
    
    std::cout << "BIP39 Functionality Test Suite\n";
    std::cout << std::string(50, '=') << "\n\n";
    
    // Load user database
    Auth::LoadUserDatabase();
    
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
    
    runner.printSummary();
    
    // Save any changes to database
    Auth::SaveUserDatabase();
    
    std::cout << "\nTest run completed. Press any key to continue...\n";
    std::cin.get();
    
    return runner.allTestsPassed() ? 0 : 1;
}
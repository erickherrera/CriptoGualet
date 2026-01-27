#pragma once

#include <chrono>
#include <string>
#include <iostream>

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

// Forward declarations for dependencies
namespace Database { class DatabaseManager; }
namespace Repository { class UserRepository; class WalletRepository; }

namespace TestGlobals {
    extern int g_testsRun;
    extern int g_testsPassed;
    extern int g_testsFailed;
}

#ifndef TEST_START
#define TEST_START(name) \
    do { \
        std::cout << COLOR_BLUE << "[TEST] " << name << COLOR_RESET << std::endl; \
        TestGlobals::g_testsRun++; \
    } while(0)
#endif

#ifndef TEST_ASSERT
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cout << COLOR_RED << "  ✗ FAILED: " << message << COLOR_RESET << std::endl; \
            TestGlobals::g_testsFailed++; \
            return; \
        } \
    } while(0)
#endif

#ifndef TEST_PASS
#define TEST_PASS() \
    do { \
        std::cout << COLOR_GREEN << "  ✓ PASSED" << COLOR_RESET << std::endl; \
        TestGlobals::g_testsPassed++; \
    } while(0)
#endif

namespace TestUtils {
    // Constants
    const std::string STANDARD_TEST_ENCRYPTION_KEY =
        "CriptoGualet_TestKey_2026_Secure_Encryption_32bytes";

    // Setup and teardown
    void printTestHeader(const std::string& title);
    void initializeTestLogger(const std::string& logFileName);
    void initializeTestDatabase(Database::DatabaseManager& dbManager, const std::string& dbPath, const std::string& encryptionKey);
    bool createFullSchema(Database::DatabaseManager& dbManager);
    void shutdownTestEnvironment(Database::DatabaseManager& dbManager, const std::string& dbPath);
    void printTestSummary(const std::string& testSuiteName);

    // Helper functions
    int createTestUser(Repository::UserRepository& userRepo, const std::string& username);
    int createTestUserWithWallet(Repository::UserRepository& userRepo, Repository::WalletRepository& walletRepo, const std::string& username);
}

class MockTime {
private:
    static std::chrono::steady_clock::time_point mockTime_;
    static bool useMockTime_;
    
public:
    static void enable();
    static void disable();
    static std::chrono::steady_clock::time_point now();
    static void advance(std::chrono::minutes minutes);
    static void reset();
};
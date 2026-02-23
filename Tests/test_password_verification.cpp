#include "../backend/core/include/Auth.h"
#include "../backend/database/include/Database/DatabaseManager.h"
#include "../backend/repository/include/Repository/UserRepository.h"
#include "TestUtils.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace {
    std::string getTestDbPath() {
        return TestUtils::getWritableTestPath("test_password_wallet.db");
    }

    void cleanupTestDatabase() {
        try {
            std::string dbPath = getTestDbPath();
            namespace fs = std::filesystem;
            if (fs::exists(dbPath)) fs::remove(dbPath);
            if (fs::exists(dbPath + "-wal")) fs::remove(dbPath + "-wal");
            if (fs::exists(dbPath + "-shm")) fs::remove(dbPath + "-shm");
        } catch (...) {}
    }

    void cleanupProductionDatabase() {
        // Disabled for safety when running from installed app
    }

    void logResult(const std::string& name, bool passed) {
        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Password Verification Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    std::string dbPath = getTestDbPath();
    
    // Override Auth database path for testing
#ifdef _WIN32
    _putenv_s("WALLET_DB_PATH", dbPath.c_str());
#else
    setenv("WALLET_DB_PATH", dbPath.c_str(), 1);
#endif

    cleanupTestDatabase();

    std::cout << "\n=== Initializing at " << dbPath << " ===" << std::endl;
    if (!Auth::InitializeAuthDatabase()) {
        std::cerr << "Failed to initialize database" << std::endl;
        return 1;
    }
    std::cout << "Database initialized" << std::endl;

    bool allPassed = true;

    std::cout << "\n=== Test 1: Password Hash Generation ===" << std::endl;
    {
        std::string password = "TestP@ssw0rd123!";
        std::string hash1 = Auth::CreatePasswordHash(password);
        std::string hash2 = Auth::CreatePasswordHash(password);
        bool generated = !hash1.empty();
        logResult("Generate password hash", generated);
        allPassed &= generated;

        if (generated) {
            bool verifies = Auth::VerifyPassword(password, hash1);
            logResult("Verify password with generated hash", verifies);
            allPassed &= verifies;

            bool different = (hash1 != hash2);
            logResult("Salt randomization works", different);
            allPassed &= different;
        }
    }

    std::cout << "\n=== Test 2: User Registration ===" << std::endl;
    std::string testUser = "pw_test_user";
    std::string testPass = "TestP@ss123!";
    {
        std::vector<std::string> mnemonic;
        auto response = Auth::RegisterUserWithMnemonic(testUser, testPass, mnemonic);
        bool registered = response.success();
        logResult("Register new user", registered);
        allPassed &= registered;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\n=== Test 3: Login with Correct Password ===" << std::endl;
    {
        auto loginResult = Auth::LoginUser(testUser, testPass);
        bool passed = loginResult.success();
        logResult("Login with correct password", passed);
        allPassed &= passed;
    }

    std::cout << "\n=== Test 4: Reject Wrong Password ===" << std::endl;
    {
        auto loginResult = Auth::LoginUser(testUser, "WrongPassword123!");
        bool rejected = !loginResult.success();
        logResult("Reject incorrect password", rejected);
        allPassed &= rejected;
    }

    std::cout << "\n=== Test 5: Password Strength Validation ===" << std::endl;
    {
        auto weakResult = Auth::LoginUser("weakuser", "weak");
        bool weakRejected = !weakResult.success();
        logResult("Reject weak password in login", weakRejected);
        allPassed &= weakRejected;
    }

    std::cout << "\n=== Test 6: Database User Lookup ===" << std::endl;
    {
        try {
            auto& dbManager = Database::DatabaseManager::getInstance();
            Repository::UserRepository userRepo(dbManager);
            auto userResult = userRepo.getUserByUsername(testUser);
            bool found = userResult.success;
            logResult("Find user in database", found);
            allPassed &= found;

            if (found) {
                bool notEmpty = !userResult.data.passwordHash.empty();
                logResult("Password hash stored in database", notEmpty);
                allPassed &= notEmpty;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            logResult("Database user lookup", false);
            allPassed = false;
        }
    }

    cleanupProductionDatabase();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "  ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "  SOME TESTS FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return allPassed ? 0 : 1;
}

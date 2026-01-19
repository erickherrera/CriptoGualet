#include "TestUtils.h"
#include <iostream>
#include <filesystem>
#include "Database/DatabaseManager.h"
#include "Repository/UserRepository.h"
#include "Repository/WalletRepository.h"
#include "Repository/Logger.h"


// This file can be used for shared test utilities.

// Global test counters
namespace TestGlobals {
    int g_testsRun = 0;
    int g_testsPassed = 0;
    int g_testsFailed = 0;
}

namespace TestUtils {

    void printTestHeader(const std::string& title) {
        std::cout << "\n" << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << " " << title << COLOR_RESET << std::endl;
        std::cout << COLOR_CYAN << "==================================================" << COLOR_RESET << std::endl;
    }

    void initializeTestLogger(const std::string& logFileName) {
        Repository::Logger::getInstance().initialize(logFileName);
    }

    void initializeTestDatabase(Database::DatabaseManager& dbManager, const std::string& dbPath, const std::string& encryptionKey) {
        if (std::filesystem::exists(dbPath)) {
            std::filesystem::remove(dbPath);
        }
        dbManager.initialize(dbPath, encryptionKey);
    }

    void shutdownTestEnvironment(Database::DatabaseManager& dbManager, const std::string& dbPath) {
        dbManager.close();
        if (std::filesystem::exists(dbPath)) {
            // In case of test failures, you might want to inspect the database.
            // For clean runs, uncomment the line below.
            // std::filesystem::remove(dbPath);
        }
    }

    void printTestSummary(const std::string& testSuiteName) {
        std::cout << "\n" << COLOR_BLUE << "---------- Test Summary for " << testSuiteName << " ----------" << COLOR_RESET << std::endl;
        std::cout << COLOR_GREEN << "  PASSED: " << TestGlobals::g_testsPassed << COLOR_RESET << std::endl;
        std::cout << COLOR_RED << "  FAILED: " << TestGlobals::g_testsFailed << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "  TOTAL:  " << TestGlobals::g_testsRun << COLOR_RESET << std::endl;
        std::cout << COLOR_BLUE << "----------------------------------------------------" << COLOR_RESET << std::endl;
    }

    int createTestUserWithWallet(Repository::UserRepository& userRepo, Repository::WalletRepository& walletRepo, const std::string& username) {
        auto userResult = userRepo.createUser(username, "SecurePass123!");
        if (!userResult.hasValue()) {
            return -1;
        }
        auto walletResult = walletRepo.createWallet(userResult->id, "My Test Wallet", "bitcoin");
        if (!walletResult.hasValue()) {
            return -1;
        }
        return walletResult->id;
    }
}


// Mock time management
std::chrono::steady_clock::time_point MockTime::mockTime_;
bool MockTime::useMockTime_ = false;

void MockTime::enable() {
    useMockTime_ = true;
    mockTime_ = std::chrono::steady_clock::now();
}

void MockTime::disable() {
    useMockTime_ = false;
}

std::chrono::steady_clock::time_point MockTime::now() {
    return useMockTime_ ? mockTime_ : std::chrono::steady_clock::now();
}

void MockTime::advance(std::chrono::minutes minutes) {
    mockTime_ += minutes;
}

void MockTime::reset() {
    mockTime_ = std::chrono::steady_clock::now();
    useMockTime_ = false;
}
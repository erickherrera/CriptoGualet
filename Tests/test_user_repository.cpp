/**
 * @file test_user_repository.cpp
 * @brief Unit tests for UserRepository
 *
 * Tests user creation, authentication, password management, and validation.
 */

#include "TestUtils.h"

constexpr const char* TEST_DB_PATH = "test_user_repo.db";

// ============================================================================
// Test Cases
// ============================================================================

static bool testCreateUser(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Valid Input");

    auto result = userRepo.createUser("alice", "alice@example.com", "SecurePass123!");
    TEST_ASSERT(result.hasValue(), "User creation should succeed");
    TEST_ASSERT(result->username == "alice", "Username should match");
    TEST_ASSERT(result->email == "alice@example.com", "Email should match");
    TEST_ASSERT(result->id > 0, "User ID should be positive");
    TEST_ASSERT(result->isActive, "User should be active by default");

    TEST_PASS();
}

static bool testCreateUserDuplicateUsername(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Duplicate Username");

    auto result1 = userRepo.createUser("bob", "bob@example.com", "SecurePass123!");
    TEST_ASSERT(result1.hasValue(), "First user creation should succeed");

    auto result2 = userRepo.createUser("bob", "bob2@example.com", "SecurePass123!");
    TEST_ASSERT(!result2.hasValue(), "Duplicate username should fail");
    TEST_ASSERT(result2.errorCode == 409, "Error code should be 409 (Conflict)");

    TEST_PASS();
}

static bool testCreateUserInvalidUsername(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Invalid Username");

    auto result1 = userRepo.createUser("ab", "test@example.com", "SecurePass123!");
    TEST_ASSERT(!result1.hasValue(), "Username too short should fail");
    TEST_ASSERT(result1.errorCode == 400, "Error code should be 400");

    std::string longUsername(101, 'a');
    auto result2 = userRepo.createUser(longUsername, "test@example.com", "SecurePass123!");
    TEST_ASSERT(!result2.hasValue(), "Username too long should fail");

    auto result3 = userRepo.createUser("user@name", "test@example.com", "SecurePass123!");
    TEST_ASSERT(!result3.hasValue(), "Username with invalid characters should fail");

    TEST_PASS();
}

static bool testCreateUserInvalidPassword(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Invalid Password");

    auto result1 = userRepo.createUser("charlie", "charlie@example.com", "Pass1!");
    TEST_ASSERT(!result1.hasValue(), "Password too short should fail");
    TEST_ASSERT(result1.errorCode == 400, "Error code should be 400");

    auto result2 = userRepo.createUser("charlie", "charlie@example.com", "securepass123!");
    TEST_ASSERT(!result2.hasValue(), "Password without uppercase should fail");

    auto result3 = userRepo.createUser("charlie", "charlie@example.com", "SECUREPASS123!");
    TEST_ASSERT(!result3.hasValue(), "Password without lowercase should fail");

    auto result4 = userRepo.createUser("charlie", "charlie@example.com", "SecurePassword!");
    TEST_ASSERT(!result4.hasValue(), "Password without digit should fail");

    auto result5 = userRepo.createUser("charlie", "charlie@example.com", "SecurePass123");
    TEST_ASSERT(!result5.hasValue(), "Password without special character should fail");

    TEST_PASS();
}

static bool testAuthenticateUserSuccess(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Success");

    const std::string password = "SecurePass123!";
    auto createResult = userRepo.createUser("dave", "dave@example.com", password);
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto authResult = userRepo.authenticateUser("dave", password);
    TEST_ASSERT(authResult.hasValue(), "Authentication should succeed");
    TEST_ASSERT(authResult->username == "dave", "Username should match");
    TEST_ASSERT(authResult->id == createResult->id, "User ID should match");

    TEST_PASS();
}

static bool testAuthenticateUserWrongPassword(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Wrong Password");

    auto createResult = userRepo.createUser("eve", "eve@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto authResult = userRepo.authenticateUser("eve", "WrongPassword123!");
    TEST_ASSERT(!authResult.hasValue(), "Authentication should fail");
    TEST_ASSERT(authResult.errorCode == 401, "Error code should be 401 (Unauthorized)");

    TEST_PASS();
}

static bool testAuthenticateUserNotFound(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - User Not Found");

    auto authResult = userRepo.authenticateUser("nonexistent", "SecurePass123!");
    TEST_ASSERT(!authResult.hasValue(), "Authentication should fail");
    TEST_ASSERT(authResult.errorCode == 401, "Error code should be 401");

    TEST_PASS();
}

static bool testGetUserByUsername(Repository::UserRepository& userRepo) {
    TEST_START("Get User By Username");

    auto createResult = userRepo.createUser("frank", "frank@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto getUserResult = userRepo.getUserByUsername("frank");
    TEST_ASSERT(getUserResult.hasValue(), "Get user should succeed");
    TEST_ASSERT(getUserResult->username == "frank", "Username should match");
    TEST_ASSERT(getUserResult->id == createResult->id, "User ID should match");

    TEST_PASS();
}

static bool testGetUserById(Repository::UserRepository& userRepo) {
    TEST_START("Get User By ID");

    auto createResult = userRepo.createUser("grace", "grace@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto getUserResult = userRepo.getUserById(createResult->id);
    TEST_ASSERT(getUserResult.hasValue(), "Get user should succeed");
    TEST_ASSERT(getUserResult->username == "grace", "Username should match");
    TEST_ASSERT(getUserResult->id == createResult->id, "User ID should match");

    TEST_PASS();
}

static bool testChangePassword(Repository::UserRepository& userRepo) {
    TEST_START("Change Password");

    const std::string oldPassword = "OldPass123!";
    const std::string newPassword = "NewPass456!";
    auto createResult = userRepo.createUser("henry", "henry@example.com", oldPassword);
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto changeResult = userRepo.changePassword(createResult->id, oldPassword, newPassword);
    TEST_ASSERT(changeResult.hasValue(), "Password change should succeed");
    TEST_ASSERT(*changeResult == true, "Password change should return true");

    auto authOldResult = userRepo.authenticateUser("henry", oldPassword);
    TEST_ASSERT(!authOldResult.hasValue(), "Old password should not work");

    auto authNewResult = userRepo.authenticateUser("henry", newPassword);
    TEST_ASSERT(authNewResult.hasValue(), "New password should work");

    TEST_PASS();
}

static bool testChangePasswordWrongCurrent(Repository::UserRepository& userRepo) {
    TEST_START("Change Password - Wrong Current Password");

    auto createResult = userRepo.createUser("iris", "iris@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto changeResult = userRepo.changePassword(createResult->id, "WrongPass123!", "NewPass456!");
    TEST_ASSERT(!changeResult.hasValue(), "Password change should fail");
    TEST_ASSERT(changeResult.errorCode == 401, "Error code should be 401");

    TEST_PASS();
}

static bool testIsUsernameAvailable(Repository::UserRepository& userRepo) {
    TEST_START("Is Username Available");

    auto createResult = userRepo.createUser("jack", "jack@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto availableResult1 = userRepo.isUsernameAvailable("jack");
    TEST_ASSERT(availableResult1.hasValue(), "Check should succeed");
    TEST_ASSERT(*availableResult1 == false, "Username 'jack' should not be available");

    auto availableResult2 = userRepo.isUsernameAvailable("newuser");
    TEST_ASSERT(availableResult2.hasValue(), "Check should succeed");
    TEST_ASSERT(*availableResult2 == true, "Username 'newuser' should be available");

    TEST_PASS();
}

static bool testPasswordHashingUniqueness(Repository::UserRepository& userRepo) {
    TEST_START("Password Hashing - Uniqueness");

    const std::string password = "SecurePass123!";
    auto user1 = userRepo.createUser("user1", "user1@example.com", password);
    auto user2 = userRepo.createUser("user2", "user2@example.com", password);

    TEST_ASSERT(user1.hasValue() && user2.hasValue(), "User creation should succeed");
    TEST_ASSERT(user1->passwordHash != user2->passwordHash, "Password hashes should differ");
    TEST_ASSERT(user1->salt != user2->salt, "Salts should differ");

    TEST_PASS();
}

static bool testUpdateLastLogin(Repository::UserRepository& userRepo) {
    TEST_START("Update Last Login");

    auto createResult = userRepo.createUser("karen", "karen@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto updateResult = userRepo.updateLastLogin(createResult->id);
    TEST_ASSERT(updateResult.hasValue(), "Update last login should succeed");
    TEST_ASSERT(*updateResult == true, "Update should return true");

    TEST_PASS();
}

// ============================================================================
// SQL Injection Protection Tests (High Priority Security)
// ============================================================================

static bool testSQLInjectionInUsername(Repository::UserRepository& userRepo) {
    TEST_START("SQL Injection Protection - Username");

    std::vector<std::string> maliciousUsernames = {
        "admin' OR '1'='1",
        "admin'--",
        "admin' /*",
        "' OR 1=1--",
        "admin'; DROP TABLE users;--",
        "' UNION SELECT * FROM users--",
        "1' AND '1'='1",
        "'; DELETE FROM users WHERE '1'='1",
        "admin\\'; DROP TABLE users;--"
    };

    for (const auto& maliciousUsername : maliciousUsernames) {
        auto result = userRepo.createUser(maliciousUsername, "test@example.com", "SecurePass123!");

        if (!result.hasValue()) {
            // Should fail validation
            std::cout << "    Rejected malicious username: " << maliciousUsername << std::endl;
        } else {
            // If it succeeded, verify it was stored as-is (not executed as SQL)
            auto getUserResult = userRepo.getUserByUsername(maliciousUsername);
            TEST_ASSERT(getUserResult.hasValue(), "Should retrieve user with special characters");
            TEST_ASSERT(getUserResult->username == maliciousUsername,
                       "Username should be stored exactly as provided (SQL escaped)");
            std::cout << "    Safely stored username: " << maliciousUsername << std::endl;
        }
    }

    TEST_PASS();
}

static bool testSQLInjectionInPassword(Repository::UserRepository& userRepo) {
    TEST_START("SQL Injection Protection - Password");

    std::string username = "sqlinjtest1";
    std::vector<std::string> maliciousPasswords = {
        "Pass' OR '1'='1",
        "Pass123!'; DROP TABLE users;--",
        "' UNION SELECT password FROM users--",
        "Pass\\'; DELETE FROM users;--"
    };

    for (size_t i = 0; i < maliciousPasswords.size(); i++) {
        std::string user = username + std::to_string(i);
        auto result = userRepo.createUser(user, user + "@example.com", maliciousPasswords[i]);

        if (result.hasValue()) {
            // Password should be hashed and stored safely
            // Try to authenticate with the exact password
            auto authResult = userRepo.authenticateUser(user, maliciousPasswords[i]);
            TEST_ASSERT(authResult.hasValue(),
                       "Should authenticate with password containing SQL injection attempts");
            std::cout << "    Safely hashed password with special chars" << std::endl;
        }
    }

    TEST_PASS();
}

static bool testSQLInjectionInEmail(Repository::UserRepository& userRepo) {
    TEST_START("SQL Injection Protection - Email");

    std::vector<std::string> maliciousEmails = {
        "test' OR '1'='1@example.com",
        "admin'; DROP TABLE users;--@example.com",
        "test@example.com'; DELETE FROM users;--"
    };

    for (size_t i = 0; i < maliciousEmails.size(); i++) {
        std::string username = "emailinjtest" + std::to_string(i);
        auto result = userRepo.createUser(username, maliciousEmails[i], "SecurePass123!");

        if (result.hasValue()) {
            // Email should be stored safely
            auto getUserResult = userRepo.getUserByUsername(username);
            TEST_ASSERT(getUserResult.hasValue(), "Should retrieve user");
            TEST_ASSERT(getUserResult->email == maliciousEmails[i],
                       "Email should be stored exactly as provided (SQL escaped)");
            std::cout << "    Safely stored email: " << maliciousEmails[i] << std::endl;
        }
    }

    TEST_PASS();
}

static bool testSQLInjectionInAuthenticateUser(Repository::UserRepository& userRepo) {
    TEST_START("SQL Injection Protection - Authenticate User");

    // Create a legitimate user
    auto createResult = userRepo.createUser("legituser", "legit@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    // Try SQL injection in authentication
    std::vector<std::pair<std::string, std::string>> maliciousAuth = {
        {"admin' OR '1'='1", "anything"},
        {"legituser", "' OR '1'='1"},
        {"legituser' OR '1'='1--", "password"},
        {"' UNION SELECT * FROM users--", "password"}
    };

    for (const auto& [username, password] : maliciousAuth) {
        auto authResult = userRepo.authenticateUser(username, password);
        TEST_ASSERT(!authResult.hasValue(),
                   "SQL injection attempt should not bypass authentication");
        std::cout << "    Blocked SQL injection: " << username << " / " << password << std::endl;
    }

    TEST_PASS();
}

// ============================================================================
// Unicode and Special Character Edge Cases
// ============================================================================

static bool testUnicodeCharactersInUsername(Repository::UserRepository& userRepo) {
    TEST_START("Unicode Characters in Username");

    std::vector<std::string> unicodeUsernames = {
        "user_中文",           // Chinese characters
        "user_日本語",         // Japanese characters
        "user_한글",           // Korean characters
        "user_Ñoño",          // Spanish characters
        "user_Здравствуй",    // Cyrillic characters
        "user_مرحبا"          // Arabic characters
    };

    for (const auto& username : unicodeUsernames) {
        auto result = userRepo.createUser(username, username + "@example.com", "SecurePass123!");

        if (!result.hasValue()) {
            std::cout << "    Rejected unicode username (validation): " << username << std::endl;
        } else {
            // Verify it can be retrieved
            auto getUserResult = userRepo.getUserByUsername(username);
            if (getUserResult.hasValue() && getUserResult->username == username) {
                std::cout << "    Successfully stored unicode username: " << username << std::endl;
            } else {
                std::cout << "    Warning: Unicode username encoding issue: " << username << std::endl;
            }
        }
    }

    TEST_PASS();
}

static bool testExtremelyLongInputs(Repository::UserRepository& userRepo) {
    TEST_START("Extremely Long Inputs - Buffer Overflow Protection");

    // Test extremely long username (beyond reasonable limits)
    std::string longUsername(1000, 'a');
    auto result1 = userRepo.createUser(longUsername, "test@example.com", "SecurePass123!");
    TEST_ASSERT(!result1.hasValue(), "Should reject extremely long username");
    std::cout << "    Rejected 1000-char username" << std::endl;

    // Test extremely long password
    std::string longPassword = std::string(10000, 'P') + "123!Aa";
    auto result2 = userRepo.createUser("longpassuser", "test@example.com", longPassword);
    if (!result2.hasValue()) {
        std::cout << "    Rejected 10000-char password (validation)" << std::endl;
    } else {
        std::cout << "    Warning: Accepted 10000-char password (may hash to standard length)" << std::endl;
    }

    // Test extremely long email
    std::string longEmail = std::string(500, 'a') + "@example.com";
    auto result3 = userRepo.createUser("longemailuser", longEmail, "SecurePass123!");
    if (!result3.hasValue()) {
        std::cout << "    Rejected 500+ char email" << std::endl;
    }

    TEST_PASS();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    TestUtils::printTestHeader("UserRepository Unit Tests");

    // Initialize database and logger
    Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
    TestUtils::initializeTestLogger("test_user_repo.log");

    if (!TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY)) {
        std::cerr << COLOR_RED << "Failed to initialize test environment" << COLOR_RESET << std::endl;
        return 1;
    }

    Repository::UserRepository userRepo(dbManager);

    // Run core functionality tests
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

    // Run SQL injection protection tests
    std::cout << "\n" << COLOR_CYAN << "Running SQL Injection Protection Tests..." << COLOR_RESET << std::endl;
    testSQLInjectionInUsername(userRepo);
    testSQLInjectionInPassword(userRepo);
    testSQLInjectionInEmail(userRepo);
    testSQLInjectionInAuthenticateUser(userRepo);

    // Run Unicode and extreme input tests
    std::cout << "\n" << COLOR_CYAN << "Running Unicode & Extreme Input Tests..." << COLOR_RESET << std::endl;
    testUnicodeCharactersInUsername(userRepo);
    testExtremelyLongInputs(userRepo);

    // Print summary
    TestUtils::printTestSummary("Test");

    // Cleanup
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

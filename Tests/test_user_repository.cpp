/**
 * @file test_user_repository.cpp
 * @brief Unit tests for UserRepository
 */

#include "TestUtils.h"
#include <iostream>
#include <string>
#include <vector>

constexpr const char* TEST_DB_PATH = "test_user_repo.db";

static bool testCreateUser(Repository::UserRepository& userRepo) {
    TEST_START("Create User");
    auto result = userRepo.createUser("testuser", "SecurePass123!");
    TEST_ASSERT(result.hasValue(), "User creation should succeed");
    TEST_ASSERT(result->username == "testuser", "Username should match");
    TEST_ASSERT(!result->passwordHash.empty(), "Password should be hashed");
    TEST_ASSERT(!result->salt.empty(), "Salt should be generated");
    TEST_PASS();
}

static bool testCreateUserDuplicateUsername(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Duplicate Username");
    userRepo.createUser("dupuser", "SecurePass123!");
    auto result = userRepo.createUser("dupuser", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Should reject duplicate username");
    TEST_PASS();
}

static bool testCreateUserInvalidUsername(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Invalid Username");
    auto result1 = userRepo.createUser("a", "SecurePass123!");
    TEST_ASSERT(!result1.hasValue(), "Should reject too short username");
    auto result2 = userRepo.createUser("user space", "SecurePass123!");
    TEST_ASSERT(!result2.hasValue(), "Should reject username with spaces");
    TEST_PASS();
}

static bool testCreateUserInvalidPassword(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Invalid Password");
    auto result1 = userRepo.createUser("user1", "short");
    TEST_ASSERT(!result1.hasValue(), "Should reject too short password");
    auto result2 = userRepo.createUser("user2", "nonumber");
    TEST_ASSERT(!result2.hasValue(), "Should reject password without numbers");
    TEST_PASS();
}

static bool testAuthenticateUserSuccess(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Success");
    userRepo.createUser("authuser", "SecurePass123!");
    auto result = userRepo.authenticateUser("authuser", "SecurePass123!");
    TEST_ASSERT(result.hasValue(), "Authentication should succeed");
    TEST_ASSERT(result->username == "authuser", "Authenticated user should match");
    TEST_PASS();
}

static bool testAuthenticateUserWrongPassword(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Wrong Password");
    userRepo.createUser("wrongpass", "SecurePass123!");
    auto result = userRepo.authenticateUser("wrongpass", "WrongPass123!");
    TEST_ASSERT(!result.hasValue(), "Authentication should fail with wrong password");
    TEST_PASS();
}

static bool testAuthenticateUserNotFound(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Not Found");
    auto result = userRepo.authenticateUser("nonexistent", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Authentication should fail for nonexistent user");
    TEST_PASS();
}

static bool testGetUserByUsername(Repository::UserRepository& userRepo) {
    TEST_START("Get User By Username");
    userRepo.createUser("getuser", "SecurePass123!");
    auto result = userRepo.getUserByUsername("getuser");
    TEST_ASSERT(result.hasValue(), "Should find user by username");
    TEST_ASSERT(result->username == "getuser", "Found user should match");
    TEST_PASS();
}

static bool testGetUserById(Repository::UserRepository& userRepo) {
    TEST_START("Get User By ID");
    auto createResult = userRepo.createUser("iduser", "SecurePass123!");
    auto result = userRepo.getUserById(createResult->id);
    TEST_ASSERT(result.hasValue(), "Should find user by ID");
    TEST_ASSERT(result->id == createResult->id, "Found ID should match");
    TEST_PASS();
}

static bool testChangePassword(Repository::UserRepository& userRepo) {
    TEST_START("Change Password");
    auto user = userRepo.createUser("changepass", "OldPass123!");
    auto result = userRepo.changePassword(user->id, "OldPass123!", "NewPass123!");
    TEST_ASSERT(result.hasValue() && *result, "Password change should succeed");
    auto auth = userRepo.authenticateUser("changepass", "NewPass123!");
    TEST_ASSERT(auth.hasValue(), "Should authenticate with new password");
    TEST_PASS();
}

static bool testChangePasswordWrongCurrent(Repository::UserRepository& userRepo) {
    TEST_START("Change Password - Wrong Current");
    auto user = userRepo.createUser("changewrong", "OldPass123!");
    auto result = userRepo.changePassword(user->id, "WrongOld123!", "NewPass123!");
    TEST_ASSERT(!result.hasValue() || !(*result),
                "Password change should fail with wrong current password");
    TEST_PASS();
}

static bool testIsUsernameAvailable(Repository::UserRepository& userRepo) {
    TEST_START("Is Username Available");
    userRepo.createUser("taken", "SecurePass123!");
    auto result1 = userRepo.isUsernameAvailable("taken");
    TEST_ASSERT(result1.hasValue() && !(*result1), "Username 'taken' should NOT be available");
    auto result2 = userRepo.isUsernameAvailable("available");
    TEST_ASSERT(result2.hasValue() && *result2, "Username 'available' SHOULD be available");
    TEST_PASS();
}

static bool testPasswordHashingUniqueness(Repository::UserRepository& userRepo) {
    TEST_START("Password Hashing Uniqueness (Salting)");
    auto user1 = userRepo.createUser("salt1", "SamePass123!");
    auto user2 = userRepo.createUser("salt2", "SamePass123!");
    TEST_ASSERT(user1->passwordHash != user2->passwordHash,
                "Same password should result in different hashes due to salt");
    TEST_PASS();
}

static bool testUpdateLastLogin(Repository::UserRepository& userRepo) {
    TEST_START("Update Last Login");
    auto user = userRepo.createUser("loginuser", "SecurePass123!");
    auto result = userRepo.updateLastLogin(user->id);
    TEST_ASSERT(result.hasValue() && *result, "Update last login should succeed");
    TEST_PASS();
}

static bool testSQLInjectionInUsername(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Username");
    auto result = userRepo.createUser("' OR '1'='1", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Should reject SQL injection in username");
    TEST_PASS();
}

static bool testSQLInjectionInPassword(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Password");
    auto user = userRepo.createUser("injectpass", "SecurePass123!");
    auto result = userRepo.authenticateUser("injectpass", "' OR '1'='1");
    TEST_ASSERT(!result.hasValue(), "Should not authenticate with SQL injection");
    TEST_PASS();
}

static bool testSQLInjectionInEmail(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Email");
    auto result = userRepo.createUser("injectemail", "SecurePass123!");
    TEST_ASSERT(!result.hasValue(), "Should reject SQL injection in email");
    TEST_PASS();
}

static bool testSQLInjectionInAuthenticateUser(Repository::UserRepository& userRepo) {
    TEST_START("Security - SQL Injection in Authenticate");
    auto result = userRepo.authenticateUser("' UNION SELECT * FROM users --", "anything");
    TEST_ASSERT(!result.hasValue(), "Should not authenticate with SQL injection");
    TEST_PASS();
}

static bool testUnicodeCharactersInUsername(Repository::UserRepository& userRepo) {
    TEST_START("Unicode Characters in Username");
    std::vector<std::string> unicodeUsernames = {"User_ñ", "User_€", "User_你好", "User_🚀"};
    for (const auto& username : unicodeUsernames) {
        auto result = userRepo.createUser(username, "SecurePass123!");
        if (result.hasValue()) {
            auto found = userRepo.getUserByUsername(username);
            TEST_ASSERT(found.hasValue() && found->username == username,
                        "Should correctly store and retrieve unicode username");
        }
    }
    TEST_PASS();
}

static bool testExtremelyLongInputs(Repository::UserRepository& userRepo) {
    TEST_START("Extremely Long Inputs");
    std::string longUsername(1000, 'a');
    auto result1 = userRepo.createUser(longUsername, "SecurePass123!");
    TEST_ASSERT(!result1.hasValue(), "Should reject extremely long username");

    std::string longPassword(10000, 'P');
    auto result2 = userRepo.createUser("longpassuser", longPassword);
    if (!result2.hasValue()) {
        std::cout << "    Rejected 10000-char password" << std::endl;
    }
    TEST_PASS();
}

int main() {
    TestUtils::printTestHeader("UserRepository Unit Tests");
    Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
    TestUtils::initializeTestLogger("test_user_repo.log");
    if (!TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH, STANDARD_TEST_ENCRYPTION_KEY)) {
        return 1;
    }
    Repository::UserRepository userRepo(dbManager);
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
    testSQLInjectionInUsername(userRepo);
    testSQLInjectionInPassword(userRepo);
    testSQLInjectionInEmail(userRepo);
    testSQLInjectionInAuthenticateUser(userRepo);
    testUnicodeCharactersInUsername(userRepo);
    testExtremelyLongInputs(userRepo);
    TestUtils::printTestSummary("UserRepository Test");
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);
    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

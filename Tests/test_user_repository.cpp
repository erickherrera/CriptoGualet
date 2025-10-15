/**
 * @file test_user_repository.cpp
 * @brief Unit tests for UserRepository
 *
 * Tests user creation, authentication, password management, and validation.
 */

#include "TestUtils.h"

const std::string TEST_DB_PATH = "test_user_repo.db";

// ============================================================================
// Test Cases
// ============================================================================

bool testCreateUser(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Valid Input");

    auto result = userRepo.createUser("alice", "alice@example.com", "SecurePass123!");
    TEST_ASSERT(result.hasValue(), "User creation should succeed");
    TEST_ASSERT(result->username == "alice", "Username should match");
    TEST_ASSERT(result->email == "alice@example.com", "Email should match");
    TEST_ASSERT(result->id > 0, "User ID should be positive");
    TEST_ASSERT(result->isActive, "User should be active by default");

    TEST_PASS();
}

bool testCreateUserDuplicateUsername(Repository::UserRepository& userRepo) {
    TEST_START("Create User - Duplicate Username");

    auto result1 = userRepo.createUser("bob", "bob@example.com", "SecurePass123!");
    TEST_ASSERT(result1.hasValue(), "First user creation should succeed");

    auto result2 = userRepo.createUser("bob", "bob2@example.com", "SecurePass123!");
    TEST_ASSERT(!result2.hasValue(), "Duplicate username should fail");
    TEST_ASSERT(result2.errorCode == 409, "Error code should be 409 (Conflict)");

    TEST_PASS();
}

bool testCreateUserInvalidUsername(Repository::UserRepository& userRepo) {
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

bool testCreateUserInvalidPassword(Repository::UserRepository& userRepo) {
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

bool testAuthenticateUserSuccess(Repository::UserRepository& userRepo) {
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

bool testAuthenticateUserWrongPassword(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - Wrong Password");

    auto createResult = userRepo.createUser("eve", "eve@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto authResult = userRepo.authenticateUser("eve", "WrongPassword123!");
    TEST_ASSERT(!authResult.hasValue(), "Authentication should fail");
    TEST_ASSERT(authResult.errorCode == 401, "Error code should be 401 (Unauthorized)");

    TEST_PASS();
}

bool testAuthenticateUserNotFound(Repository::UserRepository& userRepo) {
    TEST_START("Authenticate User - User Not Found");

    auto authResult = userRepo.authenticateUser("nonexistent", "SecurePass123!");
    TEST_ASSERT(!authResult.hasValue(), "Authentication should fail");
    TEST_ASSERT(authResult.errorCode == 401, "Error code should be 401");

    TEST_PASS();
}

bool testGetUserByUsername(Repository::UserRepository& userRepo) {
    TEST_START("Get User By Username");

    auto createResult = userRepo.createUser("frank", "frank@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto getUserResult = userRepo.getUserByUsername("frank");
    TEST_ASSERT(getUserResult.hasValue(), "Get user should succeed");
    TEST_ASSERT(getUserResult->username == "frank", "Username should match");
    TEST_ASSERT(getUserResult->id == createResult->id, "User ID should match");

    TEST_PASS();
}

bool testGetUserById(Repository::UserRepository& userRepo) {
    TEST_START("Get User By ID");

    auto createResult = userRepo.createUser("grace", "grace@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto getUserResult = userRepo.getUserById(createResult->id);
    TEST_ASSERT(getUserResult.hasValue(), "Get user should succeed");
    TEST_ASSERT(getUserResult->username == "grace", "Username should match");
    TEST_ASSERT(getUserResult->id == createResult->id, "User ID should match");

    TEST_PASS();
}

bool testChangePassword(Repository::UserRepository& userRepo) {
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

bool testChangePasswordWrongCurrent(Repository::UserRepository& userRepo) {
    TEST_START("Change Password - Wrong Current Password");

    auto createResult = userRepo.createUser("iris", "iris@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto changeResult = userRepo.changePassword(createResult->id, "WrongPass123!", "NewPass456!");
    TEST_ASSERT(!changeResult.hasValue(), "Password change should fail");
    TEST_ASSERT(changeResult.errorCode == 401, "Error code should be 401");

    TEST_PASS();
}

bool testIsUsernameAvailable(Repository::UserRepository& userRepo) {
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

bool testPasswordHashingUniqueness(Repository::UserRepository& userRepo) {
    TEST_START("Password Hashing - Uniqueness");

    const std::string password = "SecurePass123!";
    auto user1 = userRepo.createUser("user1", "user1@example.com", password);
    auto user2 = userRepo.createUser("user2", "user2@example.com", password);

    TEST_ASSERT(user1.hasValue() && user2.hasValue(), "User creation should succeed");
    TEST_ASSERT(user1->passwordHash != user2->passwordHash, "Password hashes should differ");
    TEST_ASSERT(user1->salt != user2->salt, "Salts should differ");

    TEST_PASS();
}

bool testUpdateLastLogin(Repository::UserRepository& userRepo) {
    TEST_START("Update Last Login");

    auto createResult = userRepo.createUser("karen", "karen@example.com", "SecurePass123!");
    TEST_ASSERT(createResult.hasValue(), "User creation should succeed");

    auto updateResult = userRepo.updateLastLogin(createResult->id);
    TEST_ASSERT(updateResult.hasValue(), "Update last login should succeed");
    TEST_ASSERT(*updateResult == true, "Update should return true");

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

    // Run all tests
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

    // Print summary
    TestUtils::printTestSummary("Test");

    // Cleanup
    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

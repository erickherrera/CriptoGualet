// Test utility to verify password storage and authentication
// Usage: test_password_verification <username> <password>

#include <iostream>
#include <string>
#include <vector>
#include "../backend/core/include/Auth.h"
#include "../backend/database/include/Database/DatabaseManager.h"
#include "../backend/repository/include/Repository/UserRepository.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <username> <password>" << std::endl;
        std::cerr << "Example: " << argv[0] << " testuser mypassword123" << std::endl;
        return 1;
    }

    std::string username = argv[1];
    std::string password = argv[2];

    std::cout << "=== Password Verification Test ===" << std::endl;
    std::cout << "Username: " << username << std::endl;
    std::cout << "Password length: " << password.length() << " characters" << std::endl;
    std::cout << std::endl;

    // Initialize Auth database
    std::cout << "[1/4] Initializing database..." << std::endl;
    if (!Auth::InitializeAuthDatabase()) {
        std::cerr << "ERROR: Failed to initialize authentication database" << std::endl;
        return 1;
    }
    std::cout << "      ✓ Database initialized successfully" << std::endl;
    std::cout << std::endl;

    // Try to authenticate the user
    std::cout << "[2/4] Attempting authentication..." << std::endl;
    auto loginResult = Auth::LoginUser(username, password);

    if (loginResult.success()) {
        std::cout << "      ✓ Authentication SUCCESSFUL!" << std::endl;
        std::cout << "      " << loginResult.message << std::endl;
    } else {
        std::cout << "      ✗ Authentication FAILED" << std::endl;
        std::cout << "      Result code: " << static_cast<int>(loginResult.result) << std::endl;
        std::cout << "      Message: " << loginResult.message << std::endl;
    }
    std::cout << std::endl;

    // Try to get user details from database
    std::cout << "[3/4] Checking user in database..." << std::endl;
    try {
        auto& dbManager = Database::DatabaseManager::getInstance();
        Repository::UserRepository userRepo(dbManager);

        auto userResult = userRepo.getUserByUsername(username);
        if (userResult.success) {
            std::cout << "      ✓ User found in database" << std::endl;
            std::cout << "      User ID: " << userResult.data.id << std::endl;
            std::cout << "      Username: " << userResult.data.username << std::endl;
            std::cout << "      Password hash length: " << userResult.data.passwordHash.length() << " chars" << std::endl;
            std::cout << "      Salt length: " << userResult.data.salt.size() << " bytes" << std::endl;
            std::cout << "      Is active: " << (userResult.data.isActive ? "Yes" : "No") << std::endl;
        } else {
            std::cout << "      ✗ User NOT found in database" << std::endl;
            std::cout << "      Error: " << userResult.errorMessage << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "      ✗ Exception: " << e.what() << std::endl;
    }
    std::cout << std::endl;

    // Test password hashing consistency
    std::cout << "[4/4] Testing password hash generation..." << std::endl;
    std::string hash1 = Auth::CreatePasswordHash(password);
    std::string hash2 = Auth::CreatePasswordHash(password);

    if (!hash1.empty()) {
        std::cout << "      ✓ Password hash generated successfully" << std::endl;
        std::cout << "      Hash format: " << hash1.substr(0, 50) << "..." << std::endl;

        // Verify the hash we just created
        bool verifies = Auth::VerifyPassword(password, hash1);
        std::cout << "      Hash verification: " << (verifies ? "PASS" : "FAIL") << std::endl;

        // Check if two hashes are different (they should be due to random salt)
        bool different = (hash1 != hash2);
        std::cout << "      Salt randomization: " << (different ? "WORKING" : "NOT WORKING") << std::endl;
    } else {
        std::cout << "      ✗ Failed to generate password hash" << std::endl;
    }
    std::cout << std::endl;

    // Summary
    std::cout << "=== Summary ===" << std::endl;
    if (loginResult.success()) {
        std::cout << "✓ Password is stored and verified correctly!" << std::endl;
        std::cout << "  The user can log in successfully." << std::endl;
    } else {
        std::cout << "✗ Password verification failed!" << std::endl;
        std::cout << "  Possible issues:" << std::endl;
        std::cout << "  1. The password you entered doesn't match what was registered" << std::endl;
        std::cout << "  2. The password hash was not stored correctly during registration" << std::endl;
        std::cout << "  3. The database may be corrupted" << std::endl;
        std::cout << std::endl;
        std::cout << "  Recommendations:" << std::endl;
        std::cout << "  - Double-check the password (case-sensitive)" << std::endl;
        std::cout << "  - Try creating a new user and test with that" << std::endl;
        std::cout << "  - Check the application logs for errors during registration" << std::endl;
    }

    return loginResult.success() ? 0 : 1;
}

#include "../backend/core/include/Auth.h"
#include "../backend/core/include/Crypto.h"
#include "../backend/database/include/Database/DatabaseManager.h"
#include "../backend/repository/include/Repository/UserRepository.h"
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cassert>

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

/**
 * 2FA (Two-Factor Authentication) Test Suite - TOTP Version
 *
 * Tests the complete TOTP-based 2FA flow including:
 * - TOTP secret generation
 * - TOTP code verification (valid, invalid)
 * - Enable/Disable 2FA with authenticator app
 * - Login with 2FA enabled (TOTP_REQUIRED response)
 * - Backup codes for recovery
 * - Edge cases and error handling
 *
 * NOTE: Tests run against the real wallet.db used by Auth.cpp
 */

namespace {
    const std::string TEST_DB_PATH = "wallet.db";

    void cleanupTestDatabase() {
        try {
            if (std::filesystem::exists(TEST_DB_PATH)) {
                std::filesystem::remove(TEST_DB_PATH);
            }
            std::string walPath = std::string(TEST_DB_PATH) + "-wal";
            std::string shmPath = std::string(TEST_DB_PATH) + "-shm";
            if (std::filesystem::exists(walPath)) {
                std::filesystem::remove(walPath);
            }
            if (std::filesystem::exists(shmPath)) {
                std::filesystem::remove(shmPath);
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not clean up database files: " << e.what() << std::endl;
        }
    }

    void logTestResult(const std::string& testName, bool passed) {
        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << testName << std::endl;
        if (!passed) {
            std::cerr << "FAILED: " << testName << std::endl;
        }
    }
}

class TwoFactorAuthTests {
private:
    Database::DatabaseManager* dbManager;
    std::unique_ptr<Repository::UserRepository> userRepo;

public:
    bool initialize() {
        std::cout << "\n=== Initializing TOTP 2FA Tests ===" << std::endl;

        try {
            cleanupTestDatabase();

            bool authInit = Auth::InitializeAuthDatabase();
            if (!authInit) {
                std::cerr << "Failed to initialize Auth database" << std::endl;
                return false;
            }

            dbManager = &Database::DatabaseManager::getInstance();
            userRepo = std::make_unique<Repository::UserRepository>(*dbManager);

            std::cout << "Initialization successful" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Initialization failed: " << e.what() << std::endl;
            return false;
        }
    }

    void cleanup() {
        std::cout << "\n=== Cleaning up test environment ===" << std::endl;
        userRepo.reset();
        if (dbManager) {
            dbManager->close();
            dbManager = nullptr;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        cleanupTestDatabase();
    }

    bool testTOTPGeneration() {
        std::cout << "\n=== Test 1: TOTP Secret Generation ===" << std::endl;

        bool allPassed = true;

        // Test generating TOTP secret
        {
            std::vector<uint8_t> secret;
            bool generated = Crypto::GenerateTOTPSecret(secret);
            bool passed = generated && secret.size() == 20;  // 160 bits
            logTestResult("Generate 160-bit TOTP secret", passed);
            allPassed &= passed;
        }

        // Test Base32 encoding
        {
            std::vector<uint8_t> testData = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
            std::string encoded = Crypto::Base32Encode(testData);
            bool passed = !encoded.empty() && encoded == "JBSWY3DP";
            logTestResult("Base32 encoding", passed);
            allPassed &= passed;
        }

        // Test Base32 decoding
        {
            std::vector<uint8_t> decoded = Crypto::Base32Decode("JBSWY3DP");
            bool passed = decoded.size() == 5 && 
                         decoded[0] == 0x48 && 
                         decoded[1] == 0x65;
            logTestResult("Base32 decoding", passed);
            allPassed &= passed;
        }

        // Test TOTP URI generation
        {
            std::string uri = Crypto::GenerateTOTPUri("JBSWY3DP", "testuser", "CriptoGualet");
            bool passed = uri.find("otpauth://totp/") != std::string::npos &&
                         uri.find("secret=JBSWY3DP") != std::string::npos &&
                         uri.find("issuer=CriptoGualet") != std::string::npos;
            logTestResult("TOTP URI generation", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testTOTPVerification() {
        std::cout << "\n=== Test 2: TOTP Code Verification ===" << std::endl;

        bool allPassed = true;

        // Generate a test secret
        std::vector<uint8_t> secret;
        Crypto::GenerateTOTPSecret(secret);
        std::string secretBase32 = Crypto::Base32Encode(secret);

        // Generate and verify a code for current time
        {
            std::string code = Crypto::GenerateTOTP(secret);
            bool passed = code.length() == 6;
            logTestResult("Generate 6-digit TOTP code", passed);
            allPassed &= passed;
        }

        // Verify generated code
        {
            std::string code = Crypto::GenerateTOTP(secret);
            bool verified = Crypto::VerifyTOTP(secret, code);
            logTestResult("Verify valid TOTP code", verified);
            allPassed &= verified;
        }

        // Verify with window tolerance
        {
            // Code from 30 seconds ago should still work with window=1
            uint64_t pastTime = static_cast<uint64_t>(std::time(nullptr)) - 30;
            std::string pastCode = Crypto::GenerateTOTP(secret, pastTime);
            bool verified = Crypto::VerifyTOTP(secret, pastCode, 1);
            logTestResult("Verify TOTP code with time window", verified);
            allPassed &= verified;
        }

        // Reject invalid code
        {
            bool rejected = !Crypto::VerifyTOTP(secret, "000000");
            logTestResult("Reject invalid TOTP code", rejected);
            allPassed &= rejected;
        }

        return allPassed;
    }

    bool testInitiateTwoFactorSetup() {
        std::cout << "\n=== Test 3: Initiate 2FA Setup ===" << std::endl;

        bool allPassed = true;

        // Register a user first
        std::string testUsername = "2fa_totp_user1";
        std::string testPassword = "TestP@ssw0rd1!";
        std::vector<std::string> mnemonic;

        Auth::AuthResponse registerResponse = Auth::RegisterUserWithMnemonic(
            testUsername, "", testPassword, mnemonic
        );

        {
            bool passed = (registerResponse.result == Auth::AuthResult::SUCCESS);
            logTestResult("Register user for 2FA test", passed);
            allPassed &= passed;
            if (!passed) return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Test initiating 2FA setup
        {
            Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
                testUsername, testPassword
            );
            bool passed = setupData.success && 
                         !setupData.secretBase32.empty() &&
                         !setupData.otpauthUri.empty();
            if (!passed) {
                std::cerr << "Setup failed: " << setupData.errorMessage << std::endl;
            }
            logTestResult("Initiate 2FA setup", passed);
            allPassed &= passed;
        }

        // Test with wrong password
        {
            Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
                testUsername, "WrongPassword!"
            );
            bool passed = !setupData.success;
            logTestResult("Reject setup with wrong password", passed);
            allPassed &= passed;
        }

        // Test with non-existent user
        {
            Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
                "nonexistent_user", testPassword
            );
            bool passed = !setupData.success;
            logTestResult("Reject setup for non-existent user", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testConfirmTwoFactorSetup() {
        std::cout << "\n=== Test 4: Confirm 2FA Setup ===" << std::endl;

        bool allPassed = true;

        // Register a user
        std::string testUsername = "2fa_totp_user2";
        std::string testPassword = "TestP@ssw0rd2!";
        std::vector<std::string> mnemonic;

        Auth::RegisterUserWithMnemonic(testUsername, "", testPassword, mnemonic);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Initiate setup to get secret
        Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
            testUsername, testPassword
        );

        if (!setupData.success) {
            std::cerr << "Could not initiate setup for confirm test" << std::endl;
            return false;
        }

        // Decode the secret
        std::vector<uint8_t> secret = Crypto::Base32Decode(setupData.secretBase32);

        // Generate valid TOTP code
        std::string validCode = Crypto::GenerateTOTP(secret);

        // Test confirming with valid code
        {
            Auth::AuthResponse confirmResponse = Auth::ConfirmTwoFactorSetup(
                testUsername, validCode
            );
            bool passed = confirmResponse.success();
            if (!passed) {
                std::cerr << "Confirm failed: " << confirmResponse.message << std::endl;
            }
            logTestResult("Confirm 2FA setup with valid code", passed);
            allPassed &= passed;
        }

        // Verify 2FA is now enabled
        {
            bool enabled = Auth::IsTwoFactorEnabled(testUsername);
            logTestResult("2FA is enabled after confirmation", enabled);
            allPassed &= enabled;
        }

        // Test confirming with invalid code (for another user)
        {
            std::string testUsername3 = "2fa_totp_user3";
            std::vector<std::string> mnemonic3;
            Auth::RegisterUserWithMnemonic(testUsername3, "", testPassword, mnemonic3);
            Auth::InitiateTwoFactorSetup(testUsername3, testPassword);

            Auth::AuthResponse confirmResponse = Auth::ConfirmTwoFactorSetup(
                testUsername3, "000000"
            );
            bool passed = !confirmResponse.success();
            logTestResult("Reject invalid confirmation code", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testLoginWithTwoFactor() {
        std::cout << "\n=== Test 5: Login with 2FA Enabled ===" << std::endl;

        bool allPassed = true;

        // Register and enable 2FA for a user
        std::string testUsername = "2fa_totp_user4";
        std::string testPassword = "TestP@ssw0rd4!";
        std::vector<std::string> mnemonic;

        Auth::RegisterUserWithMnemonic(testUsername, "", testPassword, mnemonic);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Enable 2FA
        Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
            testUsername, testPassword
        );
        std::vector<uint8_t> secret = Crypto::Base32Decode(setupData.secretBase32);
        std::string validCode = Crypto::GenerateTOTP(secret);
        Auth::ConfirmTwoFactorSetup(testUsername, validCode);

        // Test login - should require TOTP
        {
            Auth::AuthResponse loginResponse = Auth::LoginUser(testUsername, testPassword);
            bool passed = !loginResponse.success() && 
                         loginResponse.message.find("TOTP_REQUIRED") != std::string::npos;
            logTestResult("Login requires TOTP when 2FA enabled", passed);
            allPassed &= passed;
        }

        // Test TOTP verification
        {
            std::string currentCode = Crypto::GenerateTOTP(secret);
            Auth::AuthResponse verifyResponse = Auth::VerifyTwoFactorCode(
                testUsername, currentCode
            );
            bool passed = verifyResponse.success();
            logTestResult("Verify TOTP code for login", passed);
            allPassed &= passed;
        }

        // Test with invalid TOTP
        {
            Auth::AuthResponse verifyResponse = Auth::VerifyTwoFactorCode(
                testUsername, "000000"
            );
            bool passed = !verifyResponse.success();
            logTestResult("Reject invalid TOTP for login", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testDisableTwoFactor() {
        std::cout << "\n=== Test 6: Disable 2FA ===" << std::endl;

        bool allPassed = true;

        // Register and enable 2FA
        std::string testUsername = "2fa_totp_user5";
        std::string testPassword = "TestP@ssw0rd5!";
        std::vector<std::string> mnemonic;

        Auth::RegisterUserWithMnemonic(testUsername, "", testPassword, mnemonic);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
            testUsername, testPassword
        );
        std::vector<uint8_t> secret = Crypto::Base32Decode(setupData.secretBase32);
        std::string validCode = Crypto::GenerateTOTP(secret);
        Auth::ConfirmTwoFactorSetup(testUsername, validCode);

        // Verify 2FA is enabled
        {
            bool enabled = Auth::IsTwoFactorEnabled(testUsername);
            bool passed = enabled;
            logTestResult("2FA is enabled before disable test", passed);
            allPassed &= passed;
        }

        // Test disabling with wrong password
        {
            std::string currentCode = Crypto::GenerateTOTP(secret);
            Auth::AuthResponse response = Auth::DisableTwoFactor(
                testUsername, "WrongPassword!", currentCode
            );
            bool passed = !response.success();
            logTestResult("Reject disable with wrong password", passed);
            allPassed &= passed;
        }

        // Test disabling with wrong TOTP
        {
            Auth::AuthResponse response = Auth::DisableTwoFactor(
                testUsername, testPassword, "000000"
            );
            bool passed = !response.success();
            logTestResult("Reject disable with wrong TOTP", passed);
            allPassed &= passed;
        }

        // Test disabling with correct credentials
        {
            std::string currentCode = Crypto::GenerateTOTP(secret);
            Auth::AuthResponse response = Auth::DisableTwoFactor(
                testUsername, testPassword, currentCode
            );
            bool passed = response.success();
            if (!passed) {
                std::cerr << "Disable failed: " << response.message << std::endl;
            }
            logTestResult("Disable 2FA with correct credentials", passed);
            allPassed &= passed;
        }

        // Verify 2FA is disabled
        {
            bool disabled = !Auth::IsTwoFactorEnabled(testUsername);
            logTestResult("2FA is disabled after disable call", disabled);
            allPassed &= disabled;
        }

        return allPassed;
    }

    bool testBackupCodes() {
        std::cout << "\n=== Test 7: Backup Codes ===" << std::endl;

        bool allPassed = true;

        // Register and enable 2FA
        std::string testUsername = "2fa_totp_user6";
        std::string testPassword = "TestP@ssw0rd6!";
        std::vector<std::string> mnemonic;

        Auth::RegisterUserWithMnemonic(testUsername, "", testPassword, mnemonic);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        Auth::TwoFactorSetupData setupData = Auth::InitiateTwoFactorSetup(
            testUsername, testPassword
        );
        std::vector<uint8_t> secret = Crypto::Base32Decode(setupData.secretBase32);
        std::string validCode = Crypto::GenerateTOTP(secret);
        Auth::ConfirmTwoFactorSetup(testUsername, validCode);

        // Get backup codes
        Auth::BackupCodesResult backupResult = Auth::GetBackupCodes(
            testUsername, testPassword
        );

        {
            bool passed = backupResult.success && backupResult.codes.size() == 8;
            logTestResult("Get 8 backup codes after enabling 2FA", passed);
            allPassed &= passed;
            if (!passed) {
                std::cerr << "Backup codes error: " << backupResult.errorMessage << std::endl;
                return allPassed;
            }
        }

        // Test using a backup code
        std::string backupCode = backupResult.codes[0];
        {
            Auth::AuthResponse response = Auth::UseBackupCode(testUsername, backupCode);
            bool passed = response.success();
            if (!passed) {
                std::cerr << "Backup code failed: " << response.message << std::endl;
            }
            logTestResult("Use valid backup code to disable 2FA", passed);
            allPassed &= passed;
        }

        // Verify 2FA is disabled
        {
            bool disabled = !Auth::IsTwoFactorEnabled(testUsername);
            logTestResult("2FA is disabled after using backup code", disabled);
            allPassed &= disabled;
        }

        // Test using same backup code again (should fail)
        {
            Auth::AuthResponse response = Auth::UseBackupCode(testUsername, backupCode);
            bool passed = !response.success();
            logTestResult("Reject reused backup code", passed);
            allPassed &= passed;
        }

        return allPassed;
    }

    bool testLoginWithoutTwoFactor() {
        std::cout << "\n=== Test 8: Login Without 2FA ===" << std::endl;

        bool allPassed = true;

        // Register a user without enabling 2FA
        std::string testUsername = "2fa_totp_user7";
        std::string testPassword = "TestP@ssw0rd7!";
        std::vector<std::string> mnemonic;

        Auth::RegisterUserWithMnemonic(testUsername, "", testPassword, mnemonic);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Test normal login without 2FA
        {
            Auth::AuthResponse loginResponse = Auth::LoginUser(testUsername, testPassword);
            bool passed = loginResponse.success();
            logTestResult("Login succeeds without 2FA", passed);
            allPassed &= passed;
        }

        // Verify 2FA is not enabled
        {
            bool notEnabled = !Auth::IsTwoFactorEnabled(testUsername);
            logTestResult("2FA is not enabled for new user", notEnabled);
            allPassed &= notEnabled;
        }

        return allPassed;
    }

    bool runAllTests() {
        bool allPassed = true;

        allPassed &= testTOTPGeneration();
        allPassed &= testTOTPVerification();
        allPassed &= testInitiateTwoFactorSetup();
        allPassed &= testConfirmTwoFactorSetup();
        allPassed &= testLoginWithTwoFactor();
        allPassed &= testDisableTwoFactor();
        allPassed &= testBackupCodes();
        allPassed &= testLoginWithoutTwoFactor();

        return allPassed;
    }
};

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  TOTP 2FA (Authenticator App) Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    TwoFactorAuthTests tests;

    if (!tests.initialize()) {
        std::cerr << "Failed to initialize tests" << std::endl;
        return 1;
    }

    bool allPassed = tests.runAllTests();

    tests.cleanup();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "  ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "  SOME TESTS FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return allPassed ? 0 : 1;
}

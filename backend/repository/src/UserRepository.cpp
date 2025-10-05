#include "Repository/UserRepository.h"
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <cctype>

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

namespace Repository {

UserRepository::UserRepository(Database::DatabaseManager& dbManager) : m_dbManager(dbManager) {
    REPO_LOG_INFO(COMPONENT_NAME, "UserRepository initialized");
}

Result<User> UserRepository::createUser(const std::string& username, const std::string& email, const std::string& password) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "createUser");

    // Validate input parameters
    auto usernameValidation = validateUsername(username);
    if (!usernameValidation) {
        return Result<User>(usernameValidation.error(), 400);
    }

    auto passwordValidation = validatePassword(password);
    if (!passwordValidation || !passwordValidation->isValid) {
        std::string error = "Password validation failed";
        if (passwordValidation.hasValue() && !passwordValidation->errorMessage.empty()) {
            error += ": " + passwordValidation->errorMessage;
        }
        return Result<User>(error, 400);
    }

    // Check if username is available
    auto availabilityCheck = isUsernameAvailable(username);
    if (!availabilityCheck || !(*availabilityCheck)) {
        return Result<User>("Username is already taken", 409);
    }

    // Hash password with salt
    auto hashResult = hashPassword(password);
    if (!hashResult) {
        REPO_LOG_ERROR(COMPONENT_NAME, "Failed to hash password", hashResult.error());
        return Result<User>("Failed to create user: password hashing error", 500);
    }

    // Begin transaction
    auto transaction = m_dbManager.beginTransaction();
    if (!transaction) {
        REPO_LOG_ERROR(COMPONENT_NAME, "Failed to begin transaction", transaction.message);
        return Result<User>("Database transaction error", 500);
    }

    try {
        // Insert user into database
        const std::string sql = R"(
            INSERT INTO users (username, email, password_hash, salt, created_at, wallet_version, is_active)
            VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, 1, 1)
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            m_dbManager.rollbackTransaction();
            return Result<User>("Failed to prepare user insertion statement", 500);
        }

        // Bind parameters
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, hashResult->hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 4, hashResult->salt.data(), static_cast<int>(hashResult->salt.size()), SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            m_dbManager.rollbackTransaction();
            return Result<User>("Failed to create user: database insertion failed", 500);
        }

        int userId = static_cast<int>(sqlite3_last_insert_rowid(m_dbManager.getHandle()));
        sqlite3_finalize(stmt);

        // Commit transaction
        auto commitResult = m_dbManager.commitTransaction();
        if (!commitResult) {
            REPO_LOG_ERROR(COMPONENT_NAME, "Failed to commit user creation", commitResult.message);
            return Result<User>("Failed to commit user creation", 500);
        }

        // Retrieve and return created user
        auto userResult = getUserById(userId);
        if (userResult) {
            REPO_LOG_INFO(COMPONENT_NAME, "User created successfully", "Username: " + username + ", ID: " + std::to_string(userId));
        }

        return userResult;

    } catch (const std::exception& e) {
        m_dbManager.rollbackTransaction();
        REPO_LOG_ERROR(COMPONENT_NAME, "Exception during user creation", e.what());
        return Result<User>("Internal error during user creation", 500);
    }
}

Result<User> UserRepository::authenticateUser(const std::string& username, const std::string& password) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "authenticateUser");

    if (username.empty() || password.empty()) {
        return Result<User>("Username and password are required", 400);
    }

    // Get user by username
    auto userResult = getUserByUsername(username);
    if (!userResult) {
        REPO_LOG_WARNING(COMPONENT_NAME, "Authentication failed: user not found", "Username: " + username);
        return Result<User>("Invalid username or password", 401);
    }

    User user = *userResult;

    // Check if user is active
    if (!user.isActive) {
        REPO_LOG_WARNING(COMPONENT_NAME, "Authentication failed: user inactive", "Username: " + username);
        return Result<User>("Account is disabled", 403);
    }

    // Verify password
    auto verificationResult = verifyPassword(password, user.passwordHash, user.salt);
    if (!verificationResult || !(*verificationResult)) {
        REPO_LOG_WARNING(COMPONENT_NAME, "Authentication failed: invalid password", "Username: " + username);
        return Result<User>("Invalid username or password", 401);
    }

    // Update last login
    updateLastLogin(user.id);

    REPO_LOG_INFO(COMPONENT_NAME, "User authenticated successfully", "Username: " + username + ", ID: " + std::to_string(user.id));
    return Result<User>(user);
}

Result<User> UserRepository::getUserByUsername(const std::string& username) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getUserByUsername");

    const std::string sql = R"(
        SELECT id, username, email, password_hash, salt, created_at, last_login, wallet_version, is_active
        FROM users
        WHERE username = ? AND is_active = 1
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<User>("Failed to prepare user query", 500);
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        User user = mapRowToUser(stmt);
        sqlite3_finalize(stmt);
        return Result<User>(user);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<User>("User not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<User>("Database error while retrieving user", 500);
    }
}

Result<User> UserRepository::getUserById(int userId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getUserById");

    const std::string sql = R"(
        SELECT id, username, email, password_hash, salt, created_at, last_login, wallet_version, is_active
        FROM users
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<User>("Failed to prepare user query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        User user = mapRowToUser(stmt);
        sqlite3_finalize(stmt);
        return Result<User>(user);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<User>("User not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<User>("Database error while retrieving user", 500);
    }
}

Result<bool> UserRepository::updateLastLogin(int userId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "updateLastLogin");

    const std::string sql = R"(
        UPDATE users
        SET last_login = CURRENT_TIMESTAMP
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare update statement", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        int changes = sqlite3_changes(m_dbManager.getHandle());
        if (changes > 0) {
            return Result<bool>(true);
        } else {
            return Result<bool>("User not found", 404);
        }
    } else {
        return Result<bool>("Database error during update", 500);
    }
}

Result<bool> UserRepository::changePassword(int userId, const std::string& currentPassword, const std::string& newPassword) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "changePassword");

    // Get current user
    auto userResult = getUserById(userId);
    if (!userResult) {
        return Result<bool>(userResult.error(), userResult.errorCode);
    }

    User user = *userResult;

    // Verify current password
    auto verificationResult = verifyPassword(currentPassword, user.passwordHash, user.salt);
    if (!verificationResult || !(*verificationResult)) {
        REPO_LOG_WARNING(COMPONENT_NAME, "Password change failed: invalid current password", "UserID: " + std::to_string(userId));
        return Result<bool>("Current password is incorrect", 401);
    }

    // Validate new password
    auto passwordValidation = validatePassword(newPassword);
    if (!passwordValidation || !passwordValidation->isValid) {
        std::string error = "New password validation failed";
        if (passwordValidation.hasValue() && !passwordValidation->violations.empty()) {
            error += ": " + passwordValidation->violations[0];
        }
        return Result<bool>(error, 400);
    }

    // Hash new password
    auto hashResult = hashPassword(newPassword);
    if (!hashResult) {
        REPO_LOG_ERROR(COMPONENT_NAME, "Failed to hash new password", hashResult.error());
        return Result<bool>("Failed to change password: hashing error", 500);
    }

    // Update password in database
    const std::string sql = R"(
        UPDATE users
        SET password_hash = ?, salt = ?
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare password update statement", 500);
    }

    sqlite3_bind_text(stmt, 1, hashResult->hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, hashResult->salt.data(), static_cast<int>(hashResult->salt.size()), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, userId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        REPO_LOG_INFO(COMPONENT_NAME, "Password changed successfully", "UserID: " + std::to_string(userId));
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during password update", 500);
    }
}

Result<bool> UserRepository::isUsernameAvailable(const std::string& username) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "isUsernameAvailable");

    const std::string sql = "SELECT COUNT(*) FROM users WHERE username = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare username check statement", 500);
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return Result<bool>(count == 0);
    } else {
        sqlite3_finalize(stmt);
        return Result<bool>("Database error during username check", 500);
    }
}

Result<PasswordHashResult> UserRepository::hashPassword(const std::string& password, std::vector<uint8_t> salt) {
    if (salt.empty()) {
        salt = generateSalt();
    }

    std::vector<uint8_t> derivedKey;
    bool success = Crypto::PBKDF2_HMAC_SHA512(password, salt.data(), salt.size(),
                                             PBKDF2_ITERATIONS, derivedKey, 64);
    if (!success) {
        return Result<PasswordHashResult>("Failed to derive password hash", 500);
    }

    // Convert to base64 for storage
    std::string hash = Crypto::B64Encode(derivedKey);
    Crypto::SecureWipeVector(derivedKey);

    PasswordHashResult result;
    result.hash = hash;
    result.salt = salt;

    return Result<PasswordHashResult>(result);
}

Result<bool> UserRepository::verifyPassword(const std::string& password, const std::string& storedHash, const std::vector<uint8_t>& salt) {
    auto hashResult = hashPassword(password, salt);
    if (!hashResult) {
        return Result<bool>(hashResult.error(), hashResult.errorCode);
    }

    bool isValid = Crypto::ConstantTimeEquals(
        std::vector<uint8_t>(hashResult->hash.begin(), hashResult->hash.end()),
        std::vector<uint8_t>(storedHash.begin(), storedHash.end())
    );

    return Result<bool>(isValid);
}

User UserRepository::mapRowToUser(sqlite3_stmt* stmt) {
    User user;
    user.id = sqlite3_column_int(stmt, 0);
    user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    user.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    user.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    // Extract salt blob
    const void* saltData = sqlite3_column_blob(stmt, 4);
    int saltSize = sqlite3_column_bytes(stmt, 4);
    if (saltData && saltSize > 0) {
        user.salt.assign(static_cast<const uint8_t*>(saltData),
                        static_cast<const uint8_t*>(saltData) + saltSize);
    }

    // Parse timestamps (SQLite stores as strings)
    const char* createdAtStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (createdAtStr) {
        // For simplicity, using current time. In production, would parse ISO string
        user.createdAt = std::chrono::system_clock::now();
    }

    const char* lastLoginStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    if (lastLoginStr && strlen(lastLoginStr) > 0) {
        user.lastLogin = std::chrono::system_clock::now();
    }

    user.walletVersion = sqlite3_column_int(stmt, 7);
    user.isActive = sqlite3_column_int(stmt, 8) != 0;

    return user;
}

Result<bool> UserRepository::validateUsername(const std::string& username) {
    if (username.length() < MIN_USERNAME_LENGTH) {
        return Result<bool>("Username must be at least " + std::to_string(MIN_USERNAME_LENGTH) + " characters", 400);
    }

    if (username.length() > MAX_USERNAME_LENGTH) {
        return Result<bool>("Username must be no more than " + std::to_string(MAX_USERNAME_LENGTH) + " characters", 400);
    }

    // Check for valid characters (alphanumeric, underscore, hyphen)
    std::regex usernameRegex("^[a-zA-Z0-9_-]+$");
    if (!std::regex_match(username, usernameRegex)) {
        return Result<bool>("Username can only contain letters, numbers, underscores, and hyphens", 400);
    }

    return Result<bool>(true);
}

std::vector<uint8_t> UserRepository::generateSalt() {
    std::vector<uint8_t> salt(SALT_SIZE);
    Crypto::RandBytes(salt.data(), salt.size());
    return salt;
}

Result<PasswordValidation> UserRepository::validatePassword(const std::string& password) {
    PasswordValidation validation;

    validation.requirements = {
        "At least " + std::to_string(MIN_PASSWORD_LENGTH) + " characters",
        "At least one uppercase letter",
        "At least one lowercase letter",
        "At least one digit",
        "At least one special character (!@#$%^&*)"
    };

    int score = 0;

    // Check length
    if (password.length() >= MIN_PASSWORD_LENGTH) {
        score += 20;
    } else {
        validation.violations.push_back("Password must be at least " + std::to_string(MIN_PASSWORD_LENGTH) + " characters");
    }

    // Check for uppercase
    if (std::any_of(password.begin(), password.end(), [](char c) { return std::isupper(c); })) {
        score += 20;
    } else {
        validation.violations.push_back("Password must contain at least one uppercase letter");
    }

    // Check for lowercase
    if (std::any_of(password.begin(), password.end(), [](char c) { return std::islower(c); })) {
        score += 20;
    } else {
        validation.violations.push_back("Password must contain at least one lowercase letter");
    }

    // Check for digit
    if (std::any_of(password.begin(), password.end(), [](char c) { return std::isdigit(c); })) {
        score += 20;
    } else {
        validation.violations.push_back("Password must contain at least one digit");
    }

    // Check for special character
    const std::string specialChars = "!@#$%^&*()_+-=[]{}|;:,.<>?";
    if (std::any_of(password.begin(), password.end(), [&](char c) {
        return specialChars.find(c) != std::string::npos;
    })) {
        score += 20;
    } else {
        validation.violations.push_back("Password must contain at least one special character");
    }

    validation.strengthScore = score;
    validation.isValid = validation.violations.empty();

    return Result<PasswordValidation>(validation);
}

} // namespace Repository

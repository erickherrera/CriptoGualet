#pragma once

#include "../../core/include/Crypto.h"
#include "../../database/include/Database/DatabaseManager.h"
#include "Logger.h"
#include "RepositoryTypes.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
struct sqlite3_stmt;

namespace Repository {

/**
 * @brief Repository for user-related database operations
 *
 * Replaces the global g_users map with proper database storage and provides:
 * - Secure user authentication with salted password hashing
 * - User creation and management
 * - Session management
 * - Password security validation
 */
class UserRepository {
  public:
    /**
     * @brief Constructor
     * @param dbManager Reference to the database manager
     */
    explicit UserRepository(Database::DatabaseManager& dbManager);

    /**
     * @brief Create a new user account
     * @param username Unique username (minimum 3 characters)
     * @param password User password (will be hashed with salt)
     * @return Result containing the created user or error information
     */
    Result<User> createUser(const std::string& username, const std::string& password);

    /**
     * @brief Authenticate a user with username and password
     * @param username Username to authenticate
     * @param password Password to verify
     * @return Result containing the authenticated user or error information
     */
    Result<User> authenticateUser(const std::string& username, const std::string& password);

    /**
     * @brief Get user by username
     * @param username Username to search for
     * @return Result containing the user or error information
     */
    Result<User> getUserByUsername(const std::string& username);

    /**
     * @brief Get user by ID
     * @param userId User ID to search for
     * @return Result containing the user or error information
     */
    Result<User> getUserById(int userId);

    /**
     * @brief Update user's last login timestamp
     * @param userId User ID to update
     * @return Result indicating success or failure
     */
    Result<bool> updateLastLogin(int userId);

    /**
     * @brief Change user password
     * @param userId User ID
     * @param currentPassword Current password for verification
     * @param newPassword New password to set
     * @return Result indicating success or failure
     */
    Result<bool> changePassword(int userId, const std::string& currentPassword,
                                const std::string& newPassword);

    /**
     * @brief Update user activity status
     * @param userId User ID to update
     * @param isActive New activity status
     * @return Result indicating success or failure
     */
    Result<bool> setUserActive(int userId, bool isActive);

    /**
     * @brief Get all users (admin function)
     * @param params Pagination parameters
     * @return Result containing paginated user list
     */
    Result<PaginatedResult<User>> getAllUsers(const PaginationParams& params = PaginationParams());

    /**
     * @brief Check if username is available
     * @param username Username to check
     * @return Result indicating if username is available (true = available, false = taken)
     */
    Result<bool> isUsernameAvailable(const std::string& username);

    /**
     * @brief Delete user account (soft delete - marks as inactive)
     * @param userId
     * User ID to delete
     * @return Result indicating success or failure
     */
    Result<bool> deleteUser(int userId);

    /**
     * @brief Update encrypted seed for a user (Linux: stores in SQLCipher DB)
     * @param
     * userId User ID to update
     * @param encryptedSeedHex Hex-encoded encrypted seed
     *
     * @return Result indicating success or failure
     */
    Result<bool> updateEncryptedSeed(int userId, const std::string& encryptedSeedHex);

    /**
     * @brief Get encrypted seed for a user (Linux: retrieves from SQLCipher DB)
     *
     * @param userId User ID to query
     * @return Result containing hex-encoded encrypted seed
     * (empty if not set)
     */
    Result<std::string> getEncryptedSeed(int userId);

    /**
     * @brief Get user statistics
     * @return Result containing user count statistics
     */
    Result<UserStats> getUserStats();

    /**
     * @brief Validate password strength
     * @param password Password to validate
     * @return Result containing validation result and requirements
     */
    static Result<PasswordValidation> validatePassword(const std::string& password);

  private:
    /**
     * @brief Hash password with salt using secure algorithms
     * @param password Plain text password
     * @param salt Salt bytes (will be generated if empty)
     * @return Result containing password hash and salt
     */
    Result<PasswordHashResult> hashPassword(const std::string& password,
                                            std::vector<uint8_t> salt = {});

    /**
     * @brief Verify password against stored hash and salt
     * @param password Plain text password
     * @param storedHash Stored password hash
     * @param salt Salt used for hashing
     * @return Result indicating if password is correct
     */
    Result<bool> verifyPassword(const std::string& password, const std::string& storedHash,
                                const std::vector<uint8_t>& salt);

    /**
     * @brief Convert database row to User object
     * @param stmt SQLite statement handle
     * @return User object populated from database row
     */
    User mapRowToUser(sqlite3_stmt* stmt);

    /**
     * @brief Validate username format and requirements
     * @param username Username to validate
     * @return Result indicating if username is valid
     */
    Result<bool> validateUsername(const std::string& username);

    /**
     * @brief Generate secure random salt
     * @return Vector containing random salt bytes
     */
    std::vector<uint8_t> generateSalt();

  private:
    Database::DatabaseManager& m_dbManager;
    static constexpr const char* COMPONENT_NAME = "UserRepository";
    static constexpr int MIN_USERNAME_LENGTH = 3;
    static constexpr int MAX_USERNAME_LENGTH = 50;
    static constexpr int MIN_PASSWORD_LENGTH = 8;
    static constexpr int SALT_SIZE = 32;
    static constexpr int PBKDF2_ITERATIONS = 600000;  // Strong iteration count
};

}  // namespace Repository

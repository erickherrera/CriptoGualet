#pragma once
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

// Forward declarations from your project:
struct User;

namespace Auth {

enum class AuthResult {
  SUCCESS,
  USER_NOT_FOUND,
  INVALID_CREDENTIALS,
  USER_ALREADY_EXISTS,
  WEAK_PASSWORD,
  INVALID_USERNAME,
  RATE_LIMITED,
  SYSTEM_ERROR
};

struct AuthResponse {
  AuthResult result;
  std::string message;
  bool success() const { return result == AuthResult::SUCCESS; }
};

// Create a salted PBKDF2-HMAC-SHA256 password hash.
// Format: pbkdf2-sha256$<iterations>$<salt_b64>$<dk_b64>
std::string CreatePasswordHash(const std::string &password,
                               unsigned iterations = 100000);

// Verify a plaintext password against a stored hash in the format above.
bool VerifyPassword(const std::string &password, const std::string &stored);

// High-level user flows with detailed error messages:
AuthResponse RegisterUser(const std::string &username,
                          const std::string &password);
AuthResponse RegisterUser(const std::string &username,
                          const std::string &email,
                          const std::string &password);

// Extended registration that also returns the mnemonic for secure display
AuthResponse RegisterUserWithMnemonic(const std::string &username,
                                      const std::string &email,
                                      const std::string &password,
                                      std::vector<std::string> &outMnemonic);

// Returns detailed result of login attempt with rate limiting
AuthResponse LoginUser(const std::string &username,
                       const std::string &password);

// In namespace Auth:
AuthResponse RevealSeed(const std::string &username,
                        const std::string &password, std::string &outSeedHex,
                        std::optional<std::string> &outMnemonic);

AuthResponse RestoreFromSeed(const std::string &username,
                             const std::string &mnemonicText,
                             const std::string &passphrase,
                             const std::string &passwordForReauth);

// Rate limiting management
void ClearRateLimit(const std::string &identifier);
bool IsRateLimited(const std::string &identifier);

// Input validation helpers
bool IsValidUsername(const std::string &username);
bool IsValidPassword(const std::string &password);

// REMOVED: SaveUserDatabase() and LoadUserDatabase()
// These insecure functions have been removed. Use Repository layer instead:
// - UserRepository for user authentication data
// - WalletRepository for wallet addresses and encrypted seeds
// All data is now stored in SQLCipher encrypted database

// Initialize database and repository layer
// Should be called once at application startup
bool InitializeAuthDatabase();

// Derive a secure machine-specific encryption key for database encryption
// Uses machine identifiers (computer name, username, volume serial) with PBKDF2
// Returns true on success with 64-character hex key in outKey
bool DeriveSecureEncryptionKey(std::string &outKey);

// ===== Email Verification Functions =====

// Send verification code to user's email
// Generates a 6-digit code, stores it in database with expiration time, and sends email
// Returns SUCCESS if email was sent, SYSTEM_ERROR if email sending failed
AuthResponse SendVerificationCode(const std::string &username);

// Verify the code entered by the user
// Checks if code matches, is not expired, and hasn't exceeded attempt limit
// If successful, marks email as verified in database
AuthResponse VerifyEmailCode(const std::string &username,
                             const std::string &code);

// Resend verification code with rate limiting
// Enforces 60-second cooldown and maximum 3 resends per hour
// Returns RATE_LIMITED if cooldown/limit exceeded
AuthResponse ResendVerificationCode(const std::string &username);

// Check if user's email is verified (2FA enabled)
// Returns true if email_verified = 1 in database
bool IsEmailVerified(const std::string &username);

// Enable 2FA for a user (requires password verification)
// 1. Verifies user's password
// 2. Generates and sends verification code to email
// 3. Returns SUCCESS if code sent, INVALID_CREDENTIALS if password wrong
// User must then call VerifyEmailCode to complete 2FA enablement
AuthResponse EnableTwoFactorAuth(const std::string &username,
                                 const std::string &password);

// Disable 2FA for a user (requires password verification)
// 1. Verifies user's password
// 2. Sets email_verified = 0 in database
// Returns SUCCESS if disabled, INVALID_CREDENTIALS if password wrong
AuthResponse DisableTwoFactorAuth(const std::string &username,
                                  const std::string &password);

} // namespace Auth

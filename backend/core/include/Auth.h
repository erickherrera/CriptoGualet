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

// ===== TOTP Two-Factor Authentication =====
// Uses authenticator apps like Google Authenticator, Authy, Microsoft Authenticator

// Check if 2FA is enabled for a user
// Returns true if totp_enabled = 1 in database
bool IsTwoFactorEnabled(const std::string &username);

// Generate a new TOTP secret for enabling 2FA
// Returns the secret in base32 format (for display) and the otpauth:// URI (for QR code)
// Does NOT enable 2FA - user must verify a code first using ConfirmTwoFactorSetup
struct TwoFactorSetupData {
  std::string secretBase32;  // Secret in base32 format (for manual entry)
  std::string otpauthUri;    // otpauth:// URI for QR code generation
  bool success;
  std::string errorMessage;
};
TwoFactorSetupData InitiateTwoFactorSetup(const std::string &username,
                                           const std::string &password);

// Confirm 2FA setup by verifying a TOTP code
// This enables 2FA for the user if the code is valid
AuthResponse ConfirmTwoFactorSetup(const std::string &username,
                                    const std::string &totpCode);

// Verify a TOTP code for login (when 2FA is enabled)
// Returns SUCCESS if code is valid, INVALID_CREDENTIALS if code is wrong
AuthResponse VerifyTwoFactorCode(const std::string &username,
                                  const std::string &totpCode);

// Disable 2FA for a user (requires password and current TOTP code)
// 1. Verifies user's password
// 2. Verifies current TOTP code (proves user has authenticator)
// 3. Removes TOTP secret and disables 2FA
AuthResponse DisableTwoFactor(const std::string &username,
                               const std::string &password,
                               const std::string &totpCode);

// Get backup codes for 2FA recovery (generated when 2FA is enabled)
// These are one-time use codes that can be used if authenticator is lost
struct BackupCodesResult {
  std::vector<std::string> codes;
  bool success;
  std::string errorMessage;
};
BackupCodesResult GetBackupCodes(const std::string &username,
                                  const std::string &password);

// Use a backup code to disable 2FA (for account recovery)
AuthResponse UseBackupCode(const std::string &username,
                            const std::string &backupCode);

} // namespace Auth

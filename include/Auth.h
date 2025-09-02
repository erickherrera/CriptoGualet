#pragma once
#include <chrono>
#include <map>
#include <optional>
#include <string>

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

// Secure data persistence
void SaveUserDatabase();
void LoadUserDatabase();

} // namespace Auth

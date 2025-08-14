#pragma once
#include <string>

// Forward declarations from your project:
struct User;

namespace Auth {

// Create a salted PBKDF2-HMAC-SHA256 password hash.
// Format: pbkdf2-sha256$<iterations>$<salt_b64>$<dk_b64>
std::string CreatePasswordHash(const std::string& password, unsigned iterations = 100000);

// Verify a plaintext password against a stored hash in the format above.
bool VerifyPassword(const std::string& password, const std::string& stored);

// High-level user flows (operate on your global g_users; see Auth.cpp):
bool RegisterUser(const std::string& username, const std::string& password);

// Returns true if credentials are valid.
bool LoginUser(const std::string& username, const std::string& password);

// Secure data persistence
void SaveUserDatabase();
void LoadUserDatabase();

} // namespace Auth

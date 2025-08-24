// Windows headers need to be first with proper defines
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

// Standard library headers
#include <cstring>
#include <map>
#include <sstream>
#include <vector>
#include <fstream>

// Project headers after Windows headers
#include "../include/Auth.h"
#include "../include/CriptoGualet.h" // for User struct, GeneratePrivateKey, GenerateBitcoinAddress

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

// Use your existing globals (declared in CriptoGualet.cpp)
extern std::map<std::string, User> g_users;

// Rate limiting data structures
struct RateLimitEntry {
    int attemptCount = 0;
    std::chrono::steady_clock::time_point lastAttempt;
    std::chrono::steady_clock::time_point lockoutUntil;
};

static std::map<std::string, RateLimitEntry> g_rateLimits;

// ===== Helpers: RNG, base64, constant-time compare, PBKDF2 =====
namespace
{

inline bool RandBytes(void* buf, size_t len)
{
    return BCRYPT_SUCCESS(BCryptGenRandom(
        nullptr, static_cast<PUCHAR>(buf), static_cast<ULONG>(len),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

inline std::string B64Encode(const std::vector<uint8_t>& data)
{
    if (data.empty())
        return {};
    DWORD outLen = 0;
    if (!CryptBinaryToStringA(
            data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            nullptr, &outLen))
        return {};
    std::string out(outLen, '\0');
    if (!CryptBinaryToStringA(
            data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            &out[0], &outLen))
        return {};
    if (!out.empty() && out.back() == '\0')
        out.pop_back();
    return out;
}

inline std::vector<uint8_t> B64Decode(const std::string& s)
{
    if (s.empty())
        return {};
    DWORD outLen = 0;
    if (!CryptStringToBinaryA(
            s.c_str(), static_cast<DWORD>(s.size()), CRYPT_STRING_BASE64, nullptr, &outLen, nullptr,
            nullptr))
    {
        return {};
    }
    std::vector<uint8_t> out(outLen);
    if (!CryptStringToBinaryA(
            s.c_str(), static_cast<DWORD>(s.size()), CRYPT_STRING_BASE64, out.data(), &outLen,
            nullptr, nullptr))
    {
        return {};
    }
    return out;
}

inline bool constant_time_eq(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
        diff |= (a[i] ^ b[i]);
    return diff == 0;
}

// Manual PBKDF2-HMAC-SHA256 implementation for Windows compatibility
inline bool HMAC_SHA256(const std::vector<uint8_t>& key, const uint8_t* data, size_t data_len, std::vector<uint8_t>& out)
{
    out.assign(32, 0);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptHashData(hHash, (PUCHAR)data, (ULONG)data_len, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptFinishHash(hHash, out.data(), (ULONG)out.size(), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(st);
}

// PBKDF2-HMAC-SHA256 using manual implementation for compatibility
inline bool PBKDF2_HMAC_SHA256(
    const std::string& password, const uint8_t* salt, size_t salt_len, uint32_t iterations,
    std::vector<uint8_t>& out_key, size_t dk_len = 32)
{
    out_key.assign(dk_len, 0);
    
    const size_t hash_len = 32; // SHA256 output length
    const size_t blocks = (dk_len + hash_len - 1) / hash_len;
    
    std::vector<uint8_t> key(password.begin(), password.end());
    std::vector<uint8_t> block_input(salt_len + 4);
    std::memcpy(block_input.data(), salt, salt_len);
    
    for (size_t i = 0; i < blocks; ++i) {
        // Block number as big-endian 32-bit integer
        uint32_t block_num = static_cast<uint32_t>(i + 1);
        block_input[salt_len] = (block_num >> 24) & 0xFF;
        block_input[salt_len + 1] = (block_num >> 16) & 0xFF;
        block_input[salt_len + 2] = (block_num >> 8) & 0xFF;
        block_input[salt_len + 3] = block_num & 0xFF;
        
        std::vector<uint8_t> u, result(hash_len, 0);
        
        // First iteration
        if (!HMAC_SHA256(key, block_input.data(), block_input.size(), u))
            return false;
        
        for (size_t j = 0; j < hash_len; ++j)
            result[j] = u[j];
        
        // Remaining iterations
        for (uint32_t iter = 1; iter < iterations; ++iter) {
            if (!HMAC_SHA256(key, u.data(), u.size(), u))
                return false;
            for (size_t j = 0; j < hash_len; ++j)
                result[j] ^= u[j];
        }
        
        // Copy block result to output
        size_t copy_len = std::min(hash_len, dk_len - i * hash_len);
        std::memcpy(out_key.data() + i * hash_len, result.data(), copy_len);
    }
    
    return true;
}

} // namespace

// ===== Public API =====
namespace Auth
{

std::string CreatePasswordHash(const std::string& password, unsigned iterations)
{
    OutputDebugStringA("CreatePasswordHash: Starting...\n");
    
    // 16-byte random salt
    std::vector<uint8_t> salt(16);
    if (!RandBytes(salt.data(), salt.size())) {
        OutputDebugStringA("CreatePasswordHash: RandBytes failed\n");
        return {};
    }
    OutputDebugStringA("CreatePasswordHash: Salt generated\n");

    std::vector<uint8_t> dk;
    if (!PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(), iterations, dk, 32))
    {
        OutputDebugStringA("CreatePasswordHash: PBKDF2 failed\n");
        return {};
    }
    OutputDebugStringA("CreatePasswordHash: PBKDF2 completed\n");

    std::ostringstream fmt;
    std::string saltB64 = B64Encode(salt);
    std::string dkB64 = B64Encode(dk);
    
    if (saltB64.empty() || dkB64.empty()) {
        OutputDebugStringA("CreatePasswordHash: Base64 encoding failed\n");
        return {};
    }
    
    fmt << "pbkdf2-sha256$" << iterations << "$" << saltB64 << "$" << dkB64;
    OutputDebugStringA("CreatePasswordHash: Success\n");
    return fmt.str();
}

bool VerifyPassword(const std::string& password, const std::string& stored)
{
    // Parse "pbkdf2-sha256$iter$salt_b64$dk_b64"
    const std::string prefix = "pbkdf2-sha256$";
    if (stored.rfind(prefix, 0) != 0)
        return false;

    size_t p = prefix.size();
    size_t p2 = stored.find('$', p);
    if (p2 == std::string::npos)
        return false;
    unsigned iterations = std::stoul(stored.substr(p, p2 - p));

    size_t p3 = stored.find('$', p2 + 1);
    if (p3 == std::string::npos)
        return false;
    std::string salt_b64 = stored.substr(p2 + 1, p3 - (p2 + 1));
    std::string dk_b64 = stored.substr(p3 + 1);

    auto salt = B64Decode(salt_b64);
    auto dk = B64Decode(dk_b64);
    if (salt.empty() || dk.empty())
        return false;

    std::vector<uint8_t> test;
    if (!PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(), iterations, test, dk.size()))
    {
        return false;
    }
    return constant_time_eq(test, dk);
}

// Rate limiting constants
const int MAX_LOGIN_ATTEMPTS = 5;
const std::chrono::minutes LOCKOUT_DURATION{10};
const std::chrono::minutes RATE_LIMIT_WINDOW{1};

bool IsValidUsername(const std::string& username)
{
    if (username.size() < 3 || username.size() > 50) return false;
    
    for (char c : username) {
        if (!std::isalnum(c) && c != '_' && c != '-') return false;
    }
    return true;
}

bool IsValidPassword(const std::string& password)
{
    if (password.size() < 8 || password.size() > 128) return false;
    
    bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;
    
    for (char c : password) {
        if (std::isupper(c)) hasUpper = true;
        else if (std::islower(c)) hasLower = true;
        else if (std::isdigit(c)) hasDigit = true;
        else if (std::ispunct(c) || c == ' ') hasSpecial = true;
    }
    
    return hasUpper && hasLower && hasDigit && hasSpecial;
}

bool IsRateLimited(const std::string& identifier)
{
    auto now = std::chrono::steady_clock::now();
    auto it = g_rateLimits.find(identifier);
    
    if (it == g_rateLimits.end()) return false;
    
    RateLimitEntry& entry = it->second;
    
    // Check if still in lockout period
    if (entry.lockoutUntil > now) return true;
    
    // Reset if rate limit window has expired
    if (now - entry.lastAttempt > RATE_LIMIT_WINDOW) {
        entry.attemptCount = 0;
    }
    
    return false;
}

void ClearRateLimit(const std::string& identifier)
{
    g_rateLimits.erase(identifier);
}

static void RecordFailedAttempt(const std::string& identifier)
{
    auto now = std::chrono::steady_clock::now();
    auto& entry = g_rateLimits[identifier];
    
    // Reset counter if rate limit window expired
    if (now - entry.lastAttempt > RATE_LIMIT_WINDOW) {
        entry.attemptCount = 0;
    }
    
    entry.attemptCount++;
    entry.lastAttempt = now;
    
    // Set lockout if max attempts exceeded
    if (entry.attemptCount >= MAX_LOGIN_ATTEMPTS) {
        entry.lockoutUntil = now + LOCKOUT_DURATION;
    }
}

AuthResponse RegisterUser(const std::string& username, const std::string& password)
{
    OutputDebugStringA("RegisterUser: Starting registration process\n");
    
    // Validate username
    if (!IsValidUsername(username)) {
        return {AuthResult::INVALID_USERNAME, 
                "Username must be 3-50 characters and contain only letters, numbers, underscore, or dash."};
    }
    
    // Validate password strength
    if (!IsValidPassword(password)) {
        return {AuthResult::WEAK_PASSWORD,
                "Password must be 8-128 characters with uppercase, lowercase, digit, and special character."};
    }
    
    // Check if user already exists
    if (g_users.find(username) != g_users.end()) {
        std::string msg = "RegisterUser: Username '" + username + "' already exists\n";
        OutputDebugStringA(msg.c_str());
        return {AuthResult::USER_ALREADY_EXISTS, 
                "Username already exists. Please choose a different username."};
    }

    User u;
    u.username = username;
    
    // Create secure password hash
    OutputDebugStringA("RegisterUser: Creating password hash...\n");
    u.passwordHash = CreatePasswordHash(password);
    if (u.passwordHash.empty()) {
        OutputDebugStringA("RegisterUser: Password hash creation failed\n");
        return {AuthResult::SYSTEM_ERROR, 
                "System error occurred during registration. Please try again."};
    }
    OutputDebugStringA("RegisterUser: Password hash created successfully\n");

    // Generate wallet credentials
    u.privateKey = GeneratePrivateKey();
    u.walletAddress = GenerateBitcoinAddress();

    g_users[username] = std::move(u);
    OutputDebugStringA("RegisterUser: User registered successfully\n");
    
    SaveUserDatabase(); // Persist immediately
    
    return {AuthResult::SUCCESS, "Account created successfully. You can now sign in."};
}

AuthResponse LoginUser(const std::string& username, const std::string& password)
{
    // Check rate limiting first
    if (IsRateLimited(username)) {
        return {AuthResult::RATE_LIMITED,
                "Too many failed attempts. Please wait 10 minutes before trying again."};
    }
    
    // Basic input validation
    if (username.empty() || password.empty()) {
        RecordFailedAttempt(username);
        return {AuthResult::INVALID_CREDENTIALS, "Please enter both username and password."};
    }
    
    // Find user
    auto it = g_users.find(username);
    if (it == g_users.end()) {
        RecordFailedAttempt(username);
        return {AuthResult::USER_NOT_FOUND, 
                "User not found. Please create an account before signing in."};
    }
    
    // Verify password
    if (!VerifyPassword(password, it->second.passwordHash)) {
        RecordFailedAttempt(username);
        int remainingAttempts = MAX_LOGIN_ATTEMPTS - (g_rateLimits[username].attemptCount + 1);
        if (remainingAttempts > 0) {
            return {AuthResult::INVALID_CREDENTIALS,
                    "Invalid credentials. " + std::to_string(remainingAttempts) + " attempts remaining."};
        } else {
            return {AuthResult::RATE_LIMITED,
                    "Invalid credentials. Account temporarily locked for 10 minutes."};
        }
    }
    
    // Success - clear any rate limiting
    ClearRateLimit(username);
    return {AuthResult::SUCCESS, "Login successful. Welcome to CriptoGualet!"};
}

void SaveUserDatabase()
{
    try {
        std::ofstream file("secure_wallet.db", std::ios::binary);
        if (!file.is_open()) {
            return;
        }

        // Save number of users
        size_t userCount = g_users.size();
        file.write(reinterpret_cast<const char*>(&userCount), sizeof(userCount));

        for (const auto& pair : g_users) {
            const User& user = pair.second;

            // Save username length and username
            size_t usernameLen = user.username.length();
            file.write(reinterpret_cast<const char*>(&usernameLen), sizeof(usernameLen));
            file.write(user.username.c_str(), static_cast<std::streamsize>(usernameLen));

            // Save password hash length and hash (already secure PBKDF2)
            size_t hashLen = user.passwordHash.length();
            file.write(reinterpret_cast<const char*>(&hashLen), sizeof(hashLen));
            file.write(user.passwordHash.c_str(), static_cast<std::streamsize>(hashLen));

            // Save wallet address length and address
            size_t addrLen = user.walletAddress.length();
            file.write(reinterpret_cast<const char*>(&addrLen), sizeof(addrLen));
            file.write(user.walletAddress.c_str(), static_cast<std::streamsize>(addrLen));

            // Save private key length and key
            size_t keyLen = user.privateKey.length();
            file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
            file.write(user.privateKey.c_str(), static_cast<std::streamsize>(keyLen));
        }
        file.close();
    }
    catch (...) {
        // Production should log errors properly
    }
}

void LoadUserDatabase()
{
    try {
        std::ifstream file("secure_wallet.db", std::ios::binary);
        if (!file.is_open()) {
            return;
        }

        g_users.clear();

        // Read number of users
        size_t userCount = 0;
        file.read(reinterpret_cast<char*>(&userCount), sizeof(userCount));

        for (size_t i = 0; i < userCount; ++i) {
            User user;

            // Read username
            size_t usernameLen = 0;
            file.read(reinterpret_cast<char*>(&usernameLen), sizeof(usernameLen));
            if (usernameLen > 0) {
                std::vector<char> buffer(usernameLen);
                file.read(buffer.data(), static_cast<std::streamsize>(usernameLen));
                user.username.assign(buffer.begin(), buffer.end());
            }

            // Read password hash
            size_t hashLen = 0;
            file.read(reinterpret_cast<char*>(&hashLen), sizeof(hashLen));
            if (hashLen > 0) {
                std::vector<char> buffer(hashLen);
                file.read(buffer.data(), static_cast<std::streamsize>(hashLen));
                user.passwordHash.assign(buffer.begin(), buffer.end());
            }

            // Read wallet address
            size_t addrLen = 0;
            file.read(reinterpret_cast<char*>(&addrLen), sizeof(addrLen));
            if (addrLen > 0) {
                std::vector<char> buffer(addrLen);
                file.read(buffer.data(), static_cast<std::streamsize>(addrLen));
                user.walletAddress.assign(buffer.begin(), buffer.end());
            }

            // Read private key
            size_t keyLen = 0;
            file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
            if (keyLen > 0) {
                std::vector<char> buffer(keyLen);
                file.read(buffer.data(), static_cast<std::streamsize>(keyLen));
                user.privateKey.assign(buffer.begin(), buffer.end());
            }

            g_users[user.username] = std::move(user);
        }
        file.close();
    }
    catch (...) {
        // Production should log errors properly
    }
}

} // namespace Auth

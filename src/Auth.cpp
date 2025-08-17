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

bool RegisterUser(const std::string& username, const std::string& password)
{
    // Debug: Check input validation
    if (username.size() < 3 || password.size() < 6) {
        OutputDebugStringA("RegisterUser: Input validation failed\n");
        return false;
    }
    
    // Debug: Show current users in map
    std::string debugMsg = "RegisterUser: Current users in map (" + std::to_string(g_users.size()) + "): ";
    for (const auto& pair : g_users) {
        debugMsg += "[" + pair.first + "] ";
    }
    debugMsg += "\n";
    OutputDebugStringA(debugMsg.c_str());
    
    // Debug: Check if username already exists
    if (g_users.find(username) != g_users.end()) {
        std::string msg = "RegisterUser: Username '" + username + "' already exists\n";
        OutputDebugStringA(msg.c_str());
        return false;
    }

    User u;
    u.username = username;
    
    // Debug: Test password hashing
    OutputDebugStringA("RegisterUser: Creating password hash...\n");
    u.passwordHash = CreatePasswordHash(password); // salted PBKDF2
    if (u.passwordHash.empty()) {
        OutputDebugStringA("RegisterUser: Password hash creation failed\n");
        return false;
    }
    OutputDebugStringA("RegisterUser: Password hash created successfully\n");

    // Use your existing demo helpers:
    u.privateKey = GeneratePrivateKey();
    u.walletAddress = GenerateBitcoinAddress();

    g_users[username] = std::move(u);
    OutputDebugStringA("RegisterUser: User registered successfully\n");
    return true;
}

bool LoginUser(const std::string& username, const std::string& password)
{
    auto it = g_users.find(username);
    if (it == g_users.end())
        return false;
    return VerifyPassword(password, it->second.passwordHash);
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

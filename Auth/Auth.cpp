#include "../Auth/Auth.h"

// Project headers
#include "../CriptoGualet.h" // for User struct, GeneratePrivateKey, GenerateBitcoinAddress

// Windows/CNG/Crypt32
#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>
#include <cstring>
#include <map>
#include <sstream>
#include <vector>
#include <wincrypt.h>

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

// PBKDF2-HMAC-SHA256 using CNG
inline bool PBKDF2_HMAC_SHA256(
    const std::string& password, const uint8_t* salt, size_t salt_len, uint32_t iterations,
    std::vector<uint8_t>& out_key, size_t dk_len = 32)
{
    out_key.assign(dk_len, 0);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;

    // CNG PBKDF2 via BCryptDeriveKeyPBKDF2 (available on Win 10+)
    st = BCryptDeriveKeyPBKDF2(
        hAlg, (PUCHAR)password.data(), (ULONG)password.size(), (PUCHAR)salt, (ULONG)salt_len,
        iterations, (PUCHAR)out_key.data(), (ULONG)out_key.size(), 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(st);
}

} // namespace

// ===== Public API =====
namespace Auth
{

std::string CreatePasswordHash(const std::string& password, unsigned iterations)
{
    // 16-byte random salt
    std::vector<uint8_t> salt(16);
    if (!RandBytes(salt.data(), salt.size()))
        return {};

    std::vector<uint8_t> dk;
    if (!PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(), iterations, dk, 32))
    {
        return {};
    }

    std::ostringstream fmt;
    fmt << "pbkdf2-sha256$" << iterations << "$" << B64Encode(salt) << "$" << B64Encode(dk);
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
    if (username.size() < 3 || password.size() < 6)
        return false;
    if (g_users.find(username) != g_users.end())
        return false;

    User u;
    u.username = username;
    u.passwordHash = CreatePasswordHash(password); // salted PBKDF2
    if (u.passwordHash.empty())
        return false;

    // Use your existing demo helpers:
    u.privateKey = GeneratePrivateKey();
    u.walletAddress = GenerateBitcoinAddress();

    g_users[username] = std::move(u);
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
            user.username.resize(usernameLen);
            file.read(&user.username[0], static_cast<std::streamsize>(usernameLen));

            // Read password hash
            size_t hashLen = 0;
            file.read(reinterpret_cast<char*>(&hashLen), sizeof(hashLen));
            user.passwordHash.resize(hashLen);
            file.read(&user.passwordHash[0], static_cast<std::streamsize>(hashLen));

            // Read wallet address
            size_t addrLen = 0;
            file.read(reinterpret_cast<char*>(&addrLen), sizeof(addrLen));
            user.walletAddress.resize(addrLen);
            file.read(&user.walletAddress[0], static_cast<std::streamsize>(addrLen));

            // Read private key
            size_t keyLen = 0;
            file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
            user.privateKey.resize(keyLen);
            file.read(&user.privateKey[0], static_cast<std::streamsize>(keyLen));

            g_users[user.username] = std::move(user);
        }
        file.close();
    }
    catch (...) {
        // Production should log errors properly
    }
}

} // namespace Auth

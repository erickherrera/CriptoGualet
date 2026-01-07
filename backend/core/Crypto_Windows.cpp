// Crypto_Windows.cpp - Windows-specific cryptographic implementations
// Uses BCrypt (Windows CNG) and DPAPI

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include "include/Crypto_Platform.h"

#include <algorithm>
#include <cstring>
#include <vector>

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

namespace Crypto {
namespace Platform {

// === Random Number Generation ===
bool RandBytes(void* buf, size_t len) {
    return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf),
                                          static_cast<ULONG>(len),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

// === SHA256 Hash ===
bool SHA256(const uint8_t* data, size_t len, uint8_t* out) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD objLen = 0;
    DWORD dataLen = 0;

    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st =
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &dataLen, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    std::vector<uint8_t> obj(objLen);
    st = BCryptCreateHash(hAlg, &hHash, obj.data(), objLen, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptFinishHash(hHash, out, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BCRYPT_SUCCESS(st);
}

// === HMAC-SHA256 ===
bool HMAC_SHA256(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
                 uint8_t* out) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                              BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key, (ULONG)key_len, 0);
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

    st = BCryptFinishHash(hHash, out, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BCRYPT_SUCCESS(st);
}

// === HMAC-SHA512 ===
bool HMAC_SHA512(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
                 uint8_t* out) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA512_ALGORITHM, nullptr,
                                              BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key, (ULONG)key_len, 0);
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

    st = BCryptFinishHash(hHash, out, 64, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BCRYPT_SUCCESS(st);
}

// === HMAC-SHA1 (for TOTP) ===
bool HMAC_SHA1(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
               uint8_t* out) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr,
                                              BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key, (ULONG)key_len, 0);
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

    st = BCryptFinishHash(hHash, out, 20, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BCRYPT_SUCCESS(st);
}

// === AES-GCM Encryption ===
bool AES_GCM_Encrypt(const uint8_t* key, size_t key_len, const uint8_t* plaintext,
                     size_t plaintext_len, const uint8_t* aad, size_t aad_len, uint8_t* iv,
                     size_t iv_len, uint8_t* ciphertext, uint8_t* tag, size_t tag_len) {
    if (key_len != 32 || iv_len != 12 || tag_len != 16)
        return false;

    // Generate random IV
    if (!RandBytes(iv, iv_len))
        return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key, (ULONG)key_len, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv;
    authInfo.cbNonce = (ULONG)iv_len;
    authInfo.pbAuthData = (PUCHAR)aad;
    authInfo.cbAuthData = (ULONG)aad_len;
    authInfo.pbTag = tag;
    authInfo.cbTag = (ULONG)tag_len;

    ULONG cbResult = 0;
    st = BCryptEncrypt(hKey, (PUCHAR)plaintext, (ULONG)plaintext_len, &authInfo, nullptr, 0,
                       ciphertext, (ULONG)plaintext_len, &cbResult, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BCRYPT_SUCCESS(st);
}

// === AES-GCM Decryption ===
bool AES_GCM_Decrypt(const uint8_t* key, size_t key_len, const uint8_t* ciphertext,
                     size_t ciphertext_len, const uint8_t* aad, size_t aad_len, const uint8_t* iv,
                     size_t iv_len, const uint8_t* tag, size_t tag_len, uint8_t* plaintext) {
    if (key_len != 32 || iv_len != 12 || tag_len != 16)
        return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key, (ULONG)key_len, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = (PUCHAR)iv;
    authInfo.cbNonce = (ULONG)iv_len;
    authInfo.pbAuthData = (PUCHAR)aad;
    authInfo.cbAuthData = (ULONG)aad_len;
    authInfo.pbTag = (PUCHAR)tag;
    authInfo.cbTag = (ULONG)tag_len;

    ULONG cbResult = 0;
    st = BCryptDecrypt(hKey, (PUCHAR)ciphertext, (ULONG)ciphertext_len, &authInfo, nullptr, 0,
                       plaintext, (ULONG)ciphertext_len, &cbResult, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BCRYPT_SUCCESS(st);
}

// === DPAPI Secure Storage ===
bool SecureProtect(const std::vector<uint8_t>& plaintext, const std::string& identifier,
                   std::vector<uint8_t>& ciphertext) {
    DATA_BLOB in{}, out{}, entropy{};
    in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
    in.cbData = static_cast<DWORD>(plaintext.size());
    entropy.pbData = (BYTE*)identifier.data();
    entropy.cbData = (DWORD)identifier.size();

    if (!CryptProtectData(&in, L"CriptoGualet", &entropy, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return false;
    }

    ciphertext.assign(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return true;
}

bool SecureUnprotect(const std::vector<uint8_t>& ciphertext, const std::string& identifier,
                     std::vector<uint8_t>& plaintext) {
    DATA_BLOB in{}, out{}, entropy{};
    in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(ciphertext.data()));
    in.cbData = static_cast<DWORD>(ciphertext.size());
    entropy.pbData = (BYTE*)identifier.data();
    entropy.cbData = (DWORD)identifier.size();

    if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                            &out)) {
        return false;
    }

    plaintext.assign(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return true;
}

bool SecureDelete(const std::string& identifier) {
    // DPAPI doesn't store data in a central location - it just encrypts/decrypts
    // The caller is responsible for deleting the encrypted blob from wherever it's stored
    // This function is a no-op on Windows as DPAPI doesn't maintain a registry
    (void)identifier;
    return true;
}

bool SecureExists(const std::string& identifier) {
    // DPAPI doesn't maintain a registry of protected data
    // The caller must track what data has been protected
    (void)identifier;
    return false;  // Cannot determine without external storage
}

}  // namespace Platform
}  // namespace Crypto

#endif  // _WIN32

// Windows headers need to be first with proper defines
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif

#ifndef _WIN32
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sodium.h>
#include <unistd.h>
#endif

// Standard library headers
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Third-party headers
#include <secp256k1.h>
#include <secp256k1_recovery.h>

// Project headers
#include "Crypto.h"

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

namespace Crypto {

// Global secp256k1 context (initialized once)
static secp256k1_context* GetSecp256k1Context() {
    static secp256k1_context* ctx =
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    return ctx;
}

// === Random Number Generation ===
bool RandBytes(void* buf, size_t len) {
#ifdef _WIN32
    return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf),
                                          static_cast<ULONG>(len),
                                          BCRYPT_USE_SYSTEM_PREFERRED_RNG));
#else
    return RAND_bytes(static_cast<unsigned char*>(buf), static_cast<int>(len)) == 1;
#endif
}

// === Base64 Encoding/Decoding ===
std::string B64Encode(const std::vector<uint8_t>& data) {
    if (data.empty())
        return {};
#ifdef _WIN32
    DWORD outLen = 0;
    if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen))
        return {};
    std::string out(outLen, '\0');
    if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen))
        return {};
    if (!out.empty() && out.back() == '\0')
        out.pop_back();
    return out;
#else
    int outLen = 4 * ((data.size() + 2) / 3);
    std::string out(outLen, '\0');
    int ret = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&out[0]), data.data(), data.size());
    if (ret < 0)
        return {};
    // EVP_EncodeBlock returns length including null terminator if it added one,
    // but we managed string size ourselves.
    out.resize(ret);
    return out;
#endif
}

std::vector<uint8_t> B64Decode(const std::string& s) {
    if (s.empty())
        return {};
#ifdef _WIN32
    DWORD outLen = 0;
    if (!CryptStringToBinaryA(s.c_str(), static_cast<DWORD>(s.size()), CRYPT_STRING_BASE64, nullptr,
                              &outLen, nullptr, nullptr))
        return {};
    std::vector<uint8_t> out(outLen);
    if (!CryptStringToBinaryA(s.c_str(), static_cast<DWORD>(s.size()), CRYPT_STRING_BASE64,
                              out.data(), &outLen, nullptr, nullptr))
        return {};
    return out;
#else
    int outLen = 3 * (s.size() / 4);
    std::vector<uint8_t> out(outLen);
    int ret =
        EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(s.c_str()), s.size());
    if (ret < 0)
        return {};

    // Padding adjustment
    int padding = 0;
    if (s.size() > 0 && s[s.size() - 1] == '=')
        padding++;
    if (s.size() > 1 && s[s.size() - 2] == '=')
        padding++;
    out.resize(ret - padding);
    return out;
#endif
}

// === Hash Functions ===
bool SHA256(const uint8_t* data, size_t len, std::array<uint8_t, 32>& out) {
    out.fill(uint8_t(0));
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD hashLen = 0, objLen = 0;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;

    st =
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &hashLen, 0);
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

    st = BCryptFinishHash(hHash, out.data(), (ULONG)out.size(), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(st);
#else
    unsigned int hashLen = 0;
    if (EVP_Digest(data, len, out.data(), &hashLen, EVP_sha256(), nullptr) != 1) {
        return false;
    }
    return true;
#endif
}

// === RIPEMD-160 Implementation ===
// Windows BCrypt doesn't support RIPEMD-160, so we implement it manually
// This is required for proper Bitcoin address generation (Hash160 = RIPEMD160(SHA256(data)))

static uint32_t RIPEMD160_F(uint32_t x, uint32_t y, uint32_t z, int round) {
    if (round < 16)
        return x ^ y ^ z;
    if (round < 32)
        return (x & y) | (~x & z);
    if (round < 48)
        return (x | ~y) ^ z;
    if (round < 64)
        return (x & z) | (y & ~z);
    return x ^ (y | ~z);
}

static uint32_t RIPEMD160_ROL(uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

bool RIPEMD160(const uint8_t* data, size_t len, std::array<uint8_t, 20>& out) {
    // RIPEMD-160 constants
    static const uint32_t KL[5] = {0x00000000, 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xA953FD4E};
    static const uint32_t KR[5] = {0x50A28BE6, 0x5C4DD124, 0x6D703EF3, 0x7A6D76E9, 0x00000000};

    static const int RL[80] = {0, 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
                               7, 4,  13, 1,  10, 6,  15, 3,  12, 0, 9,  5,  2,  14, 11, 8,
                               3, 10, 14, 4,  9,  15, 8,  1,  2,  7, 0,  6,  13, 11, 5,  12,
                               1, 9,  11, 10, 0,  8,  12, 4,  13, 3, 7,  15, 14, 5,  6,  2,
                               4, 0,  5,  9,  7,  12, 2,  10, 14, 1, 3,  8,  11, 6,  15, 13};

    static const int RR[80] = {5,  14, 7,  0, 9, 2,  11, 4,  13, 6,  15, 8,  1,  10, 3,  12,
                               6,  11, 3,  7, 0, 13, 5,  10, 14, 15, 8,  12, 4,  9,  1,  2,
                               15, 5,  1,  3, 7, 14, 6,  9,  11, 8,  12, 2,  10, 0,  4,  13,
                               8,  6,  4,  1, 3, 11, 15, 0,  5,  12, 2,  13, 9,  7,  10, 14,
                               12, 15, 10, 4, 1, 5,  8,  7,  6,  2,  13, 14, 0,  3,  9,  11};

    static const int SL[80] = {11, 14, 15, 12, 5,  8,  7,  9,  11, 13, 14, 15, 6,  7,  9,  8,
                               7,  6,  8,  13, 11, 9,  7,  15, 7,  12, 15, 9,  11, 7,  13, 12,
                               11, 13, 6,  7,  14, 9,  13, 15, 14, 8,  13, 6,  5,  12, 7,  5,
                               11, 12, 14, 15, 14, 15, 9,  8,  9,  14, 5,  6,  8,  6,  5,  12,
                               9,  15, 5,  11, 6,  8,  13, 12, 5,  12, 13, 14, 11, 8,  5,  6};

    static const int SR[80] = {8,  9,  9,  11, 13, 15, 15, 5,  7,  7,  8,  11, 14, 14, 12, 6,
                               9,  13, 15, 7,  12, 8,  9,  11, 7,  7,  12, 7,  6,  15, 13, 11,
                               9,  7,  15, 11, 8,  6,  6,  14, 12, 13, 5,  14, 13, 13, 7,  5,
                               15, 5,  8,  11, 14, 14, 6,  14, 6,  9,  12, 9,  12, 5,  15, 8,
                               8,  5,  12, 9,  12, 5,  14, 6,  8,  13, 6,  5,  15, 13, 11, 11};

    // Initialize state
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    // Prepare message with padding
    uint64_t bit_len = len * 8;
    size_t new_len = len + 1;
    while (new_len % 64 != 56)
        new_len++;

    std::vector<uint8_t> padded(new_len + 8);
    std::memcpy(padded.data(), data, len);
    padded[len] = 0x80;

    // Append length in little-endian
    for (int i = 0; i < 8; i++) {
        padded[new_len + i] = (bit_len >> (i * 8)) & 0xFF;
    }

    // Process blocks
    for (size_t block = 0; block < padded.size(); block += 64) {
        uint32_t X[16];
        for (int i = 0; i < 16; i++) {
            X[i] = static_cast<uint32_t>(padded[block + i * 4]) |
                   (static_cast<uint32_t>(padded[block + i * 4 + 1]) << 8) |
                   (static_cast<uint32_t>(padded[block + i * 4 + 2]) << 16) |
                   (static_cast<uint32_t>(padded[block + i * 4 + 3]) << 24);
        }

        // Left line
        uint32_t AL = h0, BL = h1, CL = h2, DL = h3, EL = h4;
        // Right line
        uint32_t AR = h0, BR = h1, CR = h2, DR = h3, ER = h4;

        // 80 rounds
        for (int i = 0; i < 80; i++) {
            // Left line
            uint32_t TL = AL + RIPEMD160_F(BL, CL, DL, i) + X[RL[i]] + KL[i / 16];
            TL = RIPEMD160_ROL(TL, SL[i]) + EL;
            AL = EL;
            EL = DL;
            DL = RIPEMD160_ROL(CL, 10);
            CL = BL;
            BL = TL;

            // Right line
            uint32_t TR = AR + RIPEMD160_F(BR, CR, DR, 79 - i) + X[RR[i]] + KR[i / 16];
            TR = RIPEMD160_ROL(TR, SR[i]) + ER;
            AR = ER;
            ER = DR;
            DR = RIPEMD160_ROL(CR, 10);
            CR = BR;
            BR = TR;
        }

        // Update state
        uint32_t T = h1 + CL + DR;
        h1 = h2 + DL + ER;
        h2 = h3 + EL + AR;
        h3 = h4 + AL + BR;
        h4 = h0 + BL + CR;
        h0 = T;
    }

    // Output hash in little-endian
    for (int i = 0; i < 4; i++) {
        out[i] = (h0 >> (i * 8)) & 0xFF;
        out[i + 4] = (h1 >> (i * 8)) & 0xFF;
        out[i + 8] = (h2 >> (i * 8)) & 0xFF;
        out[i + 12] = (h3 >> (i * 8)) & 0xFF;
        out[i + 16] = (h4 >> (i * 8)) & 0xFF;
    }

    return true;
}

// === Keccak-256 Implementation ===
// Ethereum uses Keccak-256 (not SHA3-256), which is the original Keccak submission before NIST
// standardization

static const uint64_t KECCAK_ROUND_CONSTANTS[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL, 0x8000000080008000ULL,
    0x000000000000808BULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008AULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800AULL, 0x800000008000000AULL,
    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

static inline uint64_t ROTL64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

static void Keccak_f1600(uint64_t state[25]) {
    static const int r[24] = {1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
                              27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44};

    for (int round = 0; round < 24; round++) {
        // Theta step
        uint64_t C[5];
        for (int i = 0; i < 5; i++) {
            C[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }
        for (int i = 0; i < 5; i++) {
            uint64_t D = C[(i + 4) % 5] ^ ROTL64(C[(i + 1) % 5], 1);
            for (int j = 0; j < 5; j++) {
                state[i + 5 * j] ^= D;
            }
        }

        // Rho and Pi steps
        uint64_t B[25];
        B[0] = state[0];
        int pi_x = 1, pi_y = 0;
        for (int i = 0; i < 24; i++) {
            int pi_index = pi_x + 5 * pi_y;
            B[pi_y + 5 * ((2 * pi_x + 3 * pi_y) % 5)] = ROTL64(state[pi_index], r[i]);
            int temp = pi_x;
            pi_x = pi_y;
            pi_y = (2 * temp + 3 * pi_y) % 5;
        }

        // Chi step
        for (int j = 0; j < 5; j++) {
            uint64_t temp[5];
            for (int i = 0; i < 5; i++) {
                temp[i] = B[i + 5 * j];
            }
            for (int i = 0; i < 5; i++) {
                state[i + 5 * j] = temp[i] ^ ((~temp[(i + 1) % 5]) & temp[(i + 2) % 5]);
            }
        }

        // Iota step
        state[0] ^= KECCAK_ROUND_CONSTANTS[round];
    }
}

bool Keccak256(const uint8_t* data, size_t len, std::array<uint8_t, 32>& out) {
    // Keccak-256 uses rate = 1088 bits (136 bytes), capacity = 512 bits
    const size_t rate = 136;
    const size_t rate_in_words = rate / 8;

    // Initialize state to all zeros
    uint64_t state[25] = {0};

    // Absorb phase
    size_t block_count = 0;
    while (len >= rate) {
        // XOR input block into state (little-endian)
        for (size_t i = 0; i < rate_in_words; i++) {
            uint64_t word = 0;
            for (size_t j = 0; j < 8; j++) {
                word |= static_cast<uint64_t>(data[block_count * rate + i * 8 + j]) << (j * 8);
            }
            state[i] ^= word;
        }
        Keccak_f1600(state);
        len -= rate;
        block_count++;
    }

    // Pad and process final block
    // Keccak-256 uses pad10*1 padding: append 0x01, then zeros, then 0x80
    uint8_t last_block[136] = {0};
    size_t offset = block_count * rate;
    std::memcpy(last_block, data + offset, len);

    // Append padding: 0x01 for Keccak (not 0x06 for SHA3)
    last_block[len] = 0x01;
    last_block[rate - 1] |= 0x80;

    // XOR final block into state
    for (size_t i = 0; i < rate_in_words; i++) {
        uint64_t word = 0;
        for (size_t j = 0; j < 8; j++) {
            word |= static_cast<uint64_t>(last_block[i * 8 + j]) << (j * 8);
        }
        state[i] ^= word;
    }
    Keccak_f1600(state);

    // Squeeze phase - extract first 256 bits (32 bytes)
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 8; j++) {
            out[i * 8 + j] = static_cast<uint8_t>(state[i] >> (j * 8));
        }
    }

    return true;
}

bool HMAC_SHA256(const std::vector<uint8_t>& key, const uint8_t* data, size_t data_len,
                 std::vector<uint8_t>& out) {
    std::vector<uint8_t> hmac_result(32, 0);
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                              BCRYPT_ALG_HANDLE_HMAC_FLAG);
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

    st = BCryptFinishHash(hHash, hmac_result.data(), (ULONG)hmac_result.size(), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;
#else
    unsigned int outLen = 0;
    if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data, data_len,
              hmac_result.data(), &outLen)) {
        return false;
    }
#endif
    out = std::move(hmac_result);
    return true;
}

// Helper for manual HMAC
static bool SHA512_Raw(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
    out.resize(64);
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA512_ALGORITHM, nullptr, 0)))
        return false;

    if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    if (!BCRYPT_SUCCESS(BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    NTSTATUS st = BCryptFinishHash(hHash, out.data(), (ULONG)out.size(), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(st);
#else
    unsigned int outLen = 0;
    if (EVP_Digest(data, len, out.data(), &outLen, EVP_sha512(), nullptr) != 1) {
        return false;
    }
    return true;
#endif
}

bool HMAC_SHA512(const std::vector<uint8_t>& key, const uint8_t* data, size_t data_len,
                 std::vector<uint8_t>& out) {
    std::vector<uint8_t> hmac_result(64, 0);
#ifdef _WIN32
    const size_t blockSize = 128;  // SHA-512 block size
    std::vector<uint8_t> processedKey(blockSize, 0);

    if (key.size() > blockSize) {
        std::vector<uint8_t> hash;
        if (!SHA512_Raw(key.data(), key.size(), hash))
            return false;
        std::memcpy(processedKey.data(), hash.data(), hash.size());
    } else {
        std::memcpy(processedKey.data(), key.data(), key.size());
    }

    std::vector<uint8_t> ipad(blockSize);
    std::vector<uint8_t> opad(blockSize);

    for (size_t i = 0; i < blockSize; ++i) {
        ipad[i] = processedKey[i] ^ 0x36;
        opad[i] = processedKey[i] ^ 0x5c;
    }

    // Inner hash: SHA512(ipad || data)
    std::vector<uint8_t> innerHash;
    {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        if (!BCRYPT_SUCCESS(
                BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA512_ALGORITHM, nullptr, 0)))
            return false;
        if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        BCryptHashData(hHash, ipad.data(), (ULONG)ipad.size(), 0);
        BCryptHashData(hHash, (PUCHAR)data, (ULONG)data_len, 0);

        innerHash.resize(64);
        BCryptFinishHash(hHash, innerHash.data(), 64, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    // Outer hash: SHA512(opad || innerHash)
    {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        if (!BCRYPT_SUCCESS(
                BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA512_ALGORITHM, nullptr, 0)))
            return false;
        if (!BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return false;
        }

        BCryptHashData(hHash, opad.data(), (ULONG)opad.size(), 0);
        BCryptHashData(hHash, innerHash.data(), (ULONG)innerHash.size(), 0);

        BCryptFinishHash(hHash, hmac_result.data(), 64, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    // Wipe sensitive data
    SecureWipeVector(processedKey);
    SecureWipeVector(ipad);
    SecureWipeVector(opad);
#else
    unsigned int outLen = 0;
    if (!HMAC(EVP_sha512(), key.data(), static_cast<int>(key.size()), data, data_len,
              hmac_result.data(), &outLen)) {
        return false;
    }
#endif
    out = std::move(hmac_result);
    return true;
}

// === Key Derivation Functions ===
bool PBKDF2_HMAC_SHA256(const std::string& password, const uint8_t* salt, size_t salt_len,
                        uint32_t iterations, std::vector<uint8_t>& out_key, size_t dk_len) {
    out_key.assign(dk_len, 0);
    const size_t hash_len = 32;
    const size_t blocks = (dk_len + hash_len - 1) / hash_len;

    std::vector<uint8_t> key(password.begin(), password.end());
    std::vector<uint8_t> block_input(salt_len + 4);
    std::memcpy(block_input.data(), salt, salt_len);

    for (size_t i = 0; i < blocks; ++i) {
        uint32_t block_num = static_cast<uint32_t>(i + 1);
        block_input[salt_len] = (block_num >> 24) & 0xFF;
        block_input[salt_len + 1] = (block_num >> 16) & 0xFF;
        block_input[salt_len + 2] = (block_num >> 8) & 0xFF;
        block_input[salt_len + 3] = (block_num) & 0xFF;

        std::vector<uint8_t> u, result(hash_len, 0);

        if (!HMAC_SHA256(key, block_input.data(), block_input.size(), u))
            return false;

        for (size_t j = 0; j < hash_len; ++j)
            result[j] = u[j];

        for (uint32_t iter = 1; iter < iterations; ++iter) {
            if (!HMAC_SHA256(key, u.data(), u.size(), u))
                return false;
            for (size_t j = 0; j < hash_len; ++j)
                result[j] ^= u[j];
        }

        size_t copy_len = std::min(hash_len, dk_len - i * hash_len);
        std::memcpy(out_key.data() + i * hash_len, result.data(), copy_len);
    }
    return true;
}

bool PBKDF2_HMAC_SHA512(const std::string& password, const uint8_t* salt, size_t salt_len,
                        uint32_t iterations, std::vector<uint8_t>& out_key, size_t dk_len) {
    out_key.assign(dk_len, 0);
    const size_t hash_len = 64;
    const size_t blocks = (dk_len + hash_len - 1) / hash_len;

    std::vector<uint8_t> key(password.begin(), password.end());
    std::vector<uint8_t> block_input(salt_len + 4);
    std::memcpy(block_input.data(), salt, salt_len);

    for (size_t i = 0; i < blocks; ++i) {
        uint32_t block_num = static_cast<uint32_t>(i + 1);
        block_input[salt_len] = (block_num >> 24) & 0xFF;
        block_input[salt_len + 1] = (block_num >> 16) & 0xFF;
        block_input[salt_len + 2] = (block_num >> 8) & 0xFF;
        block_input[salt_len + 3] = (block_num) & 0xFF;

        std::vector<uint8_t> u, result(hash_len, 0);

        if (!HMAC_SHA512(key, block_input.data(), block_input.size(), u))
            return false;

        for (size_t j = 0; j < hash_len; ++j)
            result[j] = u[j];

        for (uint32_t iter = 1; iter < iterations; ++iter) {
            if (!HMAC_SHA512(key, u.data(), u.size(), u))
                return false;
            for (size_t j = 0; j < hash_len; ++j)
                result[j] ^= u[j];
        }

        size_t copy_len = std::min(hash_len, dk_len - i * hash_len);
        std::memcpy(out_key.data() + i * hash_len, result.data(), copy_len);
    }
    return true;
}

// === Windows DPAPI Functions ===
// === Windows DPAPI Functions ===
bool DPAPI_Protect(const std::vector<uint8_t>& plaintext, const std::string& entropy_str,
                   std::vector<uint8_t>& ciphertext) {
#ifdef _WIN32
    DATA_BLOB in{}, out{}, entropy{};
    in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()));
    in.cbData = static_cast<DWORD>(plaintext.size());
    entropy.pbData = (BYTE*)entropy_str.data();
    entropy.cbData = (DWORD)entropy_str.size();

    if (!CryptProtectData(&in, L"seed", &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &out))
        return false;

    ciphertext.assign(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return true;
#else
    (void)plaintext;
    (void)entropy_str;
    (void)ciphertext;
    return false;
#endif
}

bool DPAPI_Unprotect(const std::vector<uint8_t>& ciphertext, const std::string& entropy_str,
                     std::vector<uint8_t>& plaintext) {
#ifdef _WIN32
    DATA_BLOB in{}, out{}, entropy{};
    in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(ciphertext.data()));
    in.cbData = static_cast<DWORD>(ciphertext.size());
    entropy.pbData = (BYTE*)entropy_str.data();
    entropy.cbData = (DWORD)entropy_str.size();

    if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                            &out))
        return false;

    plaintext.assign(out.pbData, out.pbData + out.cbData);
    LocalFree(out.pbData);
    return true;
#else
    (void)ciphertext;
    (void)entropy_str;
    (void)plaintext;
    return false;
#endif
}

// === BIP-39 Cryptographic Functions ===
bool LoadBIP39Wordlist(std::vector<std::string>& wordlist) {
    wordlist.clear();

    // Build list of possible paths - order matters (most likely first)
    std::vector<std::string> possiblePaths;

    // On Windows and Linux, try executable-relative paths first
#ifdef _WIN32
    char exePath[MAX_PATH] = {0};
    DWORD pathLen = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (pathLen > 0 && pathLen < MAX_PATH) {
        std::filesystem::path binPath(exePath);
        std::filesystem::path exeDir = binPath.parent_path();

        // For installed app: C:\Program Files\CriptoGualet\bin\CriptoGualetQt.exe
        // Assets at: C:\Program Files\CriptoGualet\bin\assets\bip39\english.txt
        possiblePaths.push_back((exeDir / "assets" / "bip39" / "english.txt").string());

        // For installed app variant: assets might be one level up
        // C:\Program Files\CriptoGualet\assets\bip39\english.txt
        possiblePaths.push_back(
            (exeDir.parent_path() / "assets" / "bip39" / "english.txt").string());

        // For development: exe in build/bin, assets in project root
        possiblePaths.push_back(
            (exeDir.parent_path().parent_path() / "assets" / "bip39" / "english.txt").string());

        // For development: exe in build/bin/Release or build/bin/Debug
        possiblePaths.push_back(
            (exeDir.parent_path().parent_path().parent_path() / "assets" / "bip39" / "english.txt")
                .string());
    }
#else
    char exePath[1024];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();

        // Installed path: /usr/bin/CriptoGualetQt -> /usr/bin/assets/bip39/english.txt
        possiblePaths.push_back((exeDir / "assets" / "bip39" / "english.txt").string());

        // Development path: out/build/linux-dbg/bin/CriptoGualetQt -> assets/bip39/english.txt
        // (project root)
        possiblePaths.push_back(
            (exeDir.parent_path().parent_path() / "assets" / "bip39" / "english.txt").string());
    }
#endif

    // Environment variable override (highest priority if set)
#pragma warning(push)
#pragma warning(disable : 4996)  // Suppress getenv warning
    if (const char* env = std::getenv("BIP39_WORDLIST")) {
        // Insert at beginning for highest priority
        possiblePaths.insert(possiblePaths.begin(), env);
    }
#pragma warning(pop)

    // Fallback paths for development/testing (relative to CWD)
    possiblePaths.push_back("assets/bip39/english.txt");
    possiblePaths.push_back("src/assets/bip39/english.txt");
    possiblePaths.push_back("../assets/bip39/english.txt");
    possiblePaths.push_back("../src/assets/bip39/english.txt");
    possiblePaths.push_back("../../assets/bip39/english.txt");
    possiblePaths.push_back("../../../assets/bip39/english.txt");
    possiblePaths.push_back("../../../../assets/bip39/english.txt");
    possiblePaths.push_back("../../../../../assets/bip39/english.txt");

    // Try each path
    for (const auto& path : possiblePaths) {
        if (path.empty())
            continue;

        // Check if path exists before trying to open
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            continue;  // Skip non-existent paths silently
        }

        std::ifstream f(path, std::ios::binary);
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                // Trim whitespace/newlines
                while (!line.empty() && (std::isspace(static_cast<unsigned char>(line.back())) ||
                                         line.back() == '\r' || line.back() == '\n')) {
                    line.pop_back();
                }
                size_t start = 0;
                while (start < line.size() &&
                       std::isspace(static_cast<unsigned char>(line[start]))) {
                    start++;
                }
                if (start < line.size()) {
                    wordlist.push_back(line.substr(start));
                }
            }
            f.close();

            if (wordlist.size() == 2048) {
                // Successfully loaded wordlist
                return true;
            }
            // Wrong number of words - try next path
            wordlist.clear();
        }
    }

    return false;
}

bool GenerateEntropy(size_t bits, std::vector<uint8_t>& out) {
    if (bits % 32 != 0 || bits < 128 || bits > 256)
        return false;
    out.assign(bits / 8, 0);
    return RandBytes(out.data(), out.size());
}

bool MnemonicFromEntropy(const std::vector<uint8_t>& entropy,
                         const std::vector<std::string>& wordlist,
                         std::vector<std::string>& outMnemonic) {
    if (wordlist.size() != 2048)
        return false;
    const size_t ENT = entropy.size() * 8;
    const size_t CS = ENT / 32;
    const size_t MS = ENT + CS;
    const size_t words = MS / 11;

    std::array<uint8_t, 32> hash{};
    if (!SHA256(entropy.data(), entropy.size(), hash))
        return false;

    std::vector<int> bits;
    bits.reserve(MS);
    for (uint8_t b : entropy)
        for (int i = 7; i >= 0; --i)
            bits.push_back((b >> i) & 1);
    for (size_t i = 0; i < CS; ++i) {
        size_t byteIdx = i / 8;
        int bitInByte = 7 - (i % 8);
        int bit = (hash[byteIdx] >> bitInByte) & 1;
        bits.push_back(bit);
    }

    outMnemonic.clear();
    outMnemonic.reserve(words);
    for (size_t i = 0; i < words; ++i) {
        uint32_t idx = 0;
        for (size_t j = 0; j < 11; ++j)
            idx = (idx << 1) | bits[i * 11 + j];
        outMnemonic.push_back(wordlist[idx]);
    }
    return outMnemonic.size() == words;
}

bool ValidateMnemonic(const std::vector<std::string>& mnemonic,
                      const std::vector<std::string>& wordlist) {
    if (wordlist.size() != 2048)
        return false;
    const size_t n = mnemonic.size();
    if (!(n == 12 || n == 15 || n == 18 || n == 21 || n == 24))
        return false;

    // Build bitstream from word indices
    std::vector<int> bits;
    bits.reserve(n * 11);
    for (const auto& w : mnemonic) {
        auto it = std::lower_bound(wordlist.begin(), wordlist.end(), w);
        if (it == wordlist.end() || *it != w)
            return false;  // word not in list
        uint32_t idx = static_cast<uint32_t>(std::distance(wordlist.begin(), it));
        // push 11 bits
        for (int i = 10; i >= 0; --i)
            bits.push_back((idx >> i) & 1);
    }

    const size_t MS = n * 11;
    const size_t ENT = (MS * 32) / 33;  // ENT = MS * 32/33
    const size_t CS = MS - ENT;

    // Rebuild entropy
    std::vector<uint8_t> entropy(ENT / 8, 0);
    for (size_t i = 0; i < ENT; ++i) {
        size_t byteIdx = i / 8;
        entropy[byteIdx] = (uint8_t)((entropy[byteIdx] << 1) | bits[i]);
        if (i % 8 == 7) { /*byte complete*/
        }
    }
    // Compute hash and compare checksum bits
    std::array<uint8_t, 32> hash{};
    if (!SHA256(entropy.data(), entropy.size(), hash))
        return false;

    for (size_t i = 0; i < CS; ++i) {
        size_t bitIdx = ENT + i;
        int expected = bits[bitIdx];
        size_t byteIdx = i / 8;
        int bitInByte = 7 - (i % 8);
        int actual = (hash[byteIdx] >> bitInByte) & 1;
        if (expected != actual)
            return false;
    }
    return true;
}

bool BIP39_SeedFromMnemonic(const std::vector<std::string>& mnemonic, const std::string& passphrase,
                            std::array<uint8_t, 64>& outSeed) {
    std::ostringstream oss;
    for (size_t i = 0; i < mnemonic.size(); ++i) {
        if (i)
            oss << ' ';
        oss << mnemonic[i];
    }
    const std::string sentence = oss.str();
    const std::string salt_str = std::string("mnemonic") + passphrase;

    std::vector<uint8_t> dk;
    if (!PBKDF2_HMAC_SHA512(sentence, reinterpret_cast<const uint8_t*>(salt_str.data()),
                            salt_str.size(), 2048, dk, 64)) {
        return false;
    }
    std::memcpy(outSeed.data(), dk.data(), 64);
    std::fill(dk.begin(), dk.end(), uint8_t(0));
    return true;
}

// === Utility Functions ===
bool ConstantTimeEquals(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
        diff |= (a[i] ^ b[i]);
    return diff == 0;
}

// === Memory Security Functions ===
#ifdef SecureZeroMemory
#undef SecureZeroMemory
#endif

void SecureClear(void* ptr, size_t size) {
    if (!ptr || size == 0)
        return;

#ifdef _WIN32
    RtlSecureZeroMemory(ptr, size);
#else
    static bool sodium_initialized = false;
    if (!sodium_initialized) {
        if (sodium_init() < 0) {
            // Fallback if sodium fails to initialize
            volatile uint8_t* vptr = static_cast<volatile uint8_t*>(ptr);
            for (size_t i = 0; i < size; ++i) {
                vptr[i] = 0;
            }
            __sync_synchronize();
            return;
        }
        sodium_initialized = true;
    }
    sodium_memzero(ptr, size);
#endif
}

void SecureWipeVector(std::vector<uint8_t>& vec) {
    if (!vec.empty()) {
        SecureClear(vec.data(), vec.size());
        vec.clear();
        vec.shrink_to_fit();
    }
}

void SecureWipeString(std::string& str) {
    if (!str.empty()) {
        SecureClear(&str[0], str.size());
        str.clear();
        str.shrink_to_fit();
    }
}

// === Secure Key Derivation ===
bool DeriveWalletKey(const std::string& password, const std::vector<uint8_t>& salt,
                     std::vector<uint8_t>& derived_key, size_t key_length) {
    if (password.empty() || salt.size() < 16 || key_length < 32) {
        return false;
    }

    const uint32_t iterations = 600000;  // Strong iteration count
    return PBKDF2_HMAC_SHA512(password, salt.data(), salt.size(), iterations, derived_key,
                              key_length);
}

bool DeriveDBEncryptionKey(const std::string& password, const std::vector<uint8_t>& salt,
                           std::vector<uint8_t>& db_key) {
    return DeriveWalletKey(password, salt, db_key, 32);  // 256-bit key for database
}

// === AES-GCM Encryption/Decryption ===
bool AES_GCM_Encrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& plaintext,
                     const std::vector<uint8_t>& aad, std::vector<uint8_t>& ciphertext,
                     std::vector<uint8_t>& iv, std::vector<uint8_t>& tag) {
    if (key.size() != 32)  // AES-256 key required
        return false;

    // Generate random IV
    iv.resize(12);  // 96-bit IV for GCM
    if (!RandBytes(iv.data(), iv.size()))
        return false;

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(st))
        return false;

    // Set chaining mode to GCM
    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(),
                                    0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Setup auth info for GCM
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = (ULONG)iv.size();
    authInfo.pbAuthData = const_cast<PUCHAR>(aad.data());
    authInfo.cbAuthData = (ULONG)aad.size();

    tag.resize(16);  // 128-bit tag
    authInfo.pbTag = tag.data();
    authInfo.cbTag = (ULONG)tag.size();

    ULONG result_len = 0;
    ciphertext.resize(plaintext.size());

    st = BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(), &authInfo, nullptr,
                       0, ciphertext.data(), (ULONG)ciphertext.size(), &result_len, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(st)) {
        SecureWipeVector(ciphertext);
        SecureWipeVector(iv);
        SecureWipeVector(tag);
        return false;
    }
#else
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int len;
    if (!aad.empty()) {
        if (EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), aad.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
    }

    ciphertext.resize(plaintext.size());
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_EncryptFinal_ex(ctx, nullptr, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    tag.resize(16);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    EVP_CIPHER_CTX_free(ctx);
#endif
    return true;
}

bool AES_GCM_Decrypt(const std::vector<uint8_t>& key, const std::vector<uint8_t>& ciphertext,
                     const std::vector<uint8_t>& aad, const std::vector<uint8_t>& iv,
                     const std::vector<uint8_t>& tag, std::vector<uint8_t>& plaintext) {
    if (key.size() != 32 || iv.size() != 12 || tag.size() != 16)
        return false;

#ifdef _WIN32
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

    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(),
                                    0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = const_cast<PUCHAR>(iv.data());
    authInfo.cbNonce = (ULONG)iv.size();
    authInfo.pbAuthData = const_cast<PUCHAR>(aad.data());
    authInfo.cbAuthData = (ULONG)aad.size();
    authInfo.pbTag = const_cast<PUCHAR>(tag.data());
    authInfo.cbTag = (ULONG)tag.size();

    ULONG result_len = 0;
    plaintext.resize(ciphertext.size());

    st = BCryptDecrypt(hKey, (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(), &authInfo,
                       nullptr, 0, plaintext.data(), (ULONG)plaintext.size(), &result_len, 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (!BCRYPT_SUCCESS(st)) {
        SecureWipeVector(plaintext);
        return false;
    }

    plaintext.resize(result_len);
    return true;
#else
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return false;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv.size(), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int len;
    if (!aad.empty()) {
        if (EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), aad.size()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }
    }

    plaintext.resize(ciphertext.size());
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<uint8_t*>(tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) {
        SecureWipeVector(plaintext);
        return false;
    }

    return true;
#endif
}

// === Database Encryption Functions ===
bool EncryptDBData(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data,
                   std::vector<uint8_t>& encrypted_blob) {
    if (key.size() != 32 || data.empty())
        return false;

    std::vector<uint8_t> ciphertext, iv, tag;
    std::vector<uint8_t> aad;  // No additional authenticated data for DB encryption

    if (!AES_GCM_Encrypt(key, data, aad, ciphertext, iv, tag))
        return false;

    // Format: [IV(12)] + [TAG(16)] + [CIPHERTEXT]
    encrypted_blob.clear();
    encrypted_blob.reserve(iv.size() + tag.size() + ciphertext.size());
    encrypted_blob.insert(encrypted_blob.end(), iv.begin(), iv.end());
    encrypted_blob.insert(encrypted_blob.end(), tag.begin(), tag.end());
    encrypted_blob.insert(encrypted_blob.end(), ciphertext.begin(), ciphertext.end());

    // Clean up sensitive data
    SecureWipeVector(ciphertext);
    SecureWipeVector(iv);
    SecureWipeVector(tag);

    return true;
}

bool DecryptDBData(const std::vector<uint8_t>& key, const std::vector<uint8_t>& encrypted_blob,
                   std::vector<uint8_t>& data) {
    if (key.size() != 32 || encrypted_blob.size() < 28)  // min: 12(IV) + 16(TAG)
        return false;

    // Extract components: [IV(12)] + [TAG(16)] + [CIPHERTEXT]
    std::vector<uint8_t> iv(encrypted_blob.begin(), encrypted_blob.begin() + 12);
    std::vector<uint8_t> tag(encrypted_blob.begin() + 12, encrypted_blob.begin() + 28);
    std::vector<uint8_t> ciphertext(encrypted_blob.begin() + 28, encrypted_blob.end());
    std::vector<uint8_t> aad;  // No additional authenticated data

    bool success = AES_GCM_Decrypt(key, ciphertext, aad, iv, tag, data);

    // Clean up
    SecureWipeVector(iv);
    SecureWipeVector(tag);
    SecureWipeVector(ciphertext);

    return success;
}

// === Encrypted Seed Storage ===

bool EncryptSeedPhrase(const std::string& password, const std::vector<std::string>& mnemonic,
                       EncryptedSeed& encrypted_seed) {
    if (password.empty() || mnemonic.empty())
        return false;

    // Generate random salt
    encrypted_seed.salt.resize(32);
    if (!RandBytes(encrypted_seed.salt.data(), encrypted_seed.salt.size()))
        return false;

    // Derive encryption key
    std::vector<uint8_t> encryption_key;
    if (!DeriveWalletKey(password, encrypted_seed.salt, encryption_key, 32))
        return false;

    // Serialize mnemonic
    std::ostringstream oss;
    for (size_t i = 0; i < mnemonic.size(); ++i) {
        if (i > 0)
            oss << " ";
        oss << mnemonic[i];
    }
    std::string mnemonic_str = oss.str();
    std::vector<uint8_t> mnemonic_data(mnemonic_str.begin(), mnemonic_str.end());

    // Encrypt the mnemonic
    if (!EncryptDBData(encryption_key, mnemonic_data, encrypted_seed.encrypted_data)) {
        SecureWipeVector(encryption_key);
        SecureWipeString(mnemonic_str);
        SecureWipeVector(mnemonic_data);
        return false;
    }

    // Create verification hash (password + salt)
    std::vector<uint8_t> password_data(password.begin(), password.end());
    std::vector<uint8_t> combined_data;
    combined_data.insert(combined_data.end(), password_data.begin(), password_data.end());
    combined_data.insert(combined_data.end(), encrypted_seed.salt.begin(),
                         encrypted_seed.salt.end());

    std::array<uint8_t, 32> hash_result;
    if (!SHA256(combined_data.data(), combined_data.size(), hash_result)) {
        SecureWipeVector(encryption_key);
        SecureWipeString(mnemonic_str);
        SecureWipeVector(mnemonic_data);
        SecureWipeVector(password_data);
        SecureWipeVector(combined_data);
        return false;
    }

    encrypted_seed.verification_hash.assign(hash_result.begin(), hash_result.end());

    // Clean up
    SecureWipeVector(encryption_key);
    SecureWipeString(mnemonic_str);
    SecureWipeVector(mnemonic_data);
    SecureWipeVector(password_data);
    SecureWipeVector(combined_data);

    return true;
}

bool DecryptSeedPhrase(const std::string& password, const EncryptedSeed& encrypted_seed,
                       std::vector<std::string>& mnemonic) {
    if (password.empty() || encrypted_seed.salt.empty() || encrypted_seed.encrypted_data.empty())
        return false;

    // Verify password using stored hash
    std::vector<uint8_t> password_data(password.begin(), password.end());
    std::vector<uint8_t> combined_data;
    combined_data.insert(combined_data.end(), password_data.begin(), password_data.end());
    combined_data.insert(combined_data.end(), encrypted_seed.salt.begin(),
                         encrypted_seed.salt.end());

    std::array<uint8_t, 32> computed_hash;
    if (!SHA256(combined_data.data(), combined_data.size(), computed_hash)) {
        SecureWipeVector(password_data);
        SecureWipeVector(combined_data);
        return false;
    }

    std::vector<uint8_t> computed_hash_vec(computed_hash.begin(), computed_hash.end());
    if (!ConstantTimeEquals(computed_hash_vec, encrypted_seed.verification_hash)) {
        SecureWipeVector(password_data);
        SecureWipeVector(combined_data);
        SecureWipeVector(computed_hash_vec);
        return false;  // Wrong password
    }

    // Derive decryption key
    std::vector<uint8_t> decryption_key;
    if (!DeriveWalletKey(password, encrypted_seed.salt, decryption_key, 32)) {
        SecureWipeVector(password_data);
        SecureWipeVector(combined_data);
        SecureWipeVector(computed_hash_vec);
        return false;
    }

    // Decrypt the mnemonic
    std::vector<uint8_t> decrypted_data;
    if (!DecryptDBData(decryption_key, encrypted_seed.encrypted_data, decrypted_data)) {
        SecureWipeVector(password_data);
        SecureWipeVector(combined_data);
        SecureWipeVector(computed_hash_vec);
        SecureWipeVector(decryption_key);
        return false;
    }

    // Parse mnemonic string
    std::string mnemonic_str(decrypted_data.begin(), decrypted_data.end());
    std::istringstream iss(mnemonic_str);
    std::string word;
    mnemonic.clear();
    while (iss >> word) {
        mnemonic.push_back(word);
    }

    // Clean up
    SecureWipeVector(password_data);
    SecureWipeVector(combined_data);
    SecureWipeVector(computed_hash_vec);
    SecureWipeVector(decryption_key);
    SecureWipeVector(decrypted_data);
    SecureWipeString(mnemonic_str);

    return !mnemonic.empty();
}

// === Secure Random Salt Generation ===
bool GenerateSecureSalt(std::vector<uint8_t>& salt, size_t size) {
    if (size < 16)
        size = 32;  // Default to 256-bit salt

    salt.resize(size);
    return RandBytes(salt.data(), salt.size());
}

// === Database Key Management ===

bool CreateDatabaseKey(const std::string& password, DatabaseKeyInfo& key_info,
                       std::vector<uint8_t>& database_key) {
    if (password.empty())
        return false;

    // Generate secure salt
    if (!GenerateSecureSalt(key_info.salt, 32))
        return false;

    key_info.iteration_count = 600000;  // Strong iteration count

    // Derive database key
    if (!PBKDF2_HMAC_SHA512(password, key_info.salt.data(), key_info.salt.size(),
                            key_info.iteration_count, database_key, 32))
        return false;

    // Create verification hash
    std::vector<uint8_t> verification_data;
    verification_data.insert(verification_data.end(), database_key.begin(), database_key.end());
    verification_data.insert(verification_data.end(), key_info.salt.begin(), key_info.salt.end());

    std::array<uint8_t, 32> hash_result;
    if (!SHA256(verification_data.data(), verification_data.size(), hash_result)) {
        SecureWipeVector(database_key);
        SecureWipeVector(verification_data);
        return false;
    }

    key_info.key_verification_hash.assign(hash_result.begin(), hash_result.end());
    SecureWipeVector(verification_data);

    return true;
}

bool VerifyDatabaseKey(const std::string& password, const DatabaseKeyInfo& key_info,
                       std::vector<uint8_t>& database_key) {
    if (password.empty() || key_info.salt.empty())
        return false;

    // Derive key
    if (!PBKDF2_HMAC_SHA512(password, key_info.salt.data(), key_info.salt.size(),
                            key_info.iteration_count, database_key, 32))
        return false;

    // Verify key
    std::vector<uint8_t> verification_data;
    verification_data.insert(verification_data.end(), database_key.begin(), database_key.end());
    verification_data.insert(verification_data.end(), key_info.salt.begin(), key_info.salt.end());

    std::array<uint8_t, 32> computed_hash;
    if (!SHA256(verification_data.data(), verification_data.size(), computed_hash)) {
        SecureWipeVector(database_key);
        SecureWipeVector(verification_data);
        return false;
    }

    std::vector<uint8_t> computed_hash_vec(computed_hash.begin(), computed_hash.end());
    bool valid = ConstantTimeEquals(computed_hash_vec, key_info.key_verification_hash);

    SecureWipeVector(verification_data);
    SecureWipeVector(computed_hash_vec);

    if (!valid) {
        SecureWipeVector(database_key);
        return false;
    }

    return true;
}

// === BIP32 Hierarchical Deterministic Key Derivation ===

// Helper: Read 4 bytes as big-endian uint32
static uint32_t ReadBE32(const uint8_t* ptr) {
    return (static_cast<uint32_t>(ptr[0]) << 24) | (static_cast<uint32_t>(ptr[1]) << 16) |
           (static_cast<uint32_t>(ptr[2]) << 8) | static_cast<uint32_t>(ptr[3]);
}

// Helper: Write uint32 as big-endian 4 bytes
static void WriteBE32(uint8_t* ptr, uint32_t value) {
    ptr[0] = static_cast<uint8_t>(value >> 24);
    ptr[1] = static_cast<uint8_t>(value >> 16);
    ptr[2] = static_cast<uint8_t>(value >> 8);
    ptr[3] = static_cast<uint8_t>(value);
}

// Helper: Base58 encoding for Bitcoin addresses and WIF
static const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::string EncodeBase58(const std::vector<uint8_t>& data) {
    // Count leading zeros
    size_t leading_zeros = 0;
    for (size_t i = 0; i < data.size() && data[i] == 0; ++i) {
        ++leading_zeros;
    }

    // Allocate enough space in big-endian base58 representation
    std::vector<uint8_t> b58((data.size() - leading_zeros) * 138 / 100 + 1);

    // Process the bytes
    for (size_t i = leading_zeros; i < data.size(); ++i) {
        int carry = data[i];
        for (size_t j = 0; j < b58.size(); ++j) {
            carry += 256 * b58[j];
            b58[j] = carry % 58;
            carry /= 58;
        }
        while (carry > 0) {
            b58.push_back(carry % 58);
            carry /= 58;
        }
    }

    // Find the last non-zero element (actual end of data)
    size_t b58_end = b58.size();
    while (b58_end > 0 && b58[b58_end - 1] == 0) {
        --b58_end;
    }

    // Skip leading zeros in base58 result
    size_t b58_leading_zeros = 0;
    for (size_t i = 0; i < b58_end && b58[i] == 0; ++i) {
        ++b58_leading_zeros;
    }

    // Translate the result into a string
    std::string result;
    result.reserve(leading_zeros + (b58_end - b58_leading_zeros));
    result.assign(leading_zeros, '1');

    for (size_t i = b58_leading_zeros; i < b58_end; ++i) {
        result += BASE58_ALPHABET[b58[b58_end - 1 - i]];
    }

    return result;
}

// Helper: Base58 decoding
std::vector<uint8_t> DecodeBase58(const std::string& str) {
    std::vector<uint8_t> result;
    // Map base58 characters to values
    static const int8_t mapBase58[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  -1, -1, -1, -1, -1, -1, -1, 9,
        10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1, 22, 23, 24, 25, 26, 27, 28, 29,
        30, 31, 32, -1, -1, -1, -1, -1, -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44,
        45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };

    // Skip leading spaces
    size_t i = 0;
    while (i < str.length() && isspace(str[i]))
        i++;
    // Skip and count leading '1's
    size_t zeroes = 0;
    while (i < str.length() && str[i] == '1') {
        zeroes++;
        i++;
    }
    // Allocate enough space in big-endian base256 representation
    size_t size = (str.length() - i) * 733 / 1000 + 1;  // log(58) / log(256), rounded up
    std::vector<uint8_t> b256(size);
    // Process the characters
    while (i < str.length() && !isspace(str[i])) {
        // Decode base58 character
        int carry = mapBase58[(uint8_t)str[i]];
        if (carry == -1)  // Invalid b58 character
            return std::vector<uint8_t>();
        int j = 0;
        for (std::vector<uint8_t>::reverse_iterator it = b256.rbegin();
             (it != b256.rend()) || (carry != 0); ++it, ++j) {
            if (it == b256.rend()) {
                if (carry != 0) {
                    b256.insert(b256.begin(), 0);
                    it = b256.rbegin() + j;
                } else {
                    break;
                }
            }
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
        i++;
    }
    // Skip leading zeroes in b256
    std::vector<uint8_t>::iterator it = b256.begin();
    while (it != b256.end() && *it == 0)
        it++;
    // Copy result into output vector
    result.reserve(zeroes + (b256.end() - it));
    result.assign(zeroes, 0x00);
    while (it != b256.end())
        result.push_back(*(it++));
    return result;
}

// Helper: Base58Check encoding (with double SHA256 checksum)
std::string EncodeBase58Check(const std::vector<uint8_t>& data) {
    // Add 4-byte checksum (first 4 bytes of double SHA256)
    std::array<uint8_t, 32> hash1, hash2;
    if (!SHA256(data.data(), data.size(), hash1)) {
        return "";
    }
    if (!SHA256(hash1.data(), hash1.size(), hash2)) {
        return "";
    }

    std::vector<uint8_t> data_with_checksum = data;
    data_with_checksum.insert(data_with_checksum.end(), hash2.begin(), hash2.begin() + 4);

    return EncodeBase58(data_with_checksum);
}

// Helper: Decode Base58Check to get payload (without version byte and checksum)
bool DecodeBase58Check(const std::string& address, std::vector<uint8_t>& payload) {
    std::vector<uint8_t> decoded = DecodeBase58(address);
    if (decoded.size() < 4) {
        return false;  // Too short to contain checksum
    }

    // Split data and checksum
    std::vector<uint8_t> data(decoded.begin(), decoded.end() - 4);
    std::vector<uint8_t> checksum(decoded.end() - 4, decoded.end());

    // Calculate expected checksum
    std::array<uint8_t, 32> hash1, hash2;
    if (!SHA256(data.data(), data.size(), hash1))
        return false;
    if (!SHA256(hash1.data(), hash1.size(), hash2))
        return false;

    // Compare checksums
    for (int i = 0; i < 4; i++) {
        if (checksum[i] != hash2[i])
            return false;
    }

    payload = data;
    return true;
}

bool ImportExtendedKey(const std::string& encoded, BIP32ExtendedKey& outKey) {
    // Decode Base58Check
    std::vector<uint8_t> decoded;
    if (!DecodeBase58Check(encoded, decoded)) {
        return false;
    }

    // Check length: 4 (version) + 1 (depth) + 4 (fingerprint) + 4 (child) + 32 (chain) + 33 (key) =
    // 78
    if (decoded.size() != 78) {
        return false;
    }

    // Parse fields
    // Version bytes (4)
    uint32_t version = ReadBE32(decoded.data());
    // Determine if private or public based on version
    // Mainnet Public: 0x0488B21E (xpub)
    // Mainnet Private: 0x0488ADE4 (xprv)
    // Testnet Public: 0x043587CF (tpub)
    // Testnet Private: 0x04358394 (tprv)
    bool isPrivate = (version == 0x0488ADE4 || version == 0x04358394);

    // Depth (1)
    outKey.depth = decoded[4];

    // Fingerprint (4)
    outKey.fingerprint = ReadBE32(decoded.data() + 5);

    // Child number (4)
    outKey.childNumber = ReadBE32(decoded.data() + 9);

    // Chain code (32)
    outKey.chainCode.assign(decoded.begin() + 13, decoded.begin() + 45);

    // Key data (33)
    outKey.key.assign(decoded.begin() + 45, decoded.begin() + 78);
    outKey.isPrivate = isPrivate;

    return true;
}

// NOTE: For production use, this requires secp256k1 library for proper elliptic curve operations
// This is a simplified implementation that provides the structure but needs external crypto library
// TODO: Integrate secp256k1 for proper point addition and scalar multiplication

bool BIP32_MasterKeyFromSeed(const std::array<uint8_t, 64>& seed, BIP32ExtendedKey& masterKey) {
    // BIP32 specifies: I = HMAC-SHA512(Key = "Bitcoin seed", Data = S)
    const char* hmac_key_str = "Bitcoin seed";
    std::vector<uint8_t> hmac_key(hmac_key_str, hmac_key_str + std::strlen(hmac_key_str));

    std::vector<uint8_t> I;
    if (!HMAC_SHA512(hmac_key, seed.data(), seed.size(), I)) {
        return false;
    }

    // Split I into two 32-byte sequences: IL (master private key) and IR (master chain code)
    masterKey.key.assign(I.begin(), I.begin() + 32);
    masterKey.chainCode.assign(I.begin() + 32, I.begin() + 64);
    masterKey.depth = 0;
    masterKey.fingerprint = 0;
    masterKey.childNumber = 0;
    masterKey.isPrivate = true;

    // Securely wipe the HMAC result
    SecureWipeVector(I);

    // TODO: Verify that the key is valid (not zero and less than secp256k1 curve order)
    // For now, assume it's valid (extremely unlikely to be invalid)

    return true;
}

bool BIP32_DeriveChild(const BIP32ExtendedKey& parent, uint32_t index, BIP32ExtendedKey& child) {
    if (!parent.isPrivate && index >= 0x80000000) {
        // Cannot derive hardened child from public key
        return false;
    }

    const bool hardened = (index >= 0x80000000);

    // Prepare data for HMAC-SHA512
    std::vector<uint8_t> data;
    data.reserve(37);  // Max size: 1 + 32 + 4

    if (hardened) {
        // Hardened derivation: data = 0x00 || ser256(kpar) || ser32(i)
        data.push_back(0x00);
        data.insert(data.end(), parent.key.begin(), parent.key.end());
    } else {
        // Normal derivation: data = serP(point(kpar)) || ser32(i)
        if (parent.isPrivate) {
            // Derive public key from private key using secp256k1
            secp256k1_pubkey pubkey;
            auto* ctx = GetSecp256k1Context();

            if (!secp256k1_ec_pubkey_create(ctx, &pubkey, parent.key.data())) {
                return false;
            }

            // Serialize public key in compressed format (33 bytes)
            uint8_t pubkey_serialized[33];
            size_t pubkey_len = sizeof(pubkey_serialized);
            secp256k1_ec_pubkey_serialize(ctx, pubkey_serialized, &pubkey_len, &pubkey,
                                          SECP256K1_EC_COMPRESSED);

            data.insert(data.end(), pubkey_serialized, pubkey_serialized + pubkey_len);
        } else {
            // Parent is already public key
            data.insert(data.end(), parent.key.begin(), parent.key.end());
        }
    }

    // Append child index as big-endian uint32
    uint8_t index_bytes[4];
    WriteBE32(index_bytes, index);
    data.insert(data.end(), index_bytes, index_bytes + 4);

    // I = HMAC-SHA512(Key = cpar, Data = data)
    std::vector<uint8_t> I;
    if (!HMAC_SHA512(parent.chainCode, data.data(), data.size(), I)) {
        SecureWipeVector(data);
        return false;
    }

    // Split I into IL (32 bytes) and IR (32 bytes)
    std::vector<uint8_t> IL(I.begin(), I.begin() + 32);
    std::vector<uint8_t> IR(I.begin() + 32, I.begin() + 64);

    // Child chain code = IR
    child.chainCode = IR;

    if (parent.isPrivate) {
        // Child private key = parse256(IL) + kpar (mod n)
        child.key = parent.key;  // Start with parent key
        auto* ctx = GetSecp256k1Context();

        // Add IL to parent private key (mod curve order)
        if (!secp256k1_ec_seckey_tweak_add(ctx, child.key.data(), IL.data())) {
            SecureWipeVector(data);
            SecureWipeVector(I);
            SecureWipeVector(IL);
            return false;
        }
        child.isPrivate = true;
    } else {
        // Child public key = point(parse256(IL)) + Kpar
        auto* ctx = GetSecp256k1Context();

        // Parse parent public key
        secp256k1_pubkey parent_pubkey;
        if (!secp256k1_ec_pubkey_parse(ctx, &parent_pubkey, parent.key.data(), parent.key.size())) {
            SecureWipeVector(data);
            SecureWipeVector(I);
            SecureWipeVector(IL);
            return false;
        }

        // Add IL as tweak to parent public key
        if (!secp256k1_ec_pubkey_tweak_add(ctx, &parent_pubkey, IL.data())) {
            SecureWipeVector(data);
            SecureWipeVector(I);
            SecureWipeVector(IL);
            return false;
        }

        // Serialize child public key
        uint8_t child_pubkey_serialized[33];
        size_t child_pubkey_len = sizeof(child_pubkey_serialized);
        secp256k1_ec_pubkey_serialize(ctx, child_pubkey_serialized, &child_pubkey_len,
                                      &parent_pubkey, SECP256K1_EC_COMPRESSED);

        child.key.assign(child_pubkey_serialized, child_pubkey_serialized + child_pubkey_len);
        child.isPrivate = false;
    }

    child.depth = parent.depth + 1;
    child.childNumber = index;

    // Compute parent fingerprint (first 4 bytes of parent public key hash)
    // TODO: Need proper public key serialization
    std::array<uint8_t, 32> parent_hash;
    SHA256(parent.key.data(), parent.key.size(), parent_hash);
    child.fingerprint = ReadBE32(parent_hash.data());

    // Clean up sensitive data
    SecureWipeVector(data);
    SecureWipeVector(I);
    SecureWipeVector(IL);

    // TODO: Verify child key is valid (not zero and less than curve order)

    return true;
}

bool BIP32_DerivePath(const BIP32ExtendedKey& master, const std::string& path,
                      BIP32ExtendedKey& derived) {
    // Parse path like "m/44'/0'/0'/0/0"
    if (path.empty() || path[0] != 'm') {
        return false;
    }

    derived = master;

    // Skip "m" and parse each level
    size_t pos = 1;
    while (pos < path.size()) {
        if (path[pos] == '/') {
            ++pos;
            continue;
        }

        // Parse index number
        uint32_t index = 0;
        bool hardened = false;

        while (pos < path.size() && std::isdigit(path[pos])) {
            index = index * 10 + (path[pos] - '0');
            ++pos;
        }

        // Check for hardened derivation marker
        if (pos < path.size() && (path[pos] == '\'' || path[pos] == 'h')) {
            hardened = true;
            ++pos;
        }

        if (hardened) {
            index |= 0x80000000;  // Set hardened bit
        }

        // Derive child at this level
        BIP32ExtendedKey child;
        if (!BIP32_DeriveChild(derived, index, child)) {
            return false;
        }
        derived = child;
    }

    return true;
}

// Internal helper for address generation with custom version byte
static bool BIP32_GetAddressWithVersion(const BIP32ExtendedKey& extKey, std::string& address,
                                        uint8_t version) {
    std::vector<uint8_t> pubkey;

    if (extKey.isPrivate) {
        // Derive public key from private key using secp256k1
        secp256k1_pubkey pubkey_struct;
        auto* ctx = GetSecp256k1Context();

        if (!secp256k1_ec_pubkey_create(ctx, &pubkey_struct, extKey.key.data())) {
            return false;
        }

        // Serialize public key in compressed format (33 bytes)
        uint8_t pubkey_serialized[33];
        size_t pubkey_len = sizeof(pubkey_serialized);
        secp256k1_ec_pubkey_serialize(ctx, pubkey_serialized, &pubkey_len, &pubkey_struct,
                                      SECP256K1_EC_COMPRESSED);

        pubkey.assign(pubkey_serialized, pubkey_serialized + pubkey_len);
    } else {
        pubkey = extKey.key;
    }

    std::array<uint8_t, 32> sha_hash;
    if (!SHA256(pubkey.data(), pubkey.size(), sha_hash)) {
        return false;
    }

    // RIPEMD-160 hash of SHA256 (Hash160)
    std::array<uint8_t, 20> pubkey_hash;
    if (!RIPEMD160(sha_hash.data(), sha_hash.size(), pubkey_hash)) {
        return false;
    }

    // Add version byte
    std::vector<uint8_t> versioned_hash;
    versioned_hash.reserve(21);
    versioned_hash.push_back(version);
    versioned_hash.insert(versioned_hash.end(), pubkey_hash.begin(), pubkey_hash.end());

    // Base58Check encode
    address = EncodeBase58Check(versioned_hash);

    return !address.empty();
}

bool BIP32_GetBitcoinAddress(const BIP32ExtendedKey& extKey, std::string& address, bool testnet) {
    // Bitcoin version bytes: 0x00 for mainnet P2PKH, 0x6F for testnet P2PKH
    return BIP32_GetAddressWithVersion(extKey, address, testnet ? 0x6F : 0x00);
}

bool BIP32_GetWIF(const BIP32ExtendedKey& extKey, std::string& wif, bool testnet) {
    if (!extKey.isPrivate) {
        return false;  // Can only export private keys
    }

    // WIF format:
    // 1. Version byte (0x80 for mainnet, 0xEF for testnet)
    // 2. Private key (32 bytes)
    // 3. Compression flag (0x01 for compressed)
    // 4. Base58Check encode

    std::vector<uint8_t> data;
    data.push_back(testnet ? 0xEF : 0x80);                          // Version byte
    data.insert(data.end(), extKey.key.begin(), extKey.key.end());  // Private key
    data.push_back(0x01);  // Compression flag (compressed pubkey)

    wif = EncodeBase58Check(data);

    // Securely wipe the data
    SecureWipeVector(data);

    return !wif.empty();
}

// === Transaction Signing Functions ===

bool SignHash(const std::vector<uint8_t>& private_key, const std::array<uint8_t, 32>& hash,
              ECDSASignature& signature) {
    if (private_key.size() != 32) {
        return false;
    }

    auto* ctx = GetSecp256k1Context();

    // Create signature
    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_sign(ctx, &sig, hash.data(), private_key.data(), nullptr, nullptr)) {
        return false;
    }

    // Normalize signature to low-S form (required by Bitcoin)
    secp256k1_ecdsa_signature_normalize(ctx, &sig, &sig);

    // Serialize to DER format
    uint8_t der[74];  // Maximum DER signature size
    size_t der_len = sizeof(der);
    if (!secp256k1_ecdsa_signature_serialize_der(ctx, der, &der_len, &sig)) {
        return false;
    }

    signature.der_encoded.assign(der, der + der_len);

    // Extract R and S components (compact form)
    uint8_t compact[64];
    secp256k1_ecdsa_signature_serialize_compact(ctx, compact, &sig);
    signature.r.assign(compact, compact + 32);
    signature.s.assign(compact + 32, compact + 64);

    return true;
}

bool SignHashRecoverable(const std::vector<uint8_t>& private_key,
                         const std::array<uint8_t, 32>& hash, RecoverableSignature& signature) {
    if (private_key.size() != 32) {
        return false;
    }

    auto* ctx = GetSecp256k1Context();

    // Create recoverable signature
    secp256k1_ecdsa_recoverable_signature sig;
    if (!secp256k1_ecdsa_sign_recoverable(ctx, &sig, hash.data(), private_key.data(), nullptr,
                                          nullptr)) {
        return false;
    }

    // Serialize to compact format (64 bytes) + recovery id
    uint8_t compact[64];
    int recid;
    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, compact, &recid, &sig);

    signature.r.assign(compact, compact + 32);
    signature.s.assign(compact + 32, compact + 64);
    signature.recovery_id = recid;

    return true;
}

bool VerifySignature(const std::vector<uint8_t>& public_key, const std::array<uint8_t, 32>& hash,
                     const ECDSASignature& signature) {
    if (public_key.empty() || signature.der_encoded.empty()) {
        return false;
    }

    auto* ctx = GetSecp256k1Context();

    // Parse public key
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_parse(ctx, &pubkey, public_key.data(), public_key.size())) {
        return false;
    }

    // Parse DER signature
    secp256k1_ecdsa_signature sig;
    if (!secp256k1_ecdsa_signature_parse_der(ctx, &sig, signature.der_encoded.data(),
                                             signature.der_encoded.size())) {
        return false;
    }

    // Verify signature
    return secp256k1_ecdsa_verify(ctx, &sig, hash.data(), &pubkey) == 1;
}

bool DerivePublicKey(const std::vector<uint8_t>& private_key, std::vector<uint8_t>& public_key) {
    if (private_key.size() != 32) {
        return false;
    }

    secp256k1_context* ctx = GetSecp256k1Context();

    // Create public key from private key
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, private_key.data())) {
        return false;
    }

    // Serialize public key (compressed format)
    unsigned char pubkey_serialized[33];
    size_t pubkey_len = 33;
    secp256k1_ec_pubkey_serialize(ctx, pubkey_serialized, &pubkey_len, &pubkey,
                                  SECP256K1_EC_COMPRESSED);

    // Copy to output vector
    public_key.assign(pubkey_serialized, pubkey_serialized + pubkey_len);

    return true;
}

// === Bitcoin Transaction Helper Functions ===

// Helper: Convert hex string to bytes
static std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::strtoul(byte_str.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// Helper: Convert bytes to hex string
static std::string BytesToHex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (uint8_t byte : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

// Helper: Write variable-length integer (VarInt)
static void WriteVarInt(std::vector<uint8_t>& out, uint64_t value) {
    if (value < 0xFD) {
        out.push_back(static_cast<uint8_t>(value));
    } else if (value <= 0xFFFF) {
        out.push_back(0xFD);
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    } else if (value <= 0xFFFFFFFF) {
        out.push_back(0xFE);
        for (int i = 0; i < 4; i++) {
            out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    } else {
        out.push_back(0xFF);
        for (int i = 0; i < 8; i++) {
            out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
}

// Helper: Write uint32 in little-endian
static void WriteUInt32LE(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

// Helper: Write uint64 in little-endian
static void WriteUInt64LE(std::vector<uint8_t>& out, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

bool CreateP2PKHScript(const std::string& address, std::vector<uint8_t>& script) {
    // Decode address to get public key hash
    std::vector<uint8_t> payload;
    if (!DecodeBase58Check(address, payload)) {
        return false;
    }

    // Version 0x00 is Mainnet, 0x6F is Testnet.
    // The payload includes the version byte as the first byte.
    // We need to extract the 20-byte hash (skip the version byte).
    if (payload.empty())
        return false;

    // Basic validation: ensure payload is 21 bytes (1 version + 20 hash)
    if (payload.size() != 21)
        return false;

    std::vector<uint8_t> pubKeyHash(payload.begin() + 1, payload.end());

    // P2PKH script: OP_DUP OP_HASH160 <pubKeyHash> OP_EQUALVERIFY OP_CHECKSIG
    script.clear();
    script.push_back(0x76);  // OP_DUP
    script.push_back(0xA9);  // OP_HASH160
    script.push_back(0x14);  // Push 20 bytes
    script.insert(script.end(), pubKeyHash.begin(), pubKeyHash.end());
    script.push_back(0x88);  // OP_EQUALVERIFY
    script.push_back(0xAC);  // OP_CHECKSIG

    return true;
}

bool CreateTransactionSigHash(const BitcoinTransaction& tx, size_t input_index,
                              const std::string& prev_script_pubkey,
                              std::array<uint8_t, 32>& sighash) {
    if (input_index >= tx.inputs.size()) {
        return false;
    }

    std::vector<uint8_t> serialized;

    // Version
    WriteUInt32LE(serialized, tx.version);

    // Input count
    WriteVarInt(serialized, tx.inputs.size());

    // Inputs
    for (size_t i = 0; i < tx.inputs.size(); i++) {
        const auto& input = tx.inputs[i];

        // Previous output (txid + vout)
        std::vector<uint8_t> txid_bytes = HexToBytes(input.txid);
        std::reverse(txid_bytes.begin(), txid_bytes.end());  // Reverse for little-endian
        serialized.insert(serialized.end(), txid_bytes.begin(), txid_bytes.end());
        WriteUInt32LE(serialized, input.vout);

        // Script
        if (i == input_index) {
            // For the input being signed, use prev_script_pubkey
            std::vector<uint8_t> script_bytes = HexToBytes(prev_script_pubkey);
            WriteVarInt(serialized, script_bytes.size());
            serialized.insert(serialized.end(), script_bytes.begin(), script_bytes.end());
        } else {
            // For other inputs, use empty script
            WriteVarInt(serialized, 0);
        }

        // Sequence
        WriteUInt32LE(serialized, input.sequence);
    }

    // Output count
    WriteVarInt(serialized, tx.outputs.size());

    // Outputs
    for (const auto& output : tx.outputs) {
        WriteUInt64LE(serialized, output.amount);
        std::vector<uint8_t> script_bytes = HexToBytes(output.script_pubkey);
        WriteVarInt(serialized, script_bytes.size());
        serialized.insert(serialized.end(), script_bytes.begin(), script_bytes.end());
    }

    // Locktime
    WriteUInt32LE(serialized, tx.locktime);

    // Sighash type (SIGHASH_ALL = 0x01)
    WriteUInt32LE(serialized, 0x00000001);

    // Double SHA256 to get sighash
    std::array<uint8_t, 32> hash1, hash2;
    if (!SHA256(serialized.data(), serialized.size(), hash1)) {
        return false;
    }
    if (!SHA256(hash1.data(), hash1.size(), hash2)) {
        return false;
    }

    sighash = hash2;
    return true;
}

bool SignTransactionInput(BitcoinTransaction& tx, size_t input_index,
                          const std::vector<uint8_t>& private_key,
                          const std::vector<uint8_t>& public_key,
                          const std::string& prev_script_pubkey) {
    if (input_index >= tx.inputs.size()) {
        return false;
    }

    // Create sighash
    std::array<uint8_t, 32> sighash;
    if (!CreateTransactionSigHash(tx, input_index, prev_script_pubkey, sighash)) {
        return false;
    }

    // Sign the hash
    ECDSASignature signature;
    if (!SignHash(private_key, sighash, signature)) {
        return false;
    }

    // Create scriptSig: <signature> <pubkey>
    std::vector<uint8_t> script_sig;

    // Push signature (with SIGHASH_ALL byte)
    std::vector<uint8_t> sig_with_hashtype = signature.der_encoded;
    sig_with_hashtype.push_back(0x01);  // SIGHASH_ALL
    script_sig.push_back(static_cast<uint8_t>(sig_with_hashtype.size()));
    script_sig.insert(script_sig.end(), sig_with_hashtype.begin(), sig_with_hashtype.end());

    // Push public key
    script_sig.push_back(static_cast<uint8_t>(public_key.size()));
    script_sig.insert(script_sig.end(), public_key.begin(), public_key.end());

    // Convert to hex and update input
    tx.inputs[input_index].script_sig = BytesToHex(script_sig);

    return true;
}

bool SerializeTransaction(const BitcoinTransaction& tx, std::string& raw_hex) {
    std::vector<uint8_t> serialized;

    // Version
    WriteUInt32LE(serialized, tx.version);

    // Input count
    WriteVarInt(serialized, tx.inputs.size());

    // Inputs
    for (const auto& input : tx.inputs) {
        // Previous output (txid + vout)
        std::vector<uint8_t> txid_bytes = HexToBytes(input.txid);
        std::reverse(txid_bytes.begin(), txid_bytes.end());  // Reverse for little-endian
        serialized.insert(serialized.end(), txid_bytes.begin(), txid_bytes.end());
        WriteUInt32LE(serialized, input.vout);

        // Script
        std::vector<uint8_t> script_bytes = HexToBytes(input.script_sig);
        WriteVarInt(serialized, script_bytes.size());
        serialized.insert(serialized.end(), script_bytes.begin(), script_bytes.end());

        // Sequence
        WriteUInt32LE(serialized, input.sequence);
    }

    // Output count
    WriteVarInt(serialized, tx.outputs.size());

    // Outputs
    for (const auto& output : tx.outputs) {
        WriteUInt64LE(serialized, output.amount);
        std::vector<uint8_t> script_bytes = HexToBytes(output.script_pubkey);
        WriteVarInt(serialized, script_bytes.size());
        serialized.insert(serialized.end(), script_bytes.begin(), script_bytes.end());
    }

    // Locktime
    WriteUInt32LE(serialized, tx.locktime);

    raw_hex = BytesToHex(serialized);
    return true;
}

bool CalculateTransactionID(const std::string& raw_hex, std::string& txid) {
    std::vector<uint8_t> raw_bytes = HexToBytes(raw_hex);

    // Double SHA256
    std::array<uint8_t, 32> hash1, hash2;
    if (!SHA256(raw_bytes.data(), raw_bytes.size(), hash1)) {
        return false;
    }
    if (!SHA256(hash1.data(), hash1.size(), hash2)) {
        return false;
    }

    // Reverse for display (Bitcoin displays txid in reverse byte order)
    std::vector<uint8_t> reversed(hash2.rbegin(), hash2.rend());
    txid = BytesToHex(reversed);

    return true;
}

// === BIP44 Helper Functions ===

bool BIP44_DeriveAddressKey(const BIP32ExtendedKey& master, uint32_t account, bool change,
                            uint32_t address_index, BIP32ExtendedKey& address_key, bool testnet) {
    // BIP44 path: m / purpose' / coin_type' / account' / change / address_index
    // purpose = 44' (hardened)
    // coin_type = 0' for Bitcoin mainnet, 1' for testnet (hardened)
    // account = account' (hardened)
    // change = 0 for receiving, 1 for change (not hardened)
    // address_index = index (not hardened)

    std::ostringstream path_builder;
    path_builder << "m/44'/";
    path_builder << (testnet ? "1'" : "0'") << "/";
    path_builder << account << "'/";
    path_builder << (change ? "1" : "0") << "/";
    path_builder << address_index;

    std::string path = path_builder.str();
    return BIP32_DerivePath(master, path, address_key);
}

bool BIP44_GetAddress(const BIP32ExtendedKey& master, uint32_t account, bool change,
                      uint32_t address_index, std::string& address, bool testnet) {
    BIP32ExtendedKey address_key;
    if (!BIP44_DeriveAddressKey(master, account, change, address_index, address_key, testnet)) {
        return false;
    }

    return BIP32_GetBitcoinAddress(address_key, address, testnet);
}

bool BIP44_GenerateAddresses(const BIP32ExtendedKey& master, uint32_t account, bool change,
                             uint32_t start_index, uint32_t count,
                             std::vector<std::string>& addresses, bool testnet) {
    addresses.clear();
    addresses.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        std::string address;
        if (!BIP44_GetAddress(master, account, change, start_index + i, address, testnet)) {
            return false;
        }
        addresses.push_back(address);
    }

    return true;
}

// === UTXO Management Functions ===

bool SelectCoins(const std::vector<UTXO>& available_utxos, uint64_t target_amount,
                 uint64_t fee_per_byte, CoinSelection& selection) {
    // Simple coin selection: Largest first algorithm
    // Sort UTXOs by amount (largest first)
    std::vector<UTXO> sorted_utxos = available_utxos;
    std::sort(sorted_utxos.begin(), sorted_utxos.end(), [](const UTXO& a, const UTXO& b) {
        return a.amount > b.amount;
    });

    selection.selected_utxos.clear();
    selection.total_input = 0;
    selection.target_amount = target_amount;
    selection.fee = 0;
    selection.change_amount = 0;
    selection.has_change = false;

    // Keep adding UTXOs until we have enough
    for (const auto& utxo : sorted_utxos) {
        selection.selected_utxos.push_back(utxo);
        selection.total_input += utxo.amount;

        // Estimate fee with current number of inputs and outputs
        size_t output_count = 1;  // At least one output (recipient)
        selection.fee = CalculateFee(selection.selected_utxos.size(), output_count, fee_per_byte);

        uint64_t required_total = target_amount + selection.fee;

        if (selection.total_input >= required_total) {
            // We have enough! Check if we need change output
            selection.change_amount = selection.total_input - required_total;

            // Only create change output if it's economical (dust threshold ~546 satoshis)
            const uint64_t DUST_THRESHOLD = 546;
            if (selection.change_amount >= DUST_THRESHOLD) {
                selection.has_change = true;
                output_count = 2;  // Add change output
                // Recalculate fee with change output
                selection.fee =
                    CalculateFee(selection.selected_utxos.size(), output_count, fee_per_byte);
                selection.change_amount = selection.total_input - target_amount - selection.fee;
            } else {
                // Change too small, add it to fee
                selection.has_change = false;
                selection.change_amount = 0;
            }

            return true;
        }
    }

    // Not enough funds
    return false;
}

uint64_t EstimateTransactionSize(size_t input_count, size_t output_count) {
    // Rough estimate of transaction size in bytes
    // Version: 4 bytes
    // Input count: ~1 byte (VarInt)
    // Inputs: ~148 bytes each (txid=32 + vout=4 + script_sig=~107 + sequence=4)
    // Output count: ~1 byte (VarInt)
    // Outputs: ~34 bytes each (amount=8 + script_pubkey=~26)
    // Locktime: 4 bytes

    uint64_t size = 4;          // Version
    size += 1;                  // Input count
    size += input_count * 148;  // Inputs
    size += 1;                  // Output count
    size += output_count * 34;  // Outputs
    size += 4;                  // Locktime

    return size;
}

uint64_t CalculateFee(size_t input_count, size_t output_count, uint64_t fee_per_byte) {
    uint64_t tx_size = EstimateTransactionSize(input_count, output_count);
    return tx_size * fee_per_byte;
}

bool CreateUnsignedTransaction(const std::vector<UTXO>& inputs,
                               const std::string& recipient_address, uint64_t send_amount,
                               const std::string& change_address, uint64_t change_amount,
                               BitcoinTransaction& tx) {
    tx.version = 1;
    tx.locktime = 0;
    tx.inputs.clear();
    tx.outputs.clear();

    // Add inputs
    for (const auto& utxo : inputs) {
        TransactionInput input;
        input.txid = utxo.txid;
        input.vout = utxo.vout;
        input.script_sig = "";        // Empty for unsigned transaction
        input.sequence = 0xFFFFFFFF;  // Final
        tx.inputs.push_back(input);
    }

    // Add recipient output
    TransactionOutput recipient_output;
    recipient_output.amount = send_amount;
    recipient_output.address = recipient_address;

    // Create P2PKH script for recipient
    std::vector<uint8_t> recipient_script;
    if (!CreateP2PKHScript(recipient_address, recipient_script)) {
        return false;
    }
    recipient_output.script_pubkey = BytesToHex(recipient_script);
    tx.outputs.push_back(recipient_output);

    // Add change output if needed
    if (change_amount > 0 && !change_address.empty()) {
        TransactionOutput change_output;
        change_output.amount = change_amount;
        change_output.address = change_address;

        std::vector<uint8_t> change_script;
        if (!CreateP2PKHScript(change_address, change_script)) {
            return false;
        }
        change_output.script_pubkey = BytesToHex(change_script);
        tx.outputs.push_back(change_output);
    }

    return true;
}

// === Ethereum Address Generation ===

// === EIP-55 Checksum Implementation ===

bool EIP55_ToChecksumAddress(const std::string& address, std::string& checksummed) {
    // Remove 0x prefix if present
    std::string addr = address;
    if (addr.size() >= 2 && addr[0] == '0' && (addr[1] == 'x' || addr[1] == 'X')) {
        addr = addr.substr(2);
    }

    // Validate address length (should be 40 hex characters)
    if (addr.size() != 40) {
        return false;
    }

    // Validate hex characters
    for (char c : addr) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    // Convert to lowercase for hashing
    std::string lowercase_addr = addr;
    std::transform(lowercase_addr.begin(), lowercase_addr.end(), lowercase_addr.begin(),
                   [](unsigned char c) {
                       return std::tolower(c);
                   });

    // Hash the lowercase address
    std::array<uint8_t, 32> hash;
    if (!Keccak256(reinterpret_cast<const uint8_t*>(lowercase_addr.c_str()), lowercase_addr.size(),
                   hash)) {
        return false;
    }

    // Apply EIP-55 checksum: capitalize letters where hash nibble >= 8
    std::ostringstream oss;
    oss << "0x";

    for (size_t i = 0; i < lowercase_addr.size(); i++) {
        char c = lowercase_addr[i];

        // Only apply checksum to letters (a-f), not digits (0-9)
        if (std::isalpha(static_cast<unsigned char>(c))) {
            // Get the corresponding nibble from the hash
            size_t hash_byte_index = i / 2;
            uint8_t hash_byte = hash[hash_byte_index];

            // Get the nibble (high or low 4 bits)
            uint8_t nibble = (i % 2 == 0) ? (hash_byte >> 4) : (hash_byte & 0x0F);

            // Capitalize if nibble >= 8
            if (nibble >= 8) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
        }

        oss << c;
    }

    checksummed = oss.str();
    return true;
}

bool EIP55_ValidateChecksumAddress(const std::string& address) {
    // Remove 0x prefix if present
    std::string addr = address;
    if (addr.size() >= 2 && addr[0] == '0' && (addr[1] == 'x' || addr[1] == 'X')) {
        addr = addr.substr(2);
    }

    // Validate address length
    if (addr.size() != 40) {
        return false;
    }

    // Check if all lowercase or all uppercase (valid but unchecksummed)
    bool all_lower = true;
    bool all_upper = true;
    for (char c : addr) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            if (std::isupper(static_cast<unsigned char>(c))) {
                all_lower = false;
            } else {
                all_upper = false;
            }
        }
    }

    // If all lowercase or all uppercase, accept it (unchecksummed addresses are valid)
    if (all_lower || all_upper) {
        return true;
    }

    // Mixed case: verify EIP-55 checksum
    std::string checksummed;
    if (!EIP55_ToChecksumAddress(addr, checksummed)) {
        return false;
    }

    // Remove 0x prefix from checksummed address for comparison
    if (checksummed.size() >= 2 && checksummed[0] == '0' && checksummed[1] == 'x') {
        checksummed = checksummed.substr(2);
    }

    // Compare addresses
    return addr == checksummed;
}

bool BIP32_GetEthereumAddress(const BIP32ExtendedKey& extKey, std::string& address) {
    // Ethereum address generation:
    // 1. Get public key (uncompressed, 64 bytes without 0x04 prefix)
    // 2. Keccak256 hash the public key
    // 3. Take last 20 bytes of hash
    // 4. Convert to hex with 0x prefix

    std::vector<uint8_t> pubkey;

    if (extKey.isPrivate) {
        // Derive public key from private key using secp256k1
        secp256k1_pubkey pubkey_struct;
        auto* ctx = GetSecp256k1Context();

        if (!secp256k1_ec_pubkey_create(ctx, &pubkey_struct, extKey.key.data())) {
            return false;
        }

        // Serialize public key in UNCOMPRESSED format (65 bytes: 0x04 + x + y)
        uint8_t pubkey_serialized[65];
        size_t pubkey_len = sizeof(pubkey_serialized);
        secp256k1_ec_pubkey_serialize(ctx, pubkey_serialized, &pubkey_len, &pubkey_struct,
                                      SECP256K1_EC_UNCOMPRESSED);

        // Ethereum uses only the x and y coordinates (skip the 0x04 prefix)
        pubkey.assign(pubkey_serialized + 1, pubkey_serialized + 65);
    } else {
        // If already public key, need to convert to uncompressed format
        auto* ctx = GetSecp256k1Context();
        secp256k1_pubkey pubkey_struct;

        if (!secp256k1_ec_pubkey_parse(ctx, &pubkey_struct, extKey.key.data(), extKey.key.size())) {
            return false;
        }

        // Serialize to uncompressed format
        uint8_t pubkey_serialized[65];
        size_t pubkey_len = sizeof(pubkey_serialized);
        secp256k1_ec_pubkey_serialize(ctx, pubkey_serialized, &pubkey_len, &pubkey_struct,
                                      SECP256K1_EC_UNCOMPRESSED);

        // Skip the 0x04 prefix
        pubkey.assign(pubkey_serialized + 1, pubkey_serialized + 65);
    }

    // Keccak256 hash of public key (64 bytes)
    std::array<uint8_t, 32> hash;
    if (!Keccak256(pubkey.data(), pubkey.size(), hash)) {
        return false;
    }

    // Take last 20 bytes of hash (generate lowercase address first)
    std::ostringstream oss;
    oss << "0x";
    for (size_t i = 12; i < 32; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    std::string lowercase_address = oss.str();

    // Apply EIP-55 checksum to the address
    if (!EIP55_ToChecksumAddress(lowercase_address, address)) {
        return false;
    }

    return !address.empty();
}

bool BIP44_DeriveEthereumAddressKey(const BIP32ExtendedKey& master, uint32_t account, bool change,
                                    uint32_t address_index, BIP32ExtendedKey& address_key) {
    // Ethereum BIP44 path: m/44'/60'/account'/change/address_index
    // 60 is the coin type for Ethereum
    std::ostringstream path_builder;
    path_builder << "m/44'/60'/" << account << "'/" << (change ? "1" : "0") << "/" << address_index;

    std::string path = path_builder.str();
    return BIP32_DerivePath(master, path, address_key);
}

bool BIP44_GetEthereumAddress(const BIP32ExtendedKey& master, uint32_t account, bool change,
                              uint32_t address_index, std::string& address) {
    BIP32ExtendedKey address_key;
    if (!BIP44_DeriveEthereumAddressKey(master, account, change, address_index, address_key)) {
        return false;
    }

    return BIP32_GetEthereumAddress(address_key, address);
}

bool BIP44_GenerateEthereumAddresses(const BIP32ExtendedKey& master, uint32_t account, bool change,
                                     uint32_t start_index, uint32_t count,
                                     std::vector<std::string>& addresses) {
    addresses.clear();
    addresses.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        std::string address;
        if (!BIP44_GetEthereumAddress(master, account, change, start_index + i, address)) {
            return false;
        }
        addresses.push_back(address);
    }

    return true;
}

// === Multi-Chain Helper Functions ===

uint32_t GetCoinType(ChainType chain) {
    switch (chain) {
        case ChainType::BITCOIN:
            return 0;
        case ChainType::BITCOIN_TESTNET:
            return 1;
        case ChainType::LITECOIN:
            return 2;  // BIP44 coin type for Litecoin
        case ChainType::LITECOIN_TESTNET:
            return 1;  // Testnet uses coin type 1
        case ChainType::ETHEREUM:
        case ChainType::ETHEREUM_TESTNET:
            return 60;
        case ChainType::BNB_CHAIN:
            return 714;
        case ChainType::POLYGON:
            return 966;
        case ChainType::AVALANCHE:
            return 9000;  // Avalanche C-Chain uses Ethereum derivation path
        case ChainType::ARBITRUM:
        case ChainType::OPTIMISM:
        case ChainType::BASE:
            return 60;  // EVM-compatible chains use Ethereum coin type
        default:
            return 0;
    }
}

std::string GetChainName(ChainType chain) {
    switch (chain) {
        case ChainType::BITCOIN:
            return "Bitcoin";
        case ChainType::BITCOIN_TESTNET:
            return "Bitcoin Testnet";
        case ChainType::LITECOIN:
            return "Litecoin";
        case ChainType::LITECOIN_TESTNET:
            return "Litecoin Testnet";
        case ChainType::ETHEREUM:
            return "Ethereum";
        case ChainType::ETHEREUM_TESTNET:
            return "Ethereum Testnet";
        case ChainType::BNB_CHAIN:
            return "BNB Chain";
        case ChainType::POLYGON:
            return "Polygon";
        case ChainType::AVALANCHE:
            return "Avalanche C-Chain";
        case ChainType::ARBITRUM:
            return "Arbitrum";
        case ChainType::OPTIMISM:
            return "Optimism";
        case ChainType::BASE:
            return "Base";
        default:
            return "Unknown";
    }
}

bool DeriveChainAddress(const BIP32ExtendedKey& master, ChainType chain, uint32_t account,
                        bool change, uint32_t address_index, std::string& address) {
    // Determine if this is a Bitcoin-like chain or EVM-compatible chain
    switch (chain) {
        case ChainType::BITCOIN:
            return BIP44_GetAddress(master, account, change, address_index, address, false);

        case ChainType::BITCOIN_TESTNET:
            return BIP44_GetAddress(master, account, change, address_index, address, true);

        case ChainType::LITECOIN: {
            BIP32ExtendedKey address_key;
            if (!BIP44_DeriveAddressKey(master, account, change, address_index, address_key,
                                        false)) {
                return false;
            }
            // Litecoin mainnet P2PKH version byte is 0x30 (starts with 'L')
            return BIP32_GetAddressWithVersion(address_key, address, 0x30);
        }

        case ChainType::LITECOIN_TESTNET: {
            BIP32ExtendedKey address_key;
            if (!BIP44_DeriveAddressKey(master, account, change, address_index, address_key,
                                        true)) {
                return false;
            }
            // Litecoin testnet P2PKH version byte is 0x6F (starts with 'm' or 'n')
            return BIP32_GetAddressWithVersion(address_key, address, 0x6F);
        }

        case ChainType::ETHEREUM:
        case ChainType::ETHEREUM_TESTNET:
        case ChainType::BNB_CHAIN:
        case ChainType::POLYGON:
        case ChainType::AVALANCHE:
        case ChainType::ARBITRUM:
        case ChainType::OPTIMISM:
        case ChainType::BASE:
            // All EVM-compatible chains use Ethereum address format
            return BIP44_GetEthereumAddress(master, account, change, address_index, address);

        default:
            return false;
    }
}

// === Chain-Aware Address Validation ===

bool IsEVMChain(ChainType chain) {
    switch (chain) {
        case ChainType::ETHEREUM:
        case ChainType::ETHEREUM_TESTNET:
        case ChainType::BNB_CHAIN:
        case ChainType::POLYGON:
        case ChainType::AVALANCHE:
        case ChainType::ARBITRUM:
        case ChainType::OPTIMISM:
        case ChainType::BASE:
            return true;
        default:
            return false;
    }
}

bool IsBitcoinChain(ChainType chain) {
    switch (chain) {
        case ChainType::BITCOIN:
        case ChainType::BITCOIN_TESTNET:
        case ChainType::LITECOIN:
        case ChainType::LITECOIN_TESTNET:
            return true;
        default:
            return false;
    }
}

bool IsValidAddressFormat(const std::string& address, ChainType chain) {
    if (IsEVMChain(chain)) {
        // EVM address validation: 0x + 40 hex characters
        if (address.size() < 2 || address[0] != '0' || (address[1] != 'x' && address[1] != 'X')) {
            return false;
        }

        std::string addr = address.substr(2);
        if (addr.size() != 40) {
            return false;
        }

        // Validate hex characters
        for (char c : addr) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }

        // Validate EIP-55 checksum if address has mixed case
        return EIP55_ValidateChecksumAddress(address);
    } else if (IsBitcoinChain(chain)) {
        // Bitcoin-like address validation
        if (address.empty()) {
            return false;
        }

        // Check for Bech32 (SegWit) addresses - they are longer (up to 90 chars, typically 42-62)
        bool isSegWit = (address.substr(0, 3) == "bc1" || address.substr(0, 4) == "ltc1" ||
                         address.substr(0, 3) == "tb1" || address.substr(0, 4) == "tltc");

        if (isSegWit) {
            if (address.size() < 42 || address.size() > 90) {
                return false;
            }
        } else {
            // Standard Base58Check addresses are typically 26-35 characters
            if (address.size() < 26 || address.size() > 35) {
                return false;
            }
        }

        // Bitcoin addresses typically start with 1, 3, or bc1 (mainnet) or m, n, 2, tb1 (testnet)
        char first = address[0];
        if (chain == ChainType::BITCOIN) {
            if (first != '1' && first != '3' && address.substr(0, 3) != "bc1") {
                return false;
            }
        } else if (chain == ChainType::BITCOIN_TESTNET) {
            if (first != 'm' && first != 'n' && first != '2' && address.substr(0, 3) != "tb1") {
                return false;
            }
        } else if (chain == ChainType::LITECOIN) {
            if (first != 'L' && first != 'M' && first != '3' && address.substr(0, 4) != "ltc1") {
                return false;
            }
        } else if (chain == ChainType::LITECOIN_TESTNET) {
            if (first != 'm' && first != 'n' && first != '2' && first != 'Q' &&
                address.substr(0, 4) != "tltc") {
                return false;
            }
        }

        // Check for valid Base58 characters (no 0, O, I, l)
        // Only apply to non-SegWit addresses (SegWit uses Bech32)
        if (!isSegWit) {
            const std::string base58_chars =
                "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
            for (char c : address) {
                if (base58_chars.find(c) == std::string::npos) {
                    return false;
                }
            }
        }

        return true;
    }

    return false;
}

std::optional<ChainType> DetectAddressChain(const std::string& address) {
    if (address.empty()) {
        return std::nullopt;
    }

    // Check for EVM address (0x + 40 hex characters)
    if (address.size() == 42 && address[0] == '0' && (address[1] == 'x' || address[1] == 'X')) {
        std::string addr = address.substr(2);
        bool valid_hex = true;
        for (char c : addr) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                valid_hex = false;
                break;
            }
        }

        if (valid_hex) {
            // Validate EIP-55 checksum
            if (EIP55_ValidateChecksumAddress(address)) {
                // Default to Ethereum for valid EVM addresses
                // Note: Cannot distinguish between different EVM chains by address alone
                return ChainType::ETHEREUM;
            }
        }
    }

    // Check for Bitcoin mainnet addresses
    if (address.size() >= 26 && address.size() <= 35) {
        char first = address[0];
        if (first == '1' || first == '3') {
            return ChainType::BITCOIN;
        } else if (address.substr(0, 3) == "bc1") {
            return ChainType::BITCOIN;
        } else if (first == 'm' || first == 'n' || first == '2') {
            return ChainType::BITCOIN_TESTNET;
        } else if (address.substr(0, 3) == "tb1") {
            return ChainType::BITCOIN_TESTNET;
        }
    }

    return std::nullopt;
}

// === TOTP (Time-based One-Time Password) Functions ===

bool HMAC_SHA1(const std::vector<uint8_t>& key, const uint8_t* data, size_t data_len,
               std::vector<uint8_t>& out) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, nullptr,
                                                  BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status))
        return false;

    // SHA1 produces 20 bytes
    out.resize(20);
    BCRYPT_HASH_HANDLE hHash = nullptr;
    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    status = BCryptHashData(hHash, (PUCHAR)data, (ULONG)data_len, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    status = BCryptFinishHash(hHash, out.data(), (ULONG)out.size(), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(status);
#else
    out.resize(20);
    unsigned int outLen = 0;
    if (!HMAC(EVP_sha1(), key.data(), key.size(), data, data_len, out.data(), &outLen)) {
        return false;
    }
    return true;
#endif
}

bool GenerateTOTPSecret(std::vector<uint8_t>& secret, size_t length) {
    secret.resize(length);
    return RandBytes(secret.data(), length);
}

// Base32 alphabet (RFC 4648)
static const char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

std::string Base32Encode(const std::vector<uint8_t>& data) {
    if (data.empty())
        return "";

    std::string result;
    result.reserve((data.size() * 8 + 4) / 5);

    int buffer = 0;
    int bitsLeft = 0;

    for (uint8_t byte : data) {
        buffer = (buffer << 8) | byte;
        bitsLeft += 8;
        while (bitsLeft >= 5) {
            bitsLeft -= 5;
            result += BASE32_ALPHABET[(buffer >> bitsLeft) & 0x1F];
        }
    }

    if (bitsLeft > 0) {
        result += BASE32_ALPHABET[(buffer << (5 - bitsLeft)) & 0x1F];
    }

    // Add padding to make length multiple of 8
    while (result.size() % 8 != 0) {
        result += '=';
    }

    return result;
}

std::vector<uint8_t> Base32Decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    if (encoded.empty())
        return result;

    int buffer = 0;
    int bitsLeft = 0;

    for (char c : encoded) {
        if (c == '=' || c == ' ')
            continue;  // Skip padding and spaces

        // Find character in alphabet
        int value = -1;
        if (c >= 'A' && c <= 'Z') {
            value = c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            value = c - 'a';  // Case insensitive
        } else if (c >= '2' && c <= '7') {
            value = c - '2' + 26;
        }

        if (value < 0)
            continue;  // Invalid character

        buffer = (buffer << 5) | value;
        bitsLeft += 5;

        if (bitsLeft >= 8) {
            bitsLeft -= 8;
            result.push_back(static_cast<uint8_t>((buffer >> bitsLeft) & 0xFF));
        }
    }

    return result;
}

std::string GenerateTOTP(const std::vector<uint8_t>& secret, uint64_t timestamp, uint32_t timestep,
                         uint32_t digits) {
    // Use current time if timestamp not specified
    if (timestamp == 0) {
        timestamp = static_cast<uint64_t>(std::time(nullptr));
    }

    // Calculate time counter (number of time steps since Unix epoch)
    uint64_t counter = timestamp / timestep;

    // Convert counter to big-endian bytes
    uint8_t counterBytes[8];
    for (int i = 7; i >= 0; i--) {
        counterBytes[i] = static_cast<uint8_t>(counter & 0xFF);
        counter >>= 8;
    }

    // Calculate HMAC-SHA1
    std::vector<uint8_t> hmacResult;
    if (!HMAC_SHA1(secret, counterBytes, 8, hmacResult)) {
        return "";
    }

    // Dynamic truncation (RFC 4226)
    int offset = hmacResult[19] & 0x0F;
    uint32_t binary = ((hmacResult[offset] & 0x7F) << 24) |
                      ((hmacResult[offset + 1] & 0xFF) << 16) |
                      ((hmacResult[offset + 2] & 0xFF) << 8) | (hmacResult[offset + 3] & 0xFF);

    // Generate OTP of specified digits
    uint32_t otp = binary % static_cast<uint32_t>(std::pow(10, digits));

    // Format with leading zeros
    std::ostringstream oss;
    oss << std::setw(digits) << std::setfill('0') << otp;
    return oss.str();
}

bool VerifyTOTP(const std::vector<uint8_t>& secret, const std::string& code, uint32_t window,
                uint32_t timestep, uint32_t digits) {
    uint64_t currentTime = static_cast<uint64_t>(std::time(nullptr));

    // Check codes within the time window
    for (int i = -static_cast<int>(window); i <= static_cast<int>(window); i++) {
        uint64_t checkTime = currentTime + (i * timestep);
        std::string expectedCode = GenerateTOTP(secret, checkTime, timestep, digits);

        // Constant-time comparison to prevent timing attacks
        if (code.length() == expectedCode.length()) {
            bool match = true;
            for (size_t j = 0; j < code.length(); j++) {
                if (code[j] != expectedCode[j])
                    match = false;
            }
            if (match)
                return true;
        }
    }

    return false;
}

std::string GenerateTOTPUri(const std::string& secret_base32, const std::string& account_name,
                            const std::string& issuer) {
    std::ostringstream uri;
    uri << "otpauth://totp/" << issuer << ":" << account_name << "?secret=" << secret_base32
        << "&issuer=" << issuer << "&algorithm=SHA1&digits=6&period=30";
    return uri.str();
}

// === Secure Random String Generation ===
std::string GenerateSecureRandomString(size_t byteLength) {
    std::vector<uint8_t> buffer(byteLength);
    if (!RandBytes(buffer.data(), buffer.size())) {
        // In a real application, you might want to throw an exception here
        // or have a more robust error handling mechanism.
        return "";
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto& byte : buffer) {
        oss << std::setw(2) << static_cast<int>(byte);
    }

    return oss.str();
}

}  // namespace Crypto

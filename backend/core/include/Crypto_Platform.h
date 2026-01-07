#pragma once
// Platform-specific cryptographic implementations
// Windows: BCrypt, DPAPI
// macOS: CommonCrypto, Keychain

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Crypto {
namespace Platform {

// === Random Number Generation ===
// Windows: BCryptGenRandom
// macOS: CCRandomGenerateBytes
bool RandBytes(void* buf, size_t len);

// === Hash Functions ===
// Windows: BCrypt SHA256
// macOS: CC_SHA256
bool SHA256(const uint8_t* data, size_t len, uint8_t* out);

// === HMAC Functions ===
// Windows: BCrypt HMAC
// macOS: CCHmac

// HMAC-SHA256 (32-byte output)
bool HMAC_SHA256(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
                 uint8_t* out);

// HMAC-SHA512 (64-byte output)
bool HMAC_SHA512(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
                 uint8_t* out);

// HMAC-SHA1 (20-byte output) - Required for TOTP (RFC 6238)
bool HMAC_SHA1(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
               uint8_t* out);

// === AES-GCM Encryption ===
// Windows: BCrypt AES-GCM
// macOS: CCCryptorGCM

// Encrypt with AES-256-GCM
// key: 32 bytes, iv: 12 bytes (output), tag: 16 bytes (output)
bool AES_GCM_Encrypt(const uint8_t* key, size_t key_len, const uint8_t* plaintext,
                     size_t plaintext_len, const uint8_t* aad, size_t aad_len, uint8_t* iv,
                     size_t iv_len, uint8_t* ciphertext, uint8_t* tag, size_t tag_len);

// Decrypt with AES-256-GCM
bool AES_GCM_Decrypt(const uint8_t* key, size_t key_len, const uint8_t* ciphertext,
                     size_t ciphertext_len, const uint8_t* aad, size_t aad_len, const uint8_t* iv,
                     size_t iv_len, const uint8_t* tag, size_t tag_len, uint8_t* plaintext);

// === Secure Storage (OS-protected encryption) ===
// Windows: DPAPI (CryptProtectData/CryptUnprotectData)
// macOS: Keychain Services

// Protect data using OS-level encryption tied to user account
// identifier: unique key for this data (e.g., "seed::username")
bool SecureProtect(const std::vector<uint8_t>& plaintext, const std::string& identifier,
                   std::vector<uint8_t>& ciphertext);

// Unprotect data previously protected with SecureProtect
bool SecureUnprotect(const std::vector<uint8_t>& ciphertext, const std::string& identifier,
                     std::vector<uint8_t>& plaintext);

// Delete securely stored data
bool SecureDelete(const std::string& identifier);

// Check if secure data exists for identifier
bool SecureExists(const std::string& identifier);

}  // namespace Platform
}  // namespace Crypto

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Crypto {

// === Random Number Generation ===
bool RandBytes(void *buf, size_t len);

// === Base64 Encoding/Decoding ===
std::string B64Encode(const std::vector<uint8_t> &data);
std::vector<uint8_t> B64Decode(const std::string &s);

// === Hash Functions ===
bool SHA256(const uint8_t *data, size_t len, std::array<uint8_t, 32> &out);
bool HMAC_SHA256(const std::vector<uint8_t> &key, const uint8_t *data, size_t data_len, std::vector<uint8_t> &out);
bool HMAC_SHA512(const std::vector<uint8_t> &key, const uint8_t *data, size_t data_len, std::vector<uint8_t> &out);

// === Key Derivation Functions ===
bool PBKDF2_HMAC_SHA256(const std::string &password, const uint8_t *salt, size_t salt_len, 
                        uint32_t iterations, std::vector<uint8_t> &out_key, size_t dk_len = 32);
bool PBKDF2_HMAC_SHA512(const std::string &password, const uint8_t *salt, size_t salt_len, 
                        uint32_t iterations, std::vector<uint8_t> &out_key, size_t dk_len = 64);

// === Windows DPAPI Functions ===
bool DPAPI_Protect(const std::vector<uint8_t> &plaintext, const std::string &entropy_str, std::vector<uint8_t> &ciphertext);
bool DPAPI_Unprotect(const std::vector<uint8_t> &ciphertext, const std::string &entropy_str, std::vector<uint8_t> &plaintext);

// === BIP-39 Cryptographic Functions ===
bool GenerateEntropy(size_t bits, std::vector<uint8_t> &out);
bool MnemonicFromEntropy(const std::vector<uint8_t> &entropy, const std::vector<std::string> &wordlist, std::vector<std::string> &outMnemonic);
bool ValidateMnemonic(const std::vector<std::string> &mnemonic, const std::vector<std::string> &wordlist);
bool BIP39_SeedFromMnemonic(const std::vector<std::string> &mnemonic, const std::string &passphrase, std::array<uint8_t, 64> &outSeed);

// === Utility Functions ===
bool ConstantTimeEquals(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b);

// === Memory Security Functions ===
void SecureZeroMemory(void *ptr, size_t size);
void SecureWipeVector(std::vector<uint8_t> &vec);
void SecureWipeString(std::string &str);

// === Secure Key Derivation ===
bool DeriveWalletKey(const std::string &password, const std::vector<uint8_t> &salt,
                     std::vector<uint8_t> &derived_key, size_t key_length = 32);
bool DeriveDBEncryptionKey(const std::string &password, const std::vector<uint8_t> &salt,
                          std::vector<uint8_t> &db_key);

// === AES-GCM Encryption/Decryption ===
bool AES_GCM_Encrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> &plaintext,
                     const std::vector<uint8_t> &aad, std::vector<uint8_t> &ciphertext,
                     std::vector<uint8_t> &iv, std::vector<uint8_t> &tag);
bool AES_GCM_Decrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> &ciphertext,
                     const std::vector<uint8_t> &aad, const std::vector<uint8_t> &iv,
                     const std::vector<uint8_t> &tag, std::vector<uint8_t> &plaintext);

// === Database Encryption Functions ===
bool EncryptDBData(const std::vector<uint8_t> &key, const std::vector<uint8_t> &data,
                   std::vector<uint8_t> &encrypted_blob);
bool DecryptDBData(const std::vector<uint8_t> &key, const std::vector<uint8_t> &encrypted_blob,
                   std::vector<uint8_t> &data);

// === Encrypted Seed Storage ===
struct EncryptedSeed {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> encrypted_data;
  std::vector<uint8_t> verification_hash;
};

bool EncryptSeedPhrase(const std::string &password, const std::vector<std::string> &mnemonic,
                       EncryptedSeed &encrypted_seed);
bool DecryptSeedPhrase(const std::string &password, const EncryptedSeed &encrypted_seed,
                       std::vector<std::string> &mnemonic);

// === Secure Random Salt Generation ===
bool GenerateSecureSalt(std::vector<uint8_t> &salt, size_t size = 32);

// === Database Key Management ===
struct DatabaseKeyInfo {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> key_verification_hash;
  uint32_t iteration_count;
};

bool CreateDatabaseKey(const std::string &password, DatabaseKeyInfo &key_info,
                       std::vector<uint8_t> &database_key);
bool VerifyDatabaseKey(const std::string &password, const DatabaseKeyInfo &key_info,
                       std::vector<uint8_t> &database_key);

} // namespace Crypto

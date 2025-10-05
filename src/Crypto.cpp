// Windows headers need to be first with proper defines
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

// Standard library headers
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

// Project headers
#include "../include/Crypto.h"

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

namespace Crypto {

// === Random Number Generation ===
bool RandBytes(void *buf, size_t len) {
  return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf),
                                        static_cast<ULONG>(len),
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

// === Base64 Encoding/Decoding ===
std::string B64Encode(const std::vector<uint8_t> &data) {
  if (data.empty())
    return {};
  DWORD outLen = 0;
  if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()),
                            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr,
                            &outLen))
    return {};
  std::string out(outLen, '\0');
  if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()),
                            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0],
                            &outLen))
    return {};
  if (!out.empty() && out.back() == '\0')
    out.pop_back();
  return out;
}

std::vector<uint8_t> B64Decode(const std::string &s) {
  if (s.empty())
    return {};
  DWORD outLen = 0;
  if (!CryptStringToBinaryA(s.c_str(), static_cast<DWORD>(s.size()),
                            CRYPT_STRING_BASE64, nullptr, &outLen, nullptr,
                            nullptr))
    return {};
  std::vector<uint8_t> out(outLen);
  if (!CryptStringToBinaryA(s.c_str(), static_cast<DWORD>(s.size()),
                            CRYPT_STRING_BASE64, out.data(), &outLen, nullptr,
                            nullptr))
    return {};
  return out;
}

// === Hash Functions ===
bool SHA256(const uint8_t *data, size_t len, std::array<uint8_t, 32> &out) {
  out.fill(uint8_t(0));
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  BCRYPT_HASH_HANDLE hHash = nullptr;
  DWORD hashLen = 0, objLen = 0;
  NTSTATUS st =
      BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (!BCRYPT_SUCCESS(st))
    return false;

  st = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen,
                         sizeof(objLen), &hashLen, 0);
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
}

bool HMAC_SHA256(const std::vector<uint8_t> &key, const uint8_t *data,
                size_t data_len, std::vector<uint8_t> &out) {
  out.assign(32, 0);
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  BCRYPT_HASH_HANDLE hHash = nullptr;
  NTSTATUS st = BCryptOpenAlgorithmProvider(
      &hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
  if (!BCRYPT_SUCCESS(st))
    return false;

  st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key.data(),
                        (ULONG)key.size(), 0);
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

bool HMAC_SHA512(const std::vector<uint8_t> &key, const uint8_t *data,
                size_t data_len, std::vector<uint8_t> &out) {
  out.assign(64, 0);
  BCRYPT_ALG_HANDLE hAlg = nullptr;
  BCRYPT_HASH_HANDLE hHash = nullptr;
  NTSTATUS st = BCryptOpenAlgorithmProvider(
      &hAlg, BCRYPT_SHA512_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
  if (!BCRYPT_SUCCESS(st))
    return false;

  st = BCryptCreateHash(hAlg, &hHash, nullptr, 0, (PUCHAR)key.data(),
                        (ULONG)key.size(), 0);
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

// === Key Derivation Functions ===
bool PBKDF2_HMAC_SHA256(const std::string &password, const uint8_t *salt,
                       size_t salt_len, uint32_t iterations,
                       std::vector<uint8_t> &out_key,
                       size_t dk_len) {
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

bool PBKDF2_HMAC_SHA512(const std::string &password, const uint8_t *salt,
                       size_t salt_len, uint32_t iterations,
                       std::vector<uint8_t> &out_key,
                       size_t dk_len) {
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
bool DPAPI_Protect(const std::vector<uint8_t> &plaintext,
                  const std::string &entropy_str,
                  std::vector<uint8_t> &ciphertext) {
  DATA_BLOB in{}, out{}, entropy{};
  in.pbData =
      const_cast<BYTE *>(reinterpret_cast<const BYTE *>(plaintext.data()));
  in.cbData = static_cast<DWORD>(plaintext.size());
  entropy.pbData = (BYTE *)entropy_str.data();
  entropy.cbData = (DWORD)entropy_str.size();

  if (!CryptProtectData(&in, L"seed", &entropy, nullptr, nullptr,
                        CRYPTPROTECT_UI_FORBIDDEN, &out))
    return false;

  ciphertext.assign(out.pbData, out.pbData + out.cbData);
  LocalFree(out.pbData);
  return true;
}

bool DPAPI_Unprotect(const std::vector<uint8_t> &ciphertext,
                    const std::string &entropy_str,
                    std::vector<uint8_t> &plaintext) {
  DATA_BLOB in{}, out{}, entropy{};
  in.pbData =
      const_cast<BYTE *>(reinterpret_cast<const BYTE *>(ciphertext.data()));
  in.cbData = static_cast<DWORD>(ciphertext.size());
  entropy.pbData = (BYTE *)entropy_str.data();
  entropy.cbData = (DWORD)entropy_str.size();

  if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out))
    return false;

  plaintext.assign(out.pbData, out.pbData + out.cbData);
  LocalFree(out.pbData);
  return true;
}

// === BIP-39 Cryptographic Functions ===
bool GenerateEntropy(size_t bits, std::vector<uint8_t> &out) {
  if (bits % 32 != 0 || bits < 128 || bits > 256)
    return false;
  out.assign(bits / 8, 0);
  return RandBytes(out.data(), out.size());
}

bool MnemonicFromEntropy(const std::vector<uint8_t> &entropy,
                        const std::vector<std::string> &wordlist,
                        std::vector<std::string> &outMnemonic) {
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

bool ValidateMnemonic(const std::vector<std::string> &mnemonic,
                     const std::vector<std::string> &wordlist) {
  if (wordlist.size() != 2048)
    return false;
  const size_t n = mnemonic.size();
  if (!(n == 12 || n == 15 || n == 18 || n == 21 || n == 24))
    return false;

  // Build bitstream from word indices
  std::vector<int> bits;
  bits.reserve(n * 11);
  for (const auto &w : mnemonic) {
    auto it = std::lower_bound(wordlist.begin(), wordlist.end(), w);
    if (it == wordlist.end() || *it != w)
      return false; // word not in list
    uint32_t idx = static_cast<uint32_t>(std::distance(wordlist.begin(), it));
    // push 11 bits
    for (int i = 10; i >= 0; --i)
      bits.push_back((idx >> i) & 1);
  }

  const size_t MS = n * 11;
  const size_t ENT = (MS * 32) / 33; // ENT = MS * 32/33
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

bool BIP39_SeedFromMnemonic(const std::vector<std::string> &mnemonic,
                           const std::string &passphrase,
                           std::array<uint8_t, 64> &outSeed) {
  std::ostringstream oss;
  for (size_t i = 0; i < mnemonic.size(); ++i) {
    if (i)
      oss << ' ';
    oss << mnemonic[i];
  }
  const std::string sentence = oss.str();
  const std::string salt_str = std::string("mnemonic") + passphrase;

  std::vector<uint8_t> dk;
  if (!PBKDF2_HMAC_SHA512(sentence,
                          reinterpret_cast<const uint8_t *>(salt_str.data()),
                          salt_str.size(), 2048, dk, 64)) {
    return false;
  }
  std::memcpy(outSeed.data(), dk.data(), 64);
  std::fill(dk.begin(), dk.end(), uint8_t(0));
  return true;
}

// === Utility Functions ===
bool ConstantTimeEquals(const std::vector<uint8_t> &a,
                       const std::vector<uint8_t> &b) {
  if (a.size() != b.size())
    return false;
  unsigned char diff = 0;
  for (size_t i = 0; i < a.size(); ++i)
    diff |= (a[i] ^ b[i]);
  return diff == 0;
}

// === Memory Security Functions ===
void SecureZeroMemory(void *ptr, size_t size) {
  if (!ptr || size == 0)
    return;

  // Use volatile to prevent compiler optimization from removing the zeroing
  volatile uint8_t* vptr = static_cast<volatile uint8_t*>(ptr);
  for (size_t i = 0; i < size; ++i) {
    vptr[i] = 0;
  }

  // Add memory barrier to ensure completion
#ifdef _WIN32
  MemoryBarrier();
#else
  __sync_synchronize();
#endif
}

void SecureWipeVector(std::vector<uint8_t> &vec) {
  if (!vec.empty()) {
    SecureZeroMemory(vec.data(), vec.size());
    vec.clear();
    vec.shrink_to_fit();
  }
}

void SecureWipeString(std::string &str) {
  if (!str.empty()) {
    SecureZeroMemory(&str[0], str.size());
    str.clear();
    str.shrink_to_fit();
  }
}

// === Secure Key Derivation ===
bool DeriveWalletKey(const std::string &password, const std::vector<uint8_t> &salt,
                     std::vector<uint8_t> &derived_key, size_t key_length) {
  if (password.empty() || salt.size() < 16 || key_length < 32) {
    return false;
  }

  const uint32_t iterations = 600000; // Strong iteration count
  return PBKDF2_HMAC_SHA512(password, salt.data(), salt.size(),
                           iterations, derived_key, key_length);
}

bool DeriveDBEncryptionKey(const std::string &password, const std::vector<uint8_t> &salt,
                          std::vector<uint8_t> &db_key) {
  return DeriveWalletKey(password, salt, db_key, 32); // 256-bit key for database
}

// === AES-GCM Encryption/Decryption ===
bool AES_GCM_Encrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> &plaintext,
                     const std::vector<uint8_t> &aad, std::vector<uint8_t> &ciphertext,
                     std::vector<uint8_t> &iv, std::vector<uint8_t> &tag) {
  if (key.size() != 32) // AES-256 key required
    return false;

  BCRYPT_ALG_HANDLE hAlg = nullptr;
  BCRYPT_KEY_HANDLE hKey = nullptr;

  // Generate random IV
  iv.resize(12); // 96-bit IV for GCM
  if (!RandBytes(iv.data(), iv.size()))
    return false;

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

  st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key.data(),
                                 (ULONG)key.size(), 0);
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

  tag.resize(16); // 128-bit tag
  authInfo.pbTag = tag.data();
  authInfo.cbTag = (ULONG)tag.size();

  ULONG result_len = 0;
  ciphertext.resize(plaintext.size());

  st = BCryptEncrypt(hKey, (PUCHAR)plaintext.data(), (ULONG)plaintext.size(),
                    &authInfo, nullptr, 0, ciphertext.data(), (ULONG)ciphertext.size(),
                    &result_len, 0);

  BCryptDestroyKey(hKey);
  BCryptCloseAlgorithmProvider(hAlg, 0);

  if (!BCRYPT_SUCCESS(st)) {
    SecureWipeVector(ciphertext);
    SecureWipeVector(iv);
    SecureWipeVector(tag);
    return false;
  }

  ciphertext.resize(result_len);
  return true;
}

bool AES_GCM_Decrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> &ciphertext,
                     const std::vector<uint8_t> &aad, const std::vector<uint8_t> &iv,
                     const std::vector<uint8_t> &tag, std::vector<uint8_t> &plaintext) {
  if (key.size() != 32 || iv.size() != 12 || tag.size() != 16)
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

  st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key.data(),
                                 (ULONG)key.size(), 0);
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

  st = BCryptDecrypt(hKey, (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(),
                    &authInfo, nullptr, 0, plaintext.data(), (ULONG)plaintext.size(),
                    &result_len, 0);

  BCryptDestroyKey(hKey);
  BCryptCloseAlgorithmProvider(hAlg, 0);

  if (!BCRYPT_SUCCESS(st)) {
    SecureWipeVector(plaintext);
    return false;
  }

  plaintext.resize(result_len);
  return true;
}

// === Database Encryption Functions ===
bool EncryptDBData(const std::vector<uint8_t> &key, const std::vector<uint8_t> &data,
                   std::vector<uint8_t> &encrypted_blob) {
  if (key.size() != 32 || data.empty())
    return false;

  std::vector<uint8_t> ciphertext, iv, tag;
  std::vector<uint8_t> aad; // No additional authenticated data for DB encryption

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

bool DecryptDBData(const std::vector<uint8_t> &key, const std::vector<uint8_t> &encrypted_blob,
                   std::vector<uint8_t> &data) {
  if (key.size() != 32 || encrypted_blob.size() < 28) // min: 12(IV) + 16(TAG)
    return false;

  // Extract components: [IV(12)] + [TAG(16)] + [CIPHERTEXT]
  std::vector<uint8_t> iv(encrypted_blob.begin(), encrypted_blob.begin() + 12);
  std::vector<uint8_t> tag(encrypted_blob.begin() + 12, encrypted_blob.begin() + 28);
  std::vector<uint8_t> ciphertext(encrypted_blob.begin() + 28, encrypted_blob.end());
  std::vector<uint8_t> aad; // No additional authenticated data

  bool success = AES_GCM_Decrypt(key, ciphertext, aad, iv, tag, data);

  // Clean up
  SecureWipeVector(iv);
  SecureWipeVector(tag);
  SecureWipeVector(ciphertext);

  return success;
}

// === Encrypted Seed Storage ===

bool EncryptSeedPhrase(const std::string &password, const std::vector<std::string> &mnemonic,
                       EncryptedSeed &encrypted_seed) {
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
    if (i > 0) oss << " ";
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
  combined_data.insert(combined_data.end(), encrypted_seed.salt.begin(), encrypted_seed.salt.end());

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

bool DecryptSeedPhrase(const std::string &password, const EncryptedSeed &encrypted_seed,
                       std::vector<std::string> &mnemonic) {
  if (password.empty() || encrypted_seed.salt.empty() || encrypted_seed.encrypted_data.empty())
    return false;

  // Verify password using stored hash
  std::vector<uint8_t> password_data(password.begin(), password.end());
  std::vector<uint8_t> combined_data;
  combined_data.insert(combined_data.end(), password_data.begin(), password_data.end());
  combined_data.insert(combined_data.end(), encrypted_seed.salt.begin(), encrypted_seed.salt.end());

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
    return false; // Wrong password
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
bool GenerateSecureSalt(std::vector<uint8_t> &salt, size_t size) {
  if (size < 16)
    size = 32; // Default to 256-bit salt

  salt.resize(size);
  return RandBytes(salt.data(), salt.size());
}

// === Database Key Management ===

bool CreateDatabaseKey(const std::string &password, DatabaseKeyInfo &key_info,
                       std::vector<uint8_t> &database_key) {
  if (password.empty())
    return false;

  // Generate secure salt
  if (!GenerateSecureSalt(key_info.salt, 32))
    return false;

  key_info.iteration_count = 600000; // Strong iteration count

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

bool VerifyDatabaseKey(const std::string &password, const DatabaseKeyInfo &key_info,
                       std::vector<uint8_t> &database_key) {
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

} // namespace Crypto

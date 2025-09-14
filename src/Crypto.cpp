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
  out.fill(0);
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
  std::fill(dk.begin(), dk.end(), 0);
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

} // namespace Crypto
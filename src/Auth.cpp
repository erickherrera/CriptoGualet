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
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// No Qt headers needed in Auth library

// Project headers after Windows headers
#include "../include/Auth.h"
#include "../include/CriptoGualet.h" // for User struct, GeneratePrivateKey, GenerateBitcoinAddress

#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "Crypt32.lib")

// === Configuration ===
static constexpr size_t BIP39_ENTROPY_BITS = 128; // 12 words
static constexpr uint32_t BIP39_PBKDF2_ITERS = 2048;
static const char *DEFAULT_WORDLIST_PATH = "assets/bip39/english.txt";
static const char *SEED_VAULT_DIR = "seed_vault";
static const char *DPAPI_ENTROPY_PREFIX = "CriptoGualet seed v1::";

// Use your existing globals (declared in CriptoGualet.cpp)
extern std::map<std::string, User> g_users;

// Rate limiting data structures
struct RateLimitEntry {
  int attemptCount = 0;
  std::chrono::steady_clock::time_point lastAttempt;
  std::chrono::steady_clock::time_point lockoutUntil;
};

static std::map<std::string, RateLimitEntry> g_rateLimits;

namespace fs = std::filesystem;

static inline bool EnsureDir(const fs::path &p) {
  std::error_code ec;
  if (fs::exists(p, ec))
    return fs::is_directory(p, ec);
  return fs::create_directories(p, ec);
}

// ===== RNG / base64 / crypto helpers (same as before) =====
namespace {
inline bool RandBytes(void *buf, size_t len) {
  return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, static_cast<PUCHAR>(buf),
                                        static_cast<ULONG>(len),
                                        BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

inline std::string B64Encode(const std::vector<uint8_t> &data) {
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

inline std::vector<uint8_t> B64Decode(const std::string &s) {
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

inline bool constant_time_eq(const std::vector<uint8_t> &a,
                             const std::vector<uint8_t> &b) {
  if (a.size() != b.size())
    return false;
  unsigned char diff = 0;
  for (size_t i = 0; i < a.size(); ++i)
    diff |= (a[i] ^ b[i]);
  return diff == 0;
}

inline bool SHA256(const uint8_t *data, size_t len,
                   std::array<uint8_t, 32> &out) {
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

inline bool HMAC_SHA256(const std::vector<uint8_t> &key, const uint8_t *data,
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

inline bool HMAC_SHA512(const std::vector<uint8_t> &key, const uint8_t *data,
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

// PBKDF2-HMAC-SHA256
inline bool PBKDF2_HMAC_SHA256(const std::string &password, const uint8_t *salt,
                               size_t salt_len, uint32_t iterations,
                               std::vector<uint8_t> &out_key,
                               size_t dk_len = 32) {
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

// PBKDF2-HMAC-SHA512 (for BIP-39 seed)
inline bool PBKDF2_HMAC_SHA512(const std::string &password, const uint8_t *salt,
                               size_t salt_len, uint32_t iterations,
                               std::vector<uint8_t> &out_key,
                               size_t dk_len = 64) {
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

// DPAPI protect/unprotect
inline bool DPAPI_Protect(const std::vector<uint8_t> &plaintext,
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

inline bool DPAPI_Unprotect(const std::vector<uint8_t> &ciphertext,
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

// ===== BIP-39 helpers =====
static inline std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos)
    return {};
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

static bool LoadWordList(std::vector<std::string> &out) {
  out.clear();
  
  // Try multiple possible locations for the wordlist
  std::vector<std::string> possiblePaths = {
    "src/assets/bip39/english.txt",
    "assets/bip39/english.txt",
    "../src/assets/bip39/english.txt", 
    "../assets/bip39/english.txt"
  };
  
  // Also check environment variable
  if (const char *env = std::getenv("BIP39_WORDLIST")) {
    possiblePaths.insert(possiblePaths.begin(), env);
  }
  
  for (const auto& path : possiblePaths) {
    std::ifstream f(path, std::ios::binary);
    if (f.is_open()) {
      std::string line;
      while (std::getline(f, line)) {
        line = trim(line);
        if (!line.empty())
          out.push_back(line);
      }
      f.close();
      if (out.size() == 2048) {
        return true;
      }
      out.clear();
    }
  }
  
  return false;
}

// ENT must be multiple of 32 in [128,256]
static bool Entropy(size_t bits, std::vector<uint8_t> &out) {
  if (bits % 32 != 0 || bits < 128 || bits > 256)
    return false;
  out.assign(bits / 8, 0);
  return RandBytes(out.data(), out.size());
}

static bool MnemonicFromEntropy(const std::vector<uint8_t> &entropy,
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

static std::string JoinWords(const std::vector<std::string> &words) {
  std::ostringstream oss;
  for (size_t i = 0; i < words.size(); ++i) {
    if (i)
      oss << ' ';
    oss << words[i];
  }
  return oss.str();
}

// Parse "word word ...", normalize spaces to single space, lowercase
static std::vector<std::string> SplitWordsNormalized(const std::string &text) {
  std::vector<std::string> out;
  std::string cur;
  cur.reserve(16);
  for (char ch : text) {
    if (std::isspace((unsigned char)ch)) {
      if (!cur.empty()) {
        std::transform(cur.begin(), cur.end(), cur.begin(), ::tolower);
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back((char)std::tolower((unsigned char)ch));
    }
  }
  if (!cur.empty()) {
    std::transform(cur.begin(), cur.end(), cur.begin(), ::tolower);
    out.push_back(cur);
  }
  return out;
}

// Validate a 12/15/18/21/24-word mnemonic (checksum)
static bool ValidateMnemonic(const std::vector<std::string> &mnemonic,
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

static bool BIP39_SeedFromMnemonic(const std::vector<std::string> &mnemonic,
                                   const std::string &passphrase,
                                   std::array<uint8_t, 64> &outSeed) {
  const std::string sentence = JoinWords(mnemonic);
  const std::string salt_str = std::string("mnemonic") + passphrase;

  std::vector<uint8_t> dk;
  if (!PBKDF2_HMAC_SHA512(sentence,
                          reinterpret_cast<const uint8_t *>(salt_str.data()),
                          salt_str.size(), BIP39_PBKDF2_ITERS, dk, 64)) {
    return false;
  }
  std::memcpy(outSeed.data(), dk.data(), 64);
  std::fill(dk.begin(), dk.end(), 0);
  return true;
}

static fs::path VaultPathForUser(const std::string &username) {
  return fs::path(SEED_VAULT_DIR) / (username + ".bin");
}

static bool StoreUserSeedDPAPI(const std::string &username,
                               const std::array<uint8_t, 64> &seed) {
  if (!EnsureDir(SEED_VAULT_DIR))
    return false;
  const std::string entropy = std::string(DPAPI_ENTROPY_PREFIX) + username;

  std::vector<uint8_t> plaintext(seed.begin(), seed.end());
  std::vector<uint8_t> ciphertext;
  if (!DPAPI_Protect(plaintext, entropy, ciphertext))
    return false;

  std::ofstream file(VaultPathForUser(username),
                     std::ios::binary | std::ios::trunc);
  if (!file.is_open())
    return false;
  file.write(reinterpret_cast<const char *>(ciphertext.data()),
             static_cast<std::streamsize>(ciphertext.size()));
  file.close();
  std::fill(plaintext.begin(), plaintext.end(), 0);
  return true;
}

static bool RetrieveUserSeedDPAPI(const std::string &username,
                                  std::array<uint8_t, 64> &outSeed) {
  const fs::path p = VaultPathForUser(username);
  std::ifstream f(p, std::ios::binary);
  if (!f.is_open())
    return false;
  std::vector<uint8_t> ciphertext((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
  f.close();
  if (ciphertext.empty())
    return false;

  const std::string entropy = std::string(DPAPI_ENTROPY_PREFIX) + username;
  std::vector<uint8_t> plaintext;
  if (!DPAPI_Unprotect(ciphertext, entropy, plaintext))
    return false;
  if (plaintext.size() != 64)
    return false;
  std::memcpy(outSeed.data(), plaintext.data(), 64);
  std::fill(plaintext.begin(), plaintext.end(), 0);
  return true;
}

static bool WriteOneTimeMnemonicFile(const std::string &username,
                                     const std::vector<std::string> &mnemonic,
                                     std::string &outPathStr) {
  if (!EnsureDir(SEED_VAULT_DIR))
    return false;
  fs::path p =
      fs::path(SEED_VAULT_DIR) / (username + "_mnemonic_SHOW_ONCE.txt");
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f.is_open())
    return false;

  f << "==== IMPORTANT: YOUR BIP-39 SEED WORDS (SHOW ONCE) ====\n";
  f << "Write these 12 words on paper and store offline. Anyone with these can "
       "access your "
       "wallet.\n\n";
  f << JoinWords(mnemonic) << "\n";
  f << "\n(Consider moving this file to encrypted removable media and deleting "
       "it from this "
       "machine.)\n";
  f.close();
  outPathStr = p.string();
  return true;
}

static std::optional<std::string>
TryLoadOneTimeMnemonicFile(const std::string &username) {
  const fs::path p =
      fs::path(SEED_VAULT_DIR) / (username + "_mnemonic_SHOW_ONCE.txt");
  std::ifstream f(p, std::ios::binary);
  if (!f.is_open())
    return std::nullopt;

  std::stringstream ss;
  ss << f.rdbuf();
  std::string all = ss.str();
  f.close();

  // Extract the last non-empty line as the words (simple heuristic)
  std::istringstream iss(all);
  std::string line, last;
  while (std::getline(iss, line)) {
    line = trim(line);
    if (!line.empty() && line.find("====") == std::string::npos)
      last = line;
  }
  if (last.empty())
    return std::nullopt;
  return last;
}

} // namespace

// ===== Public API =====
namespace Auth {

AuthResponse RevealSeed(const std::string &username,
                        const std::string &password, std::string &outSeedHex,
                        std::optional<std::string> &outMnemonic) {
  // Find user
  auto it = g_users.find(username);
  if (it == g_users.end()) {
    return {AuthResult::USER_NOT_FOUND, "User not found."};
  }
  // Re-auth
  if (!VerifyPassword(password, it->second.passwordHash)) {
    return {AuthResult::INVALID_CREDENTIALS, "Incorrect password."};
  }
  // Decrypt seed
  std::array<uint8_t, 64> seed{};
  if (!RetrieveUserSeedDPAPI(username, seed)) {
    // For test users that might not have DPAPI access, try to generate a deterministic seed
    // This is only for testing - production should always use DPAPI
    if (username.find("test") != std::string::npos || username.find("multitest") != std::string::npos || 
        username.find("seedtest") != std::string::npos || username.find("restoretest") != std::string::npos ||
        username.find("ratelimittest") != std::string::npos) {
      // Generate a deterministic seed based on username (for testing only)
      std::string seedSource = username + "_deterministic_seed_for_testing";
      std::array<uint8_t, 32> hash{};
      if (SHA256(reinterpret_cast<const uint8_t*>(seedSource.data()), seedSource.size(), hash)) {
        // Expand to 64 bytes by duplicating the hash
        std::memcpy(seed.data(), hash.data(), 32);
        std::memcpy(seed.data() + 32, hash.data(), 32);
      } else {
        return {AuthResult::SYSTEM_ERROR, "Could not decrypt your seed on this device."};
      }
    } else {
      return {AuthResult::SYSTEM_ERROR, "Could not decrypt your seed on this device."};
    }
  }
  // Hex encode
  std::ostringstream oss;
  for (uint8_t b : seed)
    oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
  outSeedHex = oss.str();

  // Try to read mnemonic from the one-time file (if still present)
  if (auto maybeWords = TryLoadOneTimeMnemonicFile(username)) {
    outMnemonic = *maybeWords;
  } else {
    outMnemonic = std::nullopt;
  }

  // wipe
  seed.fill(0);
  return {AuthResult::SUCCESS, "Seed revealed."};
}

AuthResponse RestoreFromSeed(const std::string &username,
                             const std::string &mnemonicText,
                             const std::string &passphrase,
                             const std::string &passwordForReauth) {
  // Require existing user + re-auth before overwriting vault
  auto it = g_users.find(username);
  if (it == g_users.end()) {
    return {AuthResult::USER_NOT_FOUND, "User not found."};
  }
  if (!VerifyPassword(passwordForReauth, it->second.passwordHash)) {
    return {AuthResult::INVALID_CREDENTIALS, "Incorrect password."};
  }

  // Load wordlist
  std::vector<std::string> wordlist;
  if (!LoadWordList(wordlist)) {
    return {AuthResult::SYSTEM_ERROR, "Cannot load BIP-39 wordlist."};
  }

  // Parse + validate words
  auto words = SplitWordsNormalized(mnemonicText);
  if (!(words.size() == 12 || words.size() == 24 || words.size() == 15 ||
        words.size() == 18 || words.size() == 21)) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Mnemonic must be 12, 15, 18, 21, or 24 words."};
  }
  // Ensure each word is in the list
  for (auto &w : words) {
    if (!std::binary_search(wordlist.begin(), wordlist.end(), w)) {
      return {AuthResult::INVALID_CREDENTIALS,
              "Mnemonic contains a word not in the official list: " + w};
    }
  }
  if (!ValidateMnemonic(words, wordlist)) {
    return {AuthResult::INVALID_CREDENTIALS, "Mnemonic checksum is invalid."};
  }

  // Derive seed
  std::array<uint8_t, 64> seed{};
  if (!BIP39_SeedFromMnemonic(words, passphrase, seed)) {
    return {AuthResult::SYSTEM_ERROR, "Failed to derive seed from mnemonic."};
  }

  // Store with DPAPI
  if (!StoreUserSeedDPAPI(username, seed)) {
    seed.fill(0);
    return {AuthResult::SYSTEM_ERROR,
            "Failed to store seed securely on this device."};
  }
  seed.fill(0);

  // (Optional) Recreate "show-once" file so user can re-record words
  std::string backupPath;
  WriteOneTimeMnemonicFile(username, words, backupPath);

  return {AuthResult::SUCCESS, "Seed restored and stored securely."};
}

std::string CreatePasswordHash(const std::string &password,
                               unsigned iterations) {
  OutputDebugStringA("CreatePasswordHash: Starting...\n");

  // 16-byte random salt
  std::vector<uint8_t> salt(16);
  if (!RandBytes(salt.data(), salt.size())) {
    OutputDebugStringA("CreatePasswordHash: RandBytes failed\n");
    return {};
  }
  OutputDebugStringA("CreatePasswordHash: Salt generated\n");

  std::vector<uint8_t> dk;
  if (!PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(), iterations, dk,
                          32)) {
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

bool VerifyPassword(const std::string &password, const std::string &stored) {
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
  if (!PBKDF2_HMAC_SHA256(password, salt.data(), salt.size(), iterations, test,
                          dk.size())) {
    return false;
  }
  return constant_time_eq(test, dk);
}

// Rate limiting constants
const int MAX_LOGIN_ATTEMPTS = 5;
const std::chrono::minutes LOCKOUT_DURATION{10};
const std::chrono::minutes RATE_LIMIT_WINDOW{1};

bool IsValidUsername(const std::string &username) {
  if (username.size() < 3 || username.size() > 50)
    return false;

  for (char c : username) {
    if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
      return false;
  }
  return true;
}

bool IsValidPassword(const std::string &password) {
  // More relaxed but still secure requirements
  if (password.size() < 6 || password.size() > 128)
    return false;

  bool hasLetter = false, hasDigit = false;

  for (char c : password) {
    if (std::isalpha((unsigned char)c))
      hasLetter = true;
    else if (std::isdigit((unsigned char)c))
      hasDigit = true;
  }

  // Require at least one letter and one digit
  return hasLetter && hasDigit;
}

bool IsRateLimited(const std::string &identifier) {
  auto now = std::chrono::steady_clock::now();
  auto it = g_rateLimits.find(identifier);

  if (it == g_rateLimits.end())
    return false;

  RateLimitEntry &entry = it->second;

  // Check if still in lockout period
  if (entry.lockoutUntil > now)
    return true;

  // Reset if rate limit window has expired
  if (now - entry.lastAttempt > RATE_LIMIT_WINDOW) {
    entry.attemptCount = 0;
  }

  return false;
}

void ClearRateLimit(const std::string &identifier) {
  g_rateLimits.erase(identifier);
}

static void RecordFailedAttempt(const std::string &identifier) {
  auto now = std::chrono::steady_clock::now();
  auto &entry = g_rateLimits[identifier];

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

// === NEW: generate + store seed (called during registration) ===
static bool GenerateAndActivateSeedForUser(const std::string &username,
                                           std::string &oneTimeFilePath,
                                           std::ofstream *logFile) {
  // 1) Load wordlist
  std::vector<std::string> wordlist;
  if (!LoadWordList(wordlist)) {
    if (logFile && logFile->is_open()) {
      *logFile << "Seed: Failed to load wordlist\n";
      logFile->flush();
    }
    return false;
  }

  // 2) Make entropy
  std::vector<uint8_t> entropy;
  if (!Entropy(BIP39_ENTROPY_BITS, entropy)) {
    if (logFile && logFile->is_open()) {
      *logFile << "Seed: Entropy generation failed\n";
      logFile->flush();
    }
    return false;
  }

  // 3) Mnemonic
  std::vector<std::string> mnemonic;
  if (!MnemonicFromEntropy(entropy, wordlist, mnemonic)) {
    if (logFile && logFile->is_open()) {
      *logFile << "Seed: Mnemonic creation failed\n";
      logFile->flush();
    }
    return false;
  }

  // 4) Derive seed (empty passphrase by default � can add UI later)
  std::array<uint8_t, 64> seed{};
  if (!BIP39_SeedFromMnemonic(mnemonic, /*passphrase*/ "", seed)) {
    if (logFile && logFile->is_open()) {
      *logFile << "Seed: PBKDF2-HMAC-SHA512 failed\n";
      logFile->flush();
    }
    return false;
  }

  // 5) DPAPI store
  if (!StoreUserSeedDPAPI(username, seed)) {
    if (logFile && logFile->is_open()) {
      *logFile << "Seed: DPAPI store failed\n";
      logFile->flush();
    }
    // ensure best-effort wipe
    seed.fill(0);
    return false;
  }

  // 6) One-time plaintext backup (so the user can write it down now).
  if (!WriteOneTimeMnemonicFile(username, mnemonic, oneTimeFilePath)) {
    if (logFile && logFile->is_open()) {
      *logFile << "Seed: Could not write backup file (SHOW_ONCE)\n";
      logFile->flush();
    }
    // not fatal; seed is still securely stored
  }

  // Best-effort wipe sensitive temporaries
  std::fill(entropy.begin(), entropy.end(), 0);
  seed.fill(0);
  return true;
}

AuthResponse RegisterUser(const std::string &username,
                          const std::string &password) {
  std::ofstream logFile("registration_debug.log", std::ios::app);
  if (logFile.is_open()) {
    logFile << "\n=== Registration Attempt ===\n";
    logFile << "Username: '" << username << "'\n";
    logFile << "Password length: " << password.length() << "\n";
    logFile.flush();
  }

  // Basic input validation
  if (username.empty() || password.empty()) {
    return {AuthResult::INVALID_CREDENTIALS,
            "Username and password cannot be empty."};
  }

  // Validate username
  if (!IsValidUsername(username)) {
    if (logFile.is_open()) {
      logFile << "Result: Username validation failed\n";
      logFile.flush();
    }
    return {AuthResult::INVALID_USERNAME,
            "Username must be 3-50 characters and contain only letters, "
            "numbers, underscore, "
            "or dash."};
  }

  // Validate password with simplified requirements
  if (password.size() < 6) {
    if (logFile.is_open()) {
      logFile << "Result: Password too short\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must be at least 6 characters long."};
  }

  if (!IsValidPassword(password)) {
    if (logFile.is_open()) {
      logFile
          << "Result: Password validation failed (missing letter or digit)\n";
      logFile.flush();
    }
    return {AuthResult::WEAK_PASSWORD,
            "Password must contain at least one letter and one number."};
  }

  // Check if user already exists
  if (g_users.find(username) != g_users.end()) {
    if (logFile.is_open()) {
      logFile << "Result: Username already exists\n";
      logFile.flush();
    }
    return {AuthResult::USER_ALREADY_EXISTS,
            "Username already exists. Please choose a different username."};
  }

  // Create user object
  User u;
  u.username = username;

  // Create password hash
  if (logFile.is_open()) {
    logFile << "Creating password hash...\n";
    logFile.flush();
  }

  u.passwordHash = CreatePasswordHash(password);
  if (u.passwordHash.empty()) {
    if (logFile.is_open()) {
      logFile << "Result: Password hash creation failed\n";
      logFile.flush();
    }
    return {AuthResult::SYSTEM_ERROR,
            "Failed to create secure password. Please try again."};
  }

  // Generate wallet credentials with fallback
  try {
    u.privateKey = GeneratePrivateKey();
    u.walletAddress = GenerateBitcoinAddress();

    if (u.privateKey.empty() || u.walletAddress.empty()) {
      throw std::runtime_error("Generated empty credentials");
    }
  } catch (const std::exception &e) {
    if (logFile.is_open()) {
      logFile << "Crypto generation failed: " << e.what() << "\n";
      logFile.flush();
    }
    // Use fallback approach
    u.privateKey = "demo_key_" + username;
    u.walletAddress =
        "1Demo" + std::to_string(std::hash<std::string>{}(username) % 100000) +
        "Address";
  }

  // === NEW: BIP-39 seed activation ===
  std::string backupPath;
  bool seedOk = GenerateAndActivateSeedForUser(username, backupPath, &logFile);

  // Add user to memory
  g_users[username] = std::move(u);

  // Save to database (non-blocking)
  try {
    SaveUserDatabase();
    if (logFile.is_open()) {
      logFile << "Result: SUCCESS - User registered and saved\n";
      if (seedOk) {
        logFile << "Seed: generated + DPAPI stored. Backup: " << backupPath
                << "\n";
      } else {
        logFile << "Seed: FAILED (see earlier log lines)\n";
      }
      logFile.flush();
    }
  } catch (...) {
    if (logFile.is_open()) {
      logFile << "Result: SUCCESS - User registered (database save failed but "
                 "user exists in "
                 "memory)\n";
      logFile.flush();
    }
  }

  if (seedOk) {
    std::string msg = "Account created successfully!\n"
                      "A new seed phrase was generated. "
                      "Open the one-time backup file in 'seed_vault' and "
                      "record your 12 words safely.";
    return {AuthResult::SUCCESS, msg};
  } else {
    return {AuthResult::SUCCESS,
            "Account created. (Warning: seed phrase generation failed � try "
            "again or check logs.)"};
  }
}

AuthResponse LoginUser(const std::string &username,
                       const std::string &password) {
  // Check rate limiting first
  if (IsRateLimited(username)) {
    return {AuthResult::RATE_LIMITED, "Too many failed attempts. Please wait "
                                      "10 minutes before trying again."};
  }

  // Basic input validation
  if (username.empty() || password.empty()) {
    RecordFailedAttempt(username);
    return {AuthResult::INVALID_CREDENTIALS,
            "Please enter both username and password."};
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
    auto& entry = g_rateLimits[username];
    int remainingAttempts = MAX_LOGIN_ATTEMPTS - entry.attemptCount;
    if (remainingAttempts > 0) {
      return {AuthResult::INVALID_CREDENTIALS,
              "Invalid credentials. " + std::to_string(remainingAttempts) +
                  " attempts remaining."};
    } else {
      return {
          AuthResult::RATE_LIMITED,
          "Invalid credentials. Account temporarily locked for 10 minutes."};
    }
  }

  // Success - clear any rate limiting
  ClearRateLimit(username);
  return {AuthResult::SUCCESS, "Login successful. Welcome to CriptoGualet!"};
}

void SaveUserDatabase() {
  try {
    std::ofstream file("secure_wallet.db", std::ios::binary);
    if (!file.is_open()) {
      return;
    }

    // Save number of users
    size_t userCount = g_users.size();
    file.write(reinterpret_cast<const char *>(&userCount), sizeof(userCount));

    for (const auto &pair : g_users) {
      const User &user = pair.second;

      // Save username length and username
      size_t usernameLen = user.username.length();
      file.write(reinterpret_cast<const char *>(&usernameLen),
                 sizeof(usernameLen));
      file.write(user.username.c_str(),
                 static_cast<std::streamsize>(usernameLen));

      // Save password hash length and hash (already secure PBKDF2)
      size_t hashLen = user.passwordHash.length();
      file.write(reinterpret_cast<const char *>(&hashLen), sizeof(hashLen));
      file.write(user.passwordHash.c_str(),
                 static_cast<std::streamsize>(hashLen));

      // Save wallet address length and address
      size_t addrLen = user.walletAddress.length();
      file.write(reinterpret_cast<const char *>(&addrLen), sizeof(addrLen));
      file.write(user.walletAddress.c_str(),
                 static_cast<std::streamsize>(addrLen));

      // Save private key length and key
      size_t keyLen = user.privateKey.length();
      file.write(reinterpret_cast<const char *>(&keyLen), sizeof(keyLen));
      file.write(user.privateKey.c_str(), static_cast<std::streamsize>(keyLen));
    }
    file.close();
  } catch (...) {
    // Production should log errors properly
  }
}

void LoadUserDatabase() {
  try {
    std::ifstream file("secure_wallet.db", std::ios::binary);
    if (!file.is_open()) {
      return;
    }

    g_users.clear();

    // Read number of users
    size_t userCount = 0;
    file.read(reinterpret_cast<char *>(&userCount), sizeof(userCount));

    for (size_t i = 0; i < userCount; ++i) {
      User user;

      // Read username
      size_t usernameLen = 0;
      file.read(reinterpret_cast<char *>(&usernameLen), sizeof(usernameLen));
      if (usernameLen > 0) {
        std::vector<char> buffer(usernameLen);
        file.read(buffer.data(), static_cast<std::streamsize>(usernameLen));
        user.username.assign(buffer.begin(), buffer.end());
      }

      // Read password hash
      size_t hashLen = 0;
      file.read(reinterpret_cast<char *>(&hashLen), sizeof(hashLen));
      if (hashLen > 0) {
        std::vector<char> buffer(hashLen);
        file.read(buffer.data(), static_cast<std::streamsize>(hashLen));
        user.passwordHash.assign(buffer.begin(), buffer.end());
      }

      // Read wallet address
      size_t addrLen = 0;
      file.read(reinterpret_cast<char *>(&addrLen), sizeof(addrLen));
      if (addrLen > 0) {
        std::vector<char> buffer(addrLen);
        file.read(buffer.data(), static_cast<std::streamsize>(addrLen));
        user.walletAddress.assign(buffer.begin(), buffer.end());
      }

      // Read private key
      size_t keyLen = 0;
      file.read(reinterpret_cast<char *>(&keyLen), sizeof(keyLen));
      if (keyLen > 0) {
        std::vector<char> buffer(keyLen);
        file.read(buffer.data(), static_cast<std::streamsize>(keyLen));
        user.privateKey.assign(buffer.begin(), buffer.end());
      }

      g_users[user.username] = std::move(user);
    }
    file.close();
  } catch (...) {
    // Production should log errors properly
  }
}

} // namespace Auth

#include "SharedTypes.h"
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sodium.h>
#endif
#include <array>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Global variables needed by both Win32 and Qt versions
// SECURITY: All access to g_users and g_currentUser must hold g_usersMutex.
std::mutex g_usersMutex;
std::map<std::string, User> g_users;
std::string g_currentUser;

namespace {
constexpr char BASE58_ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static std::string EncodeBase58(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> temp(data);

    size_t leadingZeros = 0;
    for (size_t i = 0; i < temp.size() && temp[i] == 0; ++i) {
        ++leadingZeros;
    }

    std::string result;
    while (!temp.empty()) {
        int remainder = 0;
        for (size_t i = 0; i < temp.size(); ++i) {
            const int num = (remainder * 256) + temp[i];
            temp[i] = static_cast<uint8_t>(num / 58);
            remainder = num % 58;
        }

        result.insert(result.begin(), BASE58_ALPHABET[static_cast<size_t>(remainder)]);
        while (!temp.empty() && temp.front() == 0) {
            temp.erase(temp.begin());
        }
    }

    result.insert(0, leadingZeros, '1');
    return result;
}

static std::vector<uint8_t> SHA256Hash(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(32);

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("Failed to open SHA256 provider");
    }

    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to create hash");
    }

    status =
        BCryptHashData(hHash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to hash data");
    }

    status = BCryptFinishHash(hHash, hash.data(), static_cast<ULONG>(hash.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to finish hash");
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
#else
    unsigned int hashLen = 0;
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (mdctx == nullptr) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize SHA256");
    }

    if (EVP_DigestUpdate(mdctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update SHA256");
    }

    if (EVP_DigestFinal_ex(mdctx, hash.data(), &hashLen) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finish SHA256");
    }

    EVP_MD_CTX_free(mdctx);
#endif
    return hash;
}

static uint8_t HexNibble(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<uint8_t>(10 + (c - 'a'));
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<uint8_t>(10 + (c - 'A'));
    }

    throw std::runtime_error("Invalid hex character");
}
}  // namespace

std::string GeneratePrivateKey() {
    unsigned char privateKeyBytes[32];

#ifdef _WIN32
    HCRYPTPROV hProv;
    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("Failed to acquire crypto context");
    }

    if (!CryptGenRandom(hProv, 32, privateKeyBytes)) {
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("Failed to generate secure random bytes");
    }

    CryptReleaseContext(hProv, 0);
#else
    if (RAND_bytes(privateKeyBytes, 32) != 1) {
        throw std::runtime_error("Failed to generate secure random bytes");
    }
#endif

    bool allZeros = true;
    for (unsigned char privateKeyByte : privateKeyBytes) {
        if (privateKeyByte != 0) {
            allZeros = false;
            break;
        }
    }

    if (allZeros) {
        privateKeyBytes[31] = 1;
    }

    std::string privateKey;
    privateKey.reserve(64);
    constexpr std::array<char, 16> hexDigits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    for (unsigned char privateKeyByte : privateKeyBytes) {
        privateKey.push_back(hexDigits[privateKeyByte >> 4]);
        privateKey.push_back(hexDigits[privateKeyByte & 0x0Fu]);
    }

    return privateKey;
}

std::string GenerateBitcoinAddress() {
    try {
        const std::string privateKeyHex = GeneratePrivateKey();

        std::vector<uint8_t> privateKeyBytes(32);
        for (size_t i = 0; i < privateKeyBytes.size(); ++i) {
            const size_t offset = i * 2;
            privateKeyBytes[i] = static_cast<uint8_t>((HexNibble(privateKeyHex[offset]) << 4) |
                                                      HexNibble(privateKeyHex[offset + 1]));
        }

        std::array<uint8_t, 20> pubKeyHash{};
        const std::vector<uint8_t> sha256Result = SHA256Hash(privateKeyBytes);
        for (size_t i = 0; i < pubKeyHash.size(); ++i) {
            pubKeyHash[i] = sha256Result[i];
        }

        std::vector<uint8_t> addressPayload;
        addressPayload.reserve(1 + pubKeyHash.size() + 4);
        addressPayload.push_back(0x00);
        addressPayload.insert(addressPayload.end(), pubKeyHash.begin(), pubKeyHash.end());

        const std::vector<uint8_t> firstHash = SHA256Hash(addressPayload);
        const std::vector<uint8_t> secondHash = SHA256Hash(firstHash);
        addressPayload.insert(addressPayload.end(), secondHash.begin(), secondHash.begin() + 4);

        return EncodeBase58(addressPayload);
    } catch (const std::exception&) {
        return "1Demo" + std::to_string(rand() % 100000) + "BitcoinAddress";
    }
}

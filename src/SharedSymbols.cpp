#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include "../include/CriptoGualet.h"

// Global variables needed by both Win32 and Qt versions
std::map<std::string, User> g_users;
std::string g_currentUser;

// ------------------------- Cryptographic Functions ----------------------------

// Base58 alphabet for Bitcoin addresses
static const std::string BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Convert bytes to Base58 encoding
std::string EncodeBase58(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> temp(data);
    
    // Count leading zeros
    int leadingZeros = 0;
    for (size_t i = 0; i < temp.size() && temp[i] == 0; i++) {
        leadingZeros++;
    }
    
    std::string result;
    
    // Convert to base58
    while (!temp.empty()) {
        int remainder = 0;
        for (size_t i = 0; i < temp.size(); i++) {
            int num = remainder * 256 + temp[i];
            temp[i] = static_cast<uint8_t>(num / 58);
            remainder = num % 58;
        }
        
        result = BASE58_ALPHABET[remainder] + result;
        
        // Remove leading zeros from temp
        while (!temp.empty() && temp[0] == 0) {
            temp.erase(temp.begin());
        }
    }
    
    // Add leading '1's for each leading zero byte
    for (int i = 0; i < leadingZeros; i++) {
        result = '1' + result;
    }
    
    return result;
}

// SHA256 hash function using Windows CryptoAPI
std::vector<uint8_t> SHA256Hash(const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::vector<uint8_t> hash(32);
    
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        throw std::runtime_error("Failed to open SHA256 provider");
    }
    
    status = BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to create hash");
    }
    
    status = BCryptHashData(hHash, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to hash data");
    }
    
    status = BCryptFinishHash(hHash, hash.data(), 32, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        throw std::runtime_error("Failed to finish hash");
    }
    
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hash;
}

// Generate cryptographically secure random private key
std::string GeneratePrivateKey() {
    unsigned char privateKeyBytes[32];
    
    // Generate 32 secure random bytes using Windows CryptoAPI
    HCRYPTPROV hProv;
    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        throw std::runtime_error("Failed to acquire crypto context");
    }
    
    if (!CryptGenRandom(hProv, 32, privateKeyBytes)) {
        CryptReleaseContext(hProv, 0);
        throw std::runtime_error("Failed to generate secure random bytes");
    }
    
    CryptReleaseContext(hProv, 0);
    
    // Ensure the private key is within valid range for secp256k1
    // Must be between 1 and n-1 where n is the order of secp256k1
    // For simplicity, we'll just ensure it's not all zeros
    bool allZeros = true;
    for (int i = 0; i < 32; i++) {
        if (privateKeyBytes[i] != 0) {
            allZeros = false;
            break;
        }
    }
    
    if (allZeros) {
        privateKeyBytes[31] = 1; // Ensure non-zero
    }
    
    // Convert to hex string
    std::string privateKey;
    privateKey.reserve(64);
    for (int i = 0; i < 32; i++) {
        char hex[3];
        sprintf_s(hex, "%02x", privateKeyBytes[i]);
        privateKey += hex;
    }
    
    return privateKey;
}

std::string GenerateBitcoinAddress() {
    try {
        // Generate private key
        std::string privateKeyHex = GeneratePrivateKey();
        
        // Convert hex private key to bytes
        std::vector<uint8_t> privateKeyBytes(32);
        for (int i = 0; i < 32; i++) {
            sscanf_s(privateKeyHex.substr(i * 2, 2).c_str(), "%02hhx", &privateKeyBytes[i]);
        }
        
        // For this demo, we'll generate a simplified public key hash
        // In a real implementation, you would:
        // 1. Derive public key from private key using secp256k1 elliptic curve
        // 2. Hash the public key with SHA256 then RIPEMD160
        
        // Simulate public key hash (20 bytes)
        unsigned char pubKeyHash[20];
        
        // Use SHA256 of private key as a simplified public key hash
        std::vector<uint8_t> sha256Result = SHA256Hash(privateKeyBytes);
        
        // For this demo, use first 20 bytes of SHA256 (instead of RIPEMD160)
        // In production, you would use proper RIPEMD160 after SHA256
        for (int i = 0; i < 20; i++) {
            pubKeyHash[i] = sha256Result[i];
        }
        
        // Create address payload: version byte (0x00 for mainnet) + pubKeyHash
        std::vector<uint8_t> addressPayload;
        addressPayload.push_back(0x00); // Version byte for P2PKH mainnet
        for (int i = 0; i < 20; i++) {
            addressPayload.push_back(pubKeyHash[i]);
        }
        
        // Calculate checksum (first 4 bytes of double SHA256)
        std::vector<uint8_t> firstHash = SHA256Hash(addressPayload);
        std::vector<uint8_t> secondHash = SHA256Hash(firstHash);
        
        // Append checksum to payload
        for (int i = 0; i < 4; i++) {
            addressPayload.push_back(secondHash[i]);
        }
        
        // Encode with Base58
        return EncodeBase58(addressPayload);
        
    } catch (const std::exception&) {
        // Fallback to a recognizable demo address if crypto fails
        return "1Demo" + std::to_string(rand() % 100000) + "BitcoinAddress";
    }
}
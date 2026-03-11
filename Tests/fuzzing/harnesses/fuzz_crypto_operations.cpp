// fuzz_crypto_operations.cpp - Fuzzer for cryptographic operations
// Tests: ECDSA signing/verification, hash functions, key derivation, encryption

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <array>

#include "../../../backend/core/include/Crypto.h"

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    // Test 1: Hash functions (SHA256, RIPEMD160)
    {
        // SHA256
        std::array<uint8_t, 32> sha256Hash;
        Crypto::SHA256(data, size, sha256Hash);
        
        // RIPEMD160
        std::array<uint8_t, 20> ripemdHash;
        Crypto::RIPEMD160(data, size, ripemdHash);
        
        // HMAC-SHA256
        if (size > 32) {
            std::vector<uint8_t> key(data, data + 32);
            std::vector<uint8_t> hmac(32);
            Crypto::HMAC_SHA256(key, data + 32, size - 32, hmac);
        }
        
        // HMAC-SHA512
        if (size > 32) {
            std::vector<uint8_t> key(data, data + 32);
            std::vector<uint8_t> hmac(64);
            Crypto::HMAC_SHA512(key, data + 32, size - 32, hmac);
        }
    }

    // Test 2: PBKDF2 key derivation
    if (size >= 40) {
        std::string password(reinterpret_cast<const char*>(data), 20);
        std::vector<uint8_t> derivedKey(32);
        Crypto::PBKDF2_HMAC_SHA256(password, data + 20, 20, 10000, derivedKey);
    }

    // Test 3: BIP32 key derivation (if we have enough data)
    if (size >= 64) {
        // Create a seed from the fuzzed data
        std::array<uint8_t, 64> seed;
        std::memcpy(seed.data(), data, 64);
        
        // Derive master key
        Crypto::BIP32ExtendedKey masterKey;
        bool derived = Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);
        (void)derived;
        
        // Test child key derivation with various indices
        if (derived) {
            for (uint32_t index : {0u, 1u, 0x80000000u, 0x80000001u}) {
                Crypto::BIP32ExtendedKey childKey;
                bool childDerived = Crypto::BIP32_DeriveChild(masterKey, index, childKey);
                (void)childDerived;
            }
            
            // Test path derivation
            std::string path = "m/44'/0'/0'/0/0";
            Crypto::BIP32ExtendedKey pathKey;
            bool pathDerived = Crypto::BIP32_DerivePath(masterKey, path, pathKey);
            (void)pathDerived;
            
            // Test address generation
            std::string btcAddress;
            bool btcGen = Crypto::BIP32_GetBitcoinAddress(masterKey, btcAddress, false);
            (void)btcGen;
            
            std::string segwitAddress;
            bool segwitGen = Crypto::BIP32_GetSegWitAddress(masterKey, segwitAddress, false);
            (void)segwitGen;
            
            std::string ethAddress;
            bool ethGen = Crypto::BIP32_GetEthereumAddress(masterKey, ethAddress);
            (void)ethGen;
            
            std::string wif;
            bool wifGen = Crypto::BIP32_GetWIF(masterKey, wif, false);
            (void)wifGen;
        }
    }

    // Test 4: BIP39 mnemonic operations
    if (size >= 16) {
        // Generate entropy from fuzzed data
        std::vector<uint8_t> entropy(data, data + std::min(size_t(32), size));
        
        // Convert to mnemonic (requires wordlist - skip if not available)
        std::vector<std::string> mnemonic;
        std::vector<std::string> wordlist;  // Would need actual BIP39 wordlist
        // bool converted = Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic);
        // (void)converted;
        
        // Test seed generation from mnemonic (skip for fuzzing - requires valid wordlist)
        // if (converted && !mnemonic.empty() && size >= 32) {
        //     std::string passphrase(reinterpret_cast<const char*>(data + 16), std::min(size_t(16), size - 16));
        //     std::array<uint8_t, 64> seed;
        //     Crypto::BIP39_SeedFromMnemonic(mnemonic, passphrase, seed);
        // }
        (void)entropy;  // Suppress unused warning
    }

    // Test 5: AES-GCM encryption/decryption
    if (size >= 80) {
        // Extract key (32 bytes), IV (16 bytes), plaintext
        std::vector<uint8_t> key(data, data + 32);
        std::vector<uint8_t> iv(data + 32, data + 48);
        std::vector<uint8_t> plaintext(data + 48, data + size);
        std::vector<uint8_t> aad;  // Additional authenticated data (empty)
        
        // Encrypt
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> tag;
        bool encrypted = Crypto::AES_GCM_Encrypt(key, iv, plaintext, aad, ciphertext, tag);
        
        // Decrypt (if encryption succeeded)
        if (encrypted && !ciphertext.empty()) {
            std::vector<uint8_t> decrypted;
            bool decrypted_ = Crypto::AES_GCM_Decrypt(key, iv, ciphertext, aad, tag, decrypted);
            (void)decrypted_;
        }
    }

    // Test 6: ECDSA operations (if we have enough data for keys)
    if (size >= 64) {
        // Create a private key from the fuzzed data
        std::vector<uint8_t> privateKey(data, data + 32);
        
        // Test key derivation
        std::vector<uint8_t> publicKey;
        bool derived = Crypto::DerivePublicKey(privateKey, publicKey);
        
        if (derived && !publicKey.empty()) {
            // Create a message hash
            std::array<uint8_t, 32> messageHash;
            if (size >= 96) {
                std::memcpy(messageHash.data(), data + 32, 32);
            } else {
                // Use SHA256 of the remaining data
                Crypto::SHA256(data + 32, size - 32, messageHash);
            }
            
            // Sign the hash
            Crypto::ECDSASignature signature;
            bool signed_ = Crypto::SignHash(privateKey, messageHash, signature);
            
            // Verify the signature (if signing succeeded)
            if (signed_) {
                bool verified = Crypto::VerifySignature(publicKey, messageHash, signature);
                (void)verified;
            }
        }
    }

    // Test 7: Address generation via BIP32/BIP44/BIP84
    if (size >= 64) {
        // Create a seed from the fuzzed data
        std::array<uint8_t, 64> seed;
        std::memcpy(seed.data(), data, 64);
        
        // Derive master key
        Crypto::BIP32ExtendedKey masterKey;
        bool derived = Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);
        
        if (derived) {
            // Generate Bitcoin address (BIP44)
            std::string btcAddress;
            bool btcGen = Crypto::BIP44_GetAddress(masterKey, 0, false, 0, btcAddress, false);
            (void)btcGen;
            
            // Generate SegWit address (BIP84)
            std::string segwitAddress;
            bool segwitGen = Crypto::BIP84_GetAddress(masterKey, 0, false, 0, segwitAddress, false);
            (void)segwitGen;
            
            // Generate Ethereum address
            std::string ethAddress;
            bool ethGen = Crypto::BIP32_GetEthereumAddress(masterKey, ethAddress);
            (void)ethGen;
        }
    }

    // Test 8: Edge cases and boundary conditions
    {
        // Test with empty input
        std::array<uint8_t, 32> emptyHash;
        Crypto::SHA256(nullptr, 0, emptyHash);
        
        // Test with very large input (if size is large)
        if (size > 1000) {
            std::array<uint8_t, 32> largeHash;
            Crypto::SHA256(data, 1000, largeHash);
        }
        
        // Test with null bytes
        if (size > 10) {
            std::vector<uint8_t> withNulls(data, data + size);
            withNulls[size / 2] = 0;
            withNulls[size / 3] = 0;
            std::array<uint8_t, 32> nullHash;
            Crypto::SHA256(withNulls.data(), withNulls.size(), nullHash);
        }
    }

    return 0;
}

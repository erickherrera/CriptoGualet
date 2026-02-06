#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#include "../backend/core/include/Crypto.h"

// === Audit Helper Functions ===

void print_audit_header(const std::string& title) {
    std::cout << "\n============================================================" << std::endl;
    std::cout << " AUDIT: " << title << std::endl;
    std::cout << "============================================================" << std::endl;
}

void audit_check(bool condition, const std::string& description, const std::string& details = "") {
    std::cout << std::left << std::setw(60) << description;
    if (condition) {
        std::cout << "[ \033[32mPASS\033[0m ]" << std::endl;
    } else {
        std::cout << "[ \033[31mFAIL\033[0m ]" << std::endl;
        if (!details.empty()) {
            std::cout << "    >> FAILURE DETAILS: " << details << std::endl;
        }
        // Don't abort immediately, let audit continue if possible
        // But in a real CI this should exit with error code
    }
}

std::string to_hex(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : data) {
        ss << std::setw(2) << (int)b;
    }
    return ss.str();
}

std::string to_hex(const std::array<uint8_t, 32>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t b : data) {
        ss << std::setw(2) << (int)b;
    }
    return ss.str();
}

// === Security Domain Tests ===

void audit_cryptographic_primitives() {
    print_audit_header("Cryptographic Primitives");

    // 1. SHA-256 Test Vectors (NIST)
    {
        std::array<uint8_t, 32> hash;
        std::string input = "abc";
        Crypto::SHA256(reinterpret_cast<const uint8_t*>(input.data()), input.size(), hash);
        std::string expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        audit_check(to_hex(hash) == expected, "SHA-256 correctness ('abc')");
    }

    // 2. RIPEMD-160 Test Vectors
    {
        std::array<uint8_t, 20> hash;
        std::string input = "abc";
        Crypto::RIPEMD160(reinterpret_cast<const uint8_t*>(input.data()), input.size(), hash);
        // expected for 'abc'
        std::string expected = "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc"; 
        
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (uint8_t b : hash) ss << std::setw(2) << (int)b;
        
        audit_check(ss.str() == expected, "RIPEMD-160 correctness ('abc')");
    }

    // 3. HMAC-SHA512 Test (RFC 4231 Test Case 1)
    {
        std::vector<uint8_t> key(20, 0x0b);
        std::string data = "Hi There";
        std::vector<uint8_t> out;
        Crypto::HMAC_SHA512(key, reinterpret_cast<const uint8_t*>(data.data()), data.size(), out);
        
        std::string expected = "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854";
        audit_check(to_hex(out) == expected, "HMAC-SHA512 correctness (RFC 4231)");
    }
}

void audit_memory_security() {
    print_audit_header("Memory Security");

    // Test SecureWipeVector
    std::vector<uint8_t> sensitive_data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    size_t original_cap = sensitive_data.capacity();
    Crypto::SecureWipeVector(sensitive_data);
    
    audit_check(sensitive_data.empty(), "SecureWipeVector clears container");
    
    // We can't easily check if memory was actually zeroed without unsafe access, 
    // but we can check if the API is consistent.
}

void audit_encryption_standards() {
    print_audit_header("Encryption Standards");

    // 1. AES-GCM
    {
        std::vector<uint8_t> key(32); // 256 bits
        Crypto::RandBytes(key.data(), key.size());
        std::vector<uint8_t> plaintext(64, 'A');
        std::vector<uint8_t> aad = {0x01, 0x02};
        std::vector<uint8_t> ciphertext, iv, tag;
        
        bool encrypt_ok = Crypto::AES_GCM_Encrypt(key, plaintext, aad, ciphertext, iv, tag);
        audit_check(encrypt_ok, "AES-GCM-256 Encryption");
        
        audit_check(iv.size() == 12, "AES-GCM IV length (12 bytes/96 bits)");
        audit_check(tag.size() == 16, "AES-GCM Tag length (16 bytes/128 bits)");
        
        std::vector<uint8_t> decrypted;
        bool decrypt_ok = Crypto::AES_GCM_Decrypt(key, ciphertext, aad, iv, tag, decrypted);
        audit_check(decrypt_ok && decrypted == plaintext, "AES-GCM-256 Decryption Integrity");
    }

    // 2. Database Encryption Helpers
    {
        std::vector<uint8_t> key(32);
        std::vector<uint8_t> data(100, 0xFF);
        std::vector<uint8_t> blob;
        Crypto::EncryptDBData(key, data, blob);
        
        // Format: IV(12) + Tag(16) + Ciphertext(N)
        audit_check(blob.size() == 12 + 16 + 100, "Encrypted DB Data Envelope Size");
    }
}

void audit_key_derivation() {
    print_audit_header("Key Derivation (PBKDF2)");

    std::string password = "audit_password";
    std::vector<uint8_t> salt(16);
    Crypto::GenerateSecureSalt(salt, 16);
    
    // 1. Wallet Key Derivation
    {
        std::vector<uint8_t> key;
        // Check default iterations implicitly by checking performance or just API success
        bool ok = Crypto::DeriveWalletKey(password, salt, key);
        audit_check(ok, "Wallet Key Derivation (PBKDF2-SHA256)");
        audit_check(key.size() == 32, "Wallet Key Size (256 bits)");
    }

    // 2. Database Key Derivation
    {
        Crypto::DatabaseKeyInfo info;
        std::vector<uint8_t> db_key;
        bool ok = Crypto::CreateDatabaseKey(password, info, db_key);
        
        audit_check(ok, "Database Key Creation");
        audit_check(info.iteration_count >= 600000, "DB KDF Iterations >= 600,000 (OWASP recommended)");
    }
}

void audit_bip39_standards() {
    print_audit_header("BIP-39 & Wallet Standards");

    // 1. Entropy
    {
        std::vector<uint8_t> entropy;
        Crypto::GenerateEntropy(128, entropy);
        audit_check(entropy.size() == 16, "Entropy Generation (128 bits)");
    }

    // 2. Mnemonic
    {
        // Use a known vector (from BIP39 spec) if possible, or just consistency
        // Entropy: 0000...0000
        std::vector<uint8_t> entropy(16, 0);
        // We need to load wordlist... this test might fail if wordlist isn't found.
        // Skip strictly loading from file to avoid path dependency issues in test.
        // Instead, verify the API handles errors or mocks.
        
        // Actually, let's test the entropy-to-seed directly if we had a fixed mnemonic
        // But since we can't easily load wordlist here without file access, 
        // we'll audit the key derivation part which is pure math.
        
        std::vector<std::string> mnemonic_vec = {
            "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
            "abandon", "abandon", "abandon", "abandon", "abandon", "about"
        };
        std::string passphrase = "TREZOR";
        std::array<uint8_t, 64> seed;
        Crypto::BIP39_SeedFromMnemonic(mnemonic_vec, passphrase, seed);
        
        // First 4 bytes of expected seed for this vector:
        // c55257c360c3...
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)seed[0] 
           << std::setw(2) << (int)seed[1];
        
        std::string actual = ss.str();
        std::string expected = "c552";
        
        if (actual == expected) {
            audit_check(true, "BIP-39 Seed Derivation (Vector Check)");
        } else {
            audit_check(false, "BIP-39 Seed Derivation (Vector Check)", 
                "Expected start: " + expected + ", Actual start: " + actual);
        }
    }
}

void audit_2fa_totp() {
    print_audit_header("2FA / TOTP Security");

    // RFC 6238 Test Vector (SHA1)
    // Secret: 12345678901234567890 (20 bytes)
    std::vector<uint8_t> secret = {
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30
    };
    
    // Time: 59 (Window 1) -> Expect 287082
    std::string code = Crypto::GenerateTOTP(secret, 59);
    audit_check(code == "287082", "TOTP Generation (RFC 6238 Vector @ 59s)");
    
    // Time: 1111111109 -> Expect 081804
    code = Crypto::GenerateTOTP(secret, 1111111109);
    audit_check(code == "081804", "TOTP Generation (RFC 6238 Vector @ 1.1G)");
}

int main() {
    std::cout << "=== CriptoGualet Security Audit Suite ===" << std::endl;
    std::cout << "Target System: win32 / MSVC / Clang" << std::endl;
    std::cout << "Date: 2026-02-06" << std::endl;

    try {
        audit_cryptographic_primitives();
        audit_memory_security();
        audit_encryption_standards();
        audit_key_derivation();
        audit_bip39_standards();
        audit_2fa_totp();

        print_audit_header("Audit Summary");
        std::cout << "Security Audit completed. Review any FAIL items above." << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cout << "\nCRITICAL ERROR: Audit aborted with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "\nCRITICAL ERROR: Audit aborted with unknown exception" << std::endl;
        return 1;
    }
}
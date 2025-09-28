#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../../include/Crypto.h"

void test_memory_security() {
    std::cout << "Testing memory security functions..." << std::endl;

    // Test SecureWipeVector
    std::vector<uint8_t> sensitive_data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};

    Crypto::SecureWipeVector(sensitive_data);
    assert(sensitive_data.empty());

    // Test SecureWipeString
    std::string sensitive_string = "super_secret_password_123";
    Crypto::SecureWipeString(sensitive_string);
    assert(sensitive_string.empty());

    std::cout << "Memory security functions: PASSED" << std::endl;
}

void test_secure_salt_generation() {
    std::cout << "Testing secure salt generation..." << std::endl;

    std::vector<uint8_t> salt1, salt2;

    // Test default size (32 bytes)
    assert(Crypto::GenerateSecureSalt(salt1));
    assert(salt1.size() == 32);

    // Test custom size
    assert(Crypto::GenerateSecureSalt(salt2, 64));
    assert(salt2.size() == 64);

    // Verify salts are different (extremely unlikely to be same with good RNG)
    salt2.resize(32);
    assert(salt1 != salt2);

    std::cout << "Secure salt generation: PASSED" << std::endl;
}

void test_wallet_key_derivation() {
    std::cout << "Testing wallet key derivation..." << std::endl;

    const std::string password = "test_password_123";
    std::vector<uint8_t> salt;
    assert(Crypto::GenerateSecureSalt(salt, 32));

    std::vector<uint8_t> key1, key2;

    // Test DeriveWalletKey
    assert(Crypto::DeriveWalletKey(password, salt, key1, 32));
    assert(key1.size() == 32);

    // Test same input produces same output
    assert(Crypto::DeriveWalletKey(password, salt, key2, 32));
    assert(key1 == key2);

    // Test different password produces different key
    std::vector<uint8_t> key3;
    assert(Crypto::DeriveWalletKey("different_password", salt, key3, 32));
    assert(key1 != key3);

    // Test DeriveDBEncryptionKey
    std::vector<uint8_t> db_key;
    assert(Crypto::DeriveDBEncryptionKey(password, salt, db_key));
    assert(db_key.size() == 32);

    std::cout << "Wallet key derivation: PASSED" << std::endl;
}

void test_aes_gcm_encryption() {
    std::cout << "Testing AES-GCM encryption/decryption..." << std::endl;

    // Generate a key
    std::vector<uint8_t> key;
    assert(Crypto::GenerateSecureSalt(key, 32)); // Use as 256-bit key

    // Test data
    const std::string plaintext_str = "This is secret data that needs to be encrypted!";
    std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());
    std::vector<uint8_t> aad; // No additional data

    // Encrypt
    std::vector<uint8_t> ciphertext, iv, tag;
    assert(Crypto::AES_GCM_Encrypt(key, plaintext, aad, ciphertext, iv, tag));

    assert(iv.size() == 12);
    assert(tag.size() == 16);
    assert(ciphertext.size() == plaintext.size());
    assert(ciphertext != plaintext); // Ensure it's actually encrypted

    // Decrypt
    std::vector<uint8_t> decrypted;
    assert(Crypto::AES_GCM_Decrypt(key, ciphertext, aad, iv, tag, decrypted));
    assert(decrypted == plaintext);

    // Test with wrong key fails
    std::vector<uint8_t> wrong_key;
    assert(Crypto::GenerateSecureSalt(wrong_key, 32));
    std::vector<uint8_t> wrong_decrypt;
    assert(!Crypto::AES_GCM_Decrypt(wrong_key, ciphertext, aad, iv, tag, wrong_decrypt));

    std::cout << "AES-GCM encryption/decryption: PASSED" << std::endl;
}

void test_database_encryption() {
    std::cout << "Testing database encryption..." << std::endl;

    // Generate a key
    std::vector<uint8_t> key;
    assert(Crypto::GenerateSecureSalt(key, 32));

    // Test data
    const std::string data_str = "Sensitive database record: User ID 12345, Balance: $50000";
    std::vector<uint8_t> data(data_str.begin(), data_str.end());

    // Encrypt
    std::vector<uint8_t> encrypted_blob;
    assert(Crypto::EncryptDBData(key, data, encrypted_blob));

    // Verify format: [IV(12)] + [TAG(16)] + [CIPHERTEXT]
    assert(encrypted_blob.size() >= 28); // At least IV + TAG
    assert(encrypted_blob.size() == 28 + data.size()); // IV + TAG + data

    // Decrypt
    std::vector<uint8_t> decrypted_data;
    assert(Crypto::DecryptDBData(key, encrypted_blob, decrypted_data));
    assert(decrypted_data == data);

    // Test with wrong key fails
    std::vector<uint8_t> wrong_key;
    assert(Crypto::GenerateSecureSalt(wrong_key, 32));
    std::vector<uint8_t> wrong_decrypt;
    assert(!Crypto::DecryptDBData(wrong_key, encrypted_blob, wrong_decrypt));

    std::cout << "Database encryption: PASSED" << std::endl;
}

void test_encrypted_seed_storage() {
    std::cout << "Testing encrypted seed storage..." << std::endl;

    const std::string password = "my_secure_wallet_password";
    const std::vector<std::string> mnemonic = {
        "abandon", "ability", "able", "about", "above", "absent",
        "absorb", "abstract", "absurd", "abuse", "access", "accident"
    };

    // Encrypt seed phrase
    Crypto::EncryptedSeed encrypted_seed;
    assert(Crypto::EncryptSeedPhrase(password, mnemonic, encrypted_seed));

    assert(encrypted_seed.salt.size() == 32);
    assert(!encrypted_seed.encrypted_data.empty());
    assert(encrypted_seed.verification_hash.size() == 32);

    // Decrypt seed phrase
    std::vector<std::string> decrypted_mnemonic;
    assert(Crypto::DecryptSeedPhrase(password, encrypted_seed, decrypted_mnemonic));
    assert(decrypted_mnemonic == mnemonic);

    // Test with wrong password fails
    std::vector<std::string> wrong_decrypt;
    assert(!Crypto::DecryptSeedPhrase("wrong_password", encrypted_seed, wrong_decrypt));

    std::cout << "Encrypted seed storage: PASSED" << std::endl;
}

void test_database_key_management() {
    std::cout << "Testing database key management..." << std::endl;

    const std::string password = "master_password_for_database";

    // Create database key
    Crypto::DatabaseKeyInfo key_info;
    std::vector<uint8_t> database_key;
    assert(Crypto::CreateDatabaseKey(password, key_info, database_key));

    assert(key_info.salt.size() == 32);
    assert(key_info.key_verification_hash.size() == 32);
    assert(key_info.iteration_count == 600000);
    assert(database_key.size() == 32);

    // Verify database key
    std::vector<uint8_t> verified_key;
    assert(Crypto::VerifyDatabaseKey(password, key_info, verified_key));
    assert(verified_key == database_key);

    // Test with wrong password fails
    std::vector<uint8_t> wrong_key;
    assert(!Crypto::VerifyDatabaseKey("wrong_password", key_info, wrong_key));

    std::cout << "Database key management: PASSED" << std::endl;
}

void test_integration_scenario() {
    std::cout << "Testing integration scenario..." << std::endl;

    // Simulate a complete wallet security flow
    const std::string user_password = "MySecureWalletPassword123!";
    const std::vector<std::string> seed_words = {
        "abandon", "ability", "able", "about", "above", "absent",
        "absorb", "abstract", "absurd", "abuse", "access", "accident",
        "account", "accuse", "achieve", "acid", "acoustic", "acquire",
        "across", "act", "action", "actor", "actress", "actual"
    };

    // 1. Create database encryption key
    Crypto::DatabaseKeyInfo db_key_info;
    std::vector<uint8_t> db_key;
    assert(Crypto::CreateDatabaseKey(user_password, db_key_info, db_key));

    // 2. Encrypt seed phrase
    Crypto::EncryptedSeed encrypted_seed;
    assert(Crypto::EncryptSeedPhrase(user_password, seed_words, encrypted_seed));

    // 3. Encrypt some wallet data using database encryption
    const std::string wallet_data = "Wallet balance: 1.5 BTC, Address: bc1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh";
    std::vector<uint8_t> wallet_data_vec(wallet_data.begin(), wallet_data.end());

    std::vector<uint8_t> encrypted_wallet_data;
    assert(Crypto::EncryptDBData(db_key, wallet_data_vec, encrypted_wallet_data));

    // 4. Simulate app restart - verify password and decrypt everything
    std::vector<uint8_t> verified_db_key;
    assert(Crypto::VerifyDatabaseKey(user_password, db_key_info, verified_db_key));

    std::vector<std::string> decrypted_seed;
    assert(Crypto::DecryptSeedPhrase(user_password, encrypted_seed, decrypted_seed));

    std::vector<uint8_t> decrypted_wallet_data;
    assert(Crypto::DecryptDBData(verified_db_key, encrypted_wallet_data, decrypted_wallet_data));

    // 5. Verify everything matches
    assert(verified_db_key == db_key);
    assert(decrypted_seed == seed_words);
    assert(decrypted_wallet_data == wallet_data_vec);

    // 6. Clean up sensitive data
    Crypto::SecureWipeVector(db_key);
    Crypto::SecureWipeVector(verified_db_key);
    Crypto::SecureWipeVector(wallet_data_vec);
    Crypto::SecureWipeVector(decrypted_wallet_data);

    std::cout << "Integration scenario: PASSED" << std::endl;
}

int main() {
    std::cout << "=== CriptoGualet Security Enhancements Test Suite ===" << std::endl;
    std::cout << std::endl;

    try {
        test_memory_security();
        test_secure_salt_generation();
        test_wallet_key_derivation();
        test_aes_gcm_encryption();
        test_database_encryption();
        test_encrypted_seed_storage();
        test_database_key_management();
        test_integration_scenario();

        std::cout << std::endl;
        std::cout << "=== ALL TESTS PASSED! ===" << std::endl;
        std::cout << "Security enhancements are working correctly." << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
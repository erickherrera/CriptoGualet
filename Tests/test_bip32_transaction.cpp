// Test file for BIP32/BIP44 key derivation and transaction signing
// Tests Phase 2 implementation: HD wallet derivation, transaction signing, UTXO management

#include "../backend/core/include/Crypto.h"
#include <secp256k1.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <vector>
#include <string>

// Helper function to print test results
void printTest(const std::string& testName, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << testName << std::endl;
    if (!passed) {
        std::cerr << "  ERROR: Test failed!" << std::endl;
    }
}

// Helper function to print hex data
void printHex(const std::string& label, const std::vector<uint8_t>& data) {
    std::cout << label << ": ";
    for (uint8_t byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

// Test 1: BIP32 master key generation from seed
bool test_BIP32_MasterKeyGeneration() {
    std::cout << "\n=== Test 1: BIP32 Master Key Generation ===" << std::endl;

    // Create a test BIP39 seed (normally this would come from a mnemonic)
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    bool success = Crypto::BIP32_MasterKeyFromSeed(seed, masterKey);

    if (success) {
        std::cout << "Master key generated successfully" << std::endl;
        std::cout << "Key depth: " << static_cast<int>(masterKey.depth) << std::endl;
        std::cout << "Is private: " << (masterKey.isPrivate ? "Yes" : "No") << std::endl;
        std::cout << "Key size: " << masterKey.key.size() << " bytes" << std::endl;
        std::cout << "Chain code size: " << masterKey.chainCode.size() << " bytes" << std::endl;
    }

    return success && masterKey.isPrivate && masterKey.depth == 0 &&
           masterKey.key.size() == 32 && masterKey.chainCode.size() == 32;
}

// Test 2: BIP32 child key derivation
bool test_BIP32_ChildDerivation() {
    std::cout << "\n=== Test 2: BIP32 Child Key Derivation ===" << std::endl;

    // Generate master key
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Derive first hardened child (m/0')
    Crypto::BIP32ExtendedKey child;
    uint32_t hardenedIndex = 0x80000000;  // Hardened derivation
    bool success = Crypto::BIP32_DeriveChild(masterKey, hardenedIndex, child);

    if (success) {
        std::cout << "Child key derived successfully" << std::endl;
        std::cout << "Child depth: " << static_cast<int>(child.depth) << std::endl;
        std::cout << "Child number: " << child.childNumber << std::endl;
        std::cout << "Is private: " << (child.isPrivate ? "Yes" : "No") << std::endl;
    }

    return success && child.depth == 1 && child.isPrivate;
}

// Test 3: BIP44 path derivation
bool test_BIP44_PathDerivation() {
    std::cout << "\n=== Test 3: BIP44 Path Derivation ===" << std::endl;

    // Generate master key
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Derive BIP44 path: m/44'/0'/0'/0/0 (first Bitcoin receiving address)
    std::string path = "m/44'/0'/0'/0/0";
    Crypto::BIP32ExtendedKey addressKey;
    bool success = Crypto::BIP32_DerivePath(masterKey, path, addressKey);

    if (success) {
        std::cout << "BIP44 address key derived successfully" << std::endl;
        std::cout << "Path: " << path << std::endl;
        std::cout << "Key depth: " << static_cast<int>(addressKey.depth) << std::endl;
        std::cout << "Child number: " << addressKey.childNumber << std::endl;
    }

    return success && addressKey.depth == 5;
}

// Test 4: Bitcoin address generation
bool test_BitcoinAddressGeneration() {
    std::cout << "\n=== Test 4: Bitcoin Address Generation ===" << std::endl;

    // Generate master key
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Derive BIP44 address
    Crypto::BIP32ExtendedKey addressKey;
    if (!Crypto::BIP32_DerivePath(masterKey, "m/44'/0'/0'/0/0", addressKey)) {
        return false;
    }

    // Generate Bitcoin address
    std::string address;
    bool success = Crypto::BIP32_GetBitcoinAddress(addressKey, address);

    if (success) {
        std::cout << "Bitcoin address generated successfully" << std::endl;
        std::cout << "Address: " << address << std::endl;
        std::cout << "Address length: " << address.length() << " characters" << std::endl;
    }

    return success && !address.empty() && address[0] == '1';  // P2PKH addresses start with '1'
}

// Test 5: BIP44 multiple address generation
bool test_BIP44_MultipleAddresses() {
    std::cout << "\n=== Test 5: BIP44 Multiple Address Generation ===" << std::endl;

    // Generate master key
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Generate first 5 receiving addresses
    std::vector<std::string> addresses;
    bool success = Crypto::BIP44_GenerateAddresses(masterKey, 0, false, 0, 5, addresses, false);

    if (success) {
        std::cout << "Generated " << addresses.size() << " addresses:" << std::endl;
        for (size_t i = 0; i < addresses.size(); i++) {
            std::cout << "  Address " << i << ": " << addresses[i] << std::endl;
        }
    }

    return success && addresses.size() == 5;
}

// Test 6: Transaction hash signing
bool test_TransactionSigning() {
    std::cout << "\n=== Test 6: Transaction Hash Signing ===" << std::endl;

    // Generate a private key
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Create a test transaction hash (normally from CreateTransactionSigHash)
    std::array<uint8_t, 32> txHash;
    for (size_t i = 0; i < 32; i++) {
        txHash[i] = static_cast<uint8_t>(i * 2);
    }

    // Sign the hash
    Crypto::ECDSASignature signature;
    bool signSuccess = Crypto::SignHash(masterKey.key, txHash, signature);

    if (signSuccess) {
        std::cout << "Transaction signed successfully" << std::endl;
        std::cout << "DER signature size: " << signature.der_encoded.size() << " bytes" << std::endl;
        std::cout << "R component size: " << signature.r.size() << " bytes" << std::endl;
        std::cout << "S component size: " << signature.s.size() << " bytes" << std::endl;
    }

    return signSuccess && !signature.der_encoded.empty() &&
           signature.r.size() == 32 && signature.s.size() == 32;
}

// Test 7: Signature verification
bool test_SignatureVerification() {
    std::cout << "\n=== Test 7: Signature Verification ===" << std::endl;

    // Generate a key pair
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Derive public key
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, masterKey.key.data())) {
        secp256k1_context_destroy(ctx);
        return false;
    }

    uint8_t pubkey_serialized[33];
    size_t pubkey_len = sizeof(pubkey_serialized);
    secp256k1_ec_pubkey_serialize(ctx, pubkey_serialized, &pubkey_len, &pubkey, SECP256K1_EC_COMPRESSED);
    std::vector<uint8_t> public_key(pubkey_serialized, pubkey_serialized + pubkey_len);
    secp256k1_context_destroy(ctx);

    // Create and sign a hash
    std::array<uint8_t, 32> txHash;
    for (size_t i = 0; i < 32; i++) {
        txHash[i] = static_cast<uint8_t>(i * 2);
    }

    Crypto::ECDSASignature signature;
    if (!Crypto::SignHash(masterKey.key, txHash, signature)) {
        return false;
    }

    // Verify the signature
    bool verifySuccess = Crypto::VerifySignature(public_key, txHash, signature);

    std::cout << "Signature verification: " << (verifySuccess ? "SUCCESS" : "FAILED") << std::endl;

    return verifySuccess;
}

// Test 8: UTXO coin selection
bool test_CoinSelection() {
    std::cout << "\n=== Test 8: UTXO Coin Selection ===" << std::endl;

    // Create some test UTXOs
    std::vector<Crypto::UTXO> available_utxos;

    Crypto::UTXO utxo1;
    utxo1.txid = "abc123";
    utxo1.vout = 0;
    utxo1.amount = 100000;  // 0.001 BTC
    utxo1.address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    available_utxos.push_back(utxo1);

    Crypto::UTXO utxo2;
    utxo2.txid = "def456";
    utxo2.vout = 1;
    utxo2.amount = 200000;  // 0.002 BTC
    utxo2.address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    available_utxos.push_back(utxo2);

    Crypto::UTXO utxo3;
    utxo3.txid = "ghi789";
    utxo3.vout = 0;
    utxo3.amount = 50000;   // 0.0005 BTC
    utxo3.address = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    available_utxos.push_back(utxo3);

    // Try to send 0.0015 BTC (150000 satoshis) with 10 sat/byte fee
    uint64_t target_amount = 150000;
    uint64_t fee_per_byte = 10;

    Crypto::CoinSelection selection;
    bool success = Crypto::SelectCoins(available_utxos, target_amount, fee_per_byte, selection);

    if (success) {
        std::cout << "Coin selection successful" << std::endl;
        std::cout << "Selected UTXOs: " << selection.selected_utxos.size() << std::endl;
        std::cout << "Total input: " << selection.total_input << " satoshis" << std::endl;
        std::cout << "Target amount: " << selection.target_amount << " satoshis" << std::endl;
        std::cout << "Fee: " << selection.fee << " satoshis" << std::endl;
        std::cout << "Change amount: " << selection.change_amount << " satoshis" << std::endl;
        std::cout << "Has change: " << (selection.has_change ? "Yes" : "No") << std::endl;
    }

    return success && selection.total_input >= target_amount + selection.fee;
}

// Test 9: Transaction size estimation
bool test_TransactionSizeEstimation() {
    std::cout << "\n=== Test 9: Transaction Size Estimation ===" << std::endl;

    // Estimate size for a transaction with 2 inputs and 2 outputs
    size_t input_count = 2;
    size_t output_count = 2;

    uint64_t estimated_size = Crypto::EstimateTransactionSize(input_count, output_count);

    std::cout << "Estimated transaction size: " << estimated_size << " bytes" << std::endl;
    std::cout << "For " << input_count << " inputs and " << output_count << " outputs" << std::endl;

    // Calculate fee at 10 sat/byte
    uint64_t fee = Crypto::CalculateFee(input_count, output_count, 10);
    std::cout << "Estimated fee (10 sat/byte): " << fee << " satoshis" << std::endl;

    // Reasonable size check (should be > 100 bytes and < 1000 bytes for this simple tx)
    return estimated_size > 100 && estimated_size < 1000 && fee > 0;
}

// Test 10: WIF private key export
bool test_WIFExport() {
    std::cout << "\n=== Test 10: WIF Private Key Export ===" << std::endl;

    // Generate master key
    std::array<uint8_t, 64> seed;
    for (size_t i = 0; i < 64; i++) {
        seed[i] = static_cast<uint8_t>(i);
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        return false;
    }

    // Export as WIF
    std::string wif;
    bool success = Crypto::BIP32_GetWIF(masterKey, wif, false);  // mainnet

    if (success) {
        std::cout << "WIF export successful" << std::endl;
        std::cout << "WIF key: " << wif << std::endl;
        std::cout << "WIF length: " << wif.length() << " characters" << std::endl;
    }

    // Mainnet compressed WIF should start with 'K' or 'L'
    return success && !wif.empty() && (wif[0] == 'K' || wif[0] == 'L');
}

int main() {
    std::cout << "\n";
    std::cout << "============================================\n";
    std::cout << "  BIP32/BIP44 & Transaction Signing Tests  \n";
    std::cout << "  Phase 2: Transaction Functionality       \n";
    std::cout << "============================================\n";

    int passed = 0;
    int total = 10;

    // Run all tests
    if (test_BIP32_MasterKeyGeneration()) passed++;
    if (test_BIP32_ChildDerivation()) passed++;
    if (test_BIP44_PathDerivation()) passed++;
    if (test_BitcoinAddressGeneration()) passed++;
    if (test_BIP44_MultipleAddresses()) passed++;
    if (test_TransactionSigning()) passed++;
    if (test_SignatureVerification()) passed++;
    if (test_CoinSelection()) passed++;
    if (test_TransactionSizeEstimation()) passed++;
    if (test_WIFExport()) passed++;

    // Print summary
    std::cout << "\n";
    std::cout << "============================================\n";
    std::cout << "  Test Summary: " << passed << "/" << total << " passed\n";
    std::cout << "============================================\n";

    if (passed == total) {
        std::cout << "\nAll tests PASSED! \u2713\n" << std::endl;
        return 0;
    } else {
        std::cout << "\nSome tests FAILED! \u2717\n" << std::endl;
        return 1;
    }
}

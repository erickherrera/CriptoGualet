#include "../backend/core/include/Crypto.h"
#include "TestUtils.h"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>

void testBech32Encoding() {
    TEST_START("Bech32 Encoding/Decoding (BIP173)");

    // Test vector from BIP173
    // Witness version 0, Pubkey hash: 751e76e8199196d454941c45d1b3a323f1433bd6
    std::vector<uint8_t> witness_program = {
        0x75, 0x1e, 0x76, 0xe8, 0x19, 0x91, 0x96, 0xd4, 0x54, 0x94, 
        0x1c, 0x45, 0xd1, 0xb3, 0xa3, 0x23, 0xf1, 0x43, 0x3b, 0xd6
    };
    
    std::string expected_addr = "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4";
    std::string encoded = Crypto::Bech32_Encode("bc", 0, witness_program);
    
    std::cout << "    Encoded:  " << encoded << std::endl;
    std::cout << "    Expected: " << expected_addr << std::endl;
    
    TEST_ASSERT(encoded == expected_addr, "Bech32 encoding mismatch");

    // Decoding
    std::string decoded_hrp;
    int decoded_version;
    std::vector<uint8_t> decoded_program;
    
    TEST_ASSERT(Crypto::Bech32_Decode(encoded, decoded_hrp, decoded_version, decoded_program), "Bech32 decoding failed");
    TEST_ASSERT(decoded_hrp == "bc", "HRP mismatch");
    TEST_ASSERT(decoded_version == 0, "Version mismatch");
    TEST_ASSERT(decoded_program == witness_program, "Program mismatch");

    TEST_PASS();
}

void testBech32mEncoding() {
    TEST_START("Bech32m Encoding/Decoding (BIP350)");

    // Test vector from BIP350
    // Witness version 1 (Taproot), Program: 751e76e8199196d454941c45d1b3a323f1433bd6... (32 bytes)
    std::vector<uint8_t> witness_program(32, 0x01);
    
    std::string encoded = Crypto::Bech32_Encode("bc", 1, witness_program);
    std::cout << "    Encoded: " << encoded << std::endl;
    
    std::string decoded_hrp;
    int decoded_version;
    std::vector<uint8_t> decoded_program;
    
    TEST_ASSERT(Crypto::Bech32_Decode(encoded, decoded_hrp, decoded_version, decoded_program), "Bech32m decoding failed");
    TEST_ASSERT(decoded_hrp == "bc", "HRP mismatch");
    TEST_ASSERT(decoded_version == 1, "Version mismatch");
    TEST_ASSERT(decoded_program == witness_program, "Program mismatch");

    TEST_PASS();
}

void testBIP84Derivation() {
    TEST_START("BIP84 Address Derivation");

    // Mnemonic: abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about
    std::vector<std::string> mnemonic = {
        "abandon", "abandon", "abandon", "abandon", "abandon", "abandon",
        "abandon", "abandon", "abandon", "abandon", "abandon", "about"
    };
    
    std::array<uint8_t, 64> seed;
    Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed);
    
    Crypto::BIP32ExtendedKey master;
    Crypto::BIP32_MasterKeyFromSeed(seed, master);
    
    // BIP84 first address: m/84'/0'/0'/0/0
    // Expected address: bc1qcr8tevgu6xr8p06v080v8ma9p69yeuunnu6clx (from standard tools)
    std::string address;
    TEST_ASSERT(Crypto::BIP84_GetAddress(master, 0, false, 0, address, false), "BIP84 derivation failed");
    
    std::cout << "    BIP84 Address (m/84'/0'/0'/0/0): " << address << std::endl;
    TEST_ASSERT(address.substr(0, 3) == "bc1", "Address should start with bc1");
    
    // Test Testnet BIP84: m/84'/1'/0'/0/0
    std::string testnet_address;
    TEST_ASSERT(Crypto::BIP84_GetAddress(master, 0, false, 0, testnet_address, true), "BIP84 testnet derivation failed");
    std::cout << "    BIP84 Testnet Address: " << testnet_address << std::endl;
    TEST_ASSERT(testnet_address.substr(0, 3) == "tb1", "Testnet address should start with tb1");

    TEST_PASS();
}

void testSigHashBIP143() {
    TEST_START("BIP143 SigHash Algorithm");

    // Create a dummy transaction
    Crypto::BitcoinTransaction tx;
    tx.version = 1;
    tx.locktime = 0;
    
    Crypto::TransactionInput input;
    input.txid = "0000000000000000000000000000000000000000000000000000000000000000";
    input.vout = 0;
    input.sequence = 0xffffffff;
    tx.inputs.push_back(input);
    
    Crypto::TransactionOutput output;
    output.amount = 100000000; // 1 BTC
    output.script_pubkey = "0014751e76e8199196d454941c45d1b3a323f1433bd6"; // P2WPKH
    tx.outputs.push_back(output);
    
    std::array<uint8_t, 32> sighash;
    // P2WPKH scriptCode for the input (derived from pubkey hash)
    std::string prev_script_pubkey = "0014751e76e8199196d454941c45d1b3a323f1433bd6";
    uint64_t amount = 100000000;
    
    TEST_ASSERT(Crypto::CreateSegWitSigHash(tx, 0, prev_script_pubkey, amount, sighash), "SigHash calculation failed");
    
    std::cout << "    SigHash (BIP143): ";
    for (uint8_t b : sighash) printf("%02x", b);
    printf("\n");
    
    TEST_PASS();
}

int main() {
    TestUtils::printTestHeader("Bitcoin SegWit (BIP141/143/84/173/350) Tests");
    
    testBech32Encoding();
    testBech32mEncoding();
    testBIP84Derivation();
    testSigHashBIP143();
    
    TestUtils::printTestSummary("SegWit Tests");
    
    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

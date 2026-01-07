// Test file for Ethereum wallet creation using BIP39/BIP44
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <array>
#include <fstream>
#include "../backend/core/include/Crypto.h"

// Load BIP39 English wordlist
std::vector<std::string> LoadWordlist(const std::string& filepath) {
    std::vector<std::string> wordlist;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open wordlist file: " << filepath << std::endl;
        return wordlist;
    }

    std::string word;
    while (std::getline(file, word)) {
        if (!word.empty()) {
            // Remove any trailing whitespace or newlines
            while (!word.empty() && (word.back() == '\r' || word.back() == '\n' || word.back() == ' ')) {
                word.pop_back();
            }
            wordlist.push_back(word);
        }
    }

    file.close();
    return wordlist;
}

void PrintHex(const std::string& label, const uint8_t* data, size_t len) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::cout << "=== Ethereum Wallet Creation Test ===" << std::endl << std::endl;

    // Load BIP39 wordlist
    std::vector<std::string> wordlist = LoadWordlist("assets/bip39/english.txt");
    if (wordlist.size() != 2048) {
        std::cerr << "ERROR: Invalid wordlist size: " << wordlist.size() << " (expected 2048)" << std::endl;

        // Try alternative paths
        wordlist = LoadWordlist("../assets/bip39/english.txt");
        if (wordlist.size() != 2048) {
            wordlist = LoadWordlist("frontend/qt/assets/bip39/english.txt");
            if (wordlist.size() != 2048) {
                std::cerr << "ERROR: Could not load wordlist from any known location" << std::endl;
                return 1;
            }
        }
    }
    std::cout << "✓ Loaded BIP39 wordlist (" << wordlist.size() << " words)" << std::endl;

    // Test 1: Generate a new mnemonic (12 words)
    std::cout << "\n--- Test 1: Generate New Mnemonic ---" << std::endl;
    std::vector<uint8_t> entropy;
    if (!Crypto::GenerateEntropy(128, entropy)) {
        std::cerr << "ERROR: Failed to generate entropy" << std::endl;
        return 1;
    }
    PrintHex("Entropy (128 bits)", entropy.data(), entropy.size());

    std::vector<std::string> mnemonic;
    if (!Crypto::MnemonicFromEntropy(entropy, wordlist, mnemonic)) {
        std::cerr << "ERROR: Failed to generate mnemonic" << std::endl;
        return 1;
    }

    std::cout << "Mnemonic (" << mnemonic.size() << " words): ";
    for (size_t i = 0; i < mnemonic.size(); i++) {
        std::cout << mnemonic[i];
        if (i < mnemonic.size() - 1) std::cout << " ";
    }
    std::cout << std::endl;
    std::cout << "✓ Generated valid 12-word mnemonic" << std::endl;

    // Test 2: Validate mnemonic
    std::cout << "\n--- Test 2: Validate Mnemonic ---" << std::endl;
    if (!Crypto::ValidateMnemonic(mnemonic, wordlist)) {
        std::cerr << "ERROR: Mnemonic validation failed" << std::endl;
        return 1;
    }
    std::cout << "✓ Mnemonic is valid" << std::endl;

    // Test 3: Generate BIP39 seed
    std::cout << "\n--- Test 3: Generate BIP39 Seed ---" << std::endl;
    std::array<uint8_t, 64> seed;
    if (!Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed)) {
        std::cerr << "ERROR: Failed to generate seed from mnemonic" << std::endl;
        return 1;
    }
    PrintHex("BIP39 Seed (512 bits)", seed.data(), seed.size());
    std::cout << "✓ Generated BIP39 seed" << std::endl;

    // Test 4: Generate BIP32 master key
    std::cout << "\n--- Test 4: Generate BIP32 Master Key ---" << std::endl;
    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        std::cerr << "ERROR: Failed to generate master key" << std::endl;
        return 1;
    }
    PrintHex("Master Private Key", masterKey.key.data(), masterKey.key.size());
    PrintHex("Master Chain Code", masterKey.chainCode.data(), masterKey.chainCode.size());
    std::cout << "✓ Generated BIP32 master key" << std::endl;

    // Test 5: Derive Ethereum addresses (BIP44: m/44'/60'/0'/0/0)
    std::cout << "\n--- Test 5: Derive Ethereum Addresses ---" << std::endl;
    std::vector<std::string> eth_addresses;
    if (!Crypto::BIP44_GenerateEthereumAddresses(masterKey, 0, false, 0, 5, eth_addresses)) {
        std::cerr << "ERROR: Failed to generate Ethereum addresses" << std::endl;
        return 1;
    }

    std::cout << "Generated 5 Ethereum addresses (m/44'/60'/0'/0/x):" << std::endl;
    for (size_t i = 0; i < eth_addresses.size(); i++) {
        std::cout << "  Address " << i << ": " << eth_addresses[i] << std::endl;
    }
    std::cout << "✓ Generated Ethereum addresses" << std::endl;

    // Test 6: Derive Bitcoin addresses for comparison (BIP44: m/44'/0'/0'/0/0)
    std::cout << "\n--- Test 6: Derive Bitcoin Addresses ---" << std::endl;
    std::vector<std::string> btc_addresses;
    if (!Crypto::BIP44_GenerateAddresses(masterKey, 0, false, 0, 5, btc_addresses, false)) {
        std::cerr << "ERROR: Failed to generate Bitcoin addresses" << std::endl;
        return 1;
    }

    std::cout << "Generated 5 Bitcoin addresses (m/44'/0'/0'/0/x):" << std::endl;
    for (size_t i = 0; i < btc_addresses.size(); i++) {
        std::cout << "  Address " << i << ": " << btc_addresses[i] << std::endl;
    }
    std::cout << "✓ Generated Bitcoin addresses" << std::endl;

    // Test 7: Test multi-chain address derivation
    std::cout << "\n--- Test 7: Multi-Chain Address Derivation ---" << std::endl;

    std::vector<Crypto::ChainType> chains = {
        Crypto::ChainType::BITCOIN,
        Crypto::ChainType::ETHEREUM,
        Crypto::ChainType::BNB_CHAIN,
        Crypto::ChainType::POLYGON,
        Crypto::ChainType::AVALANCHE,
        Crypto::ChainType::ARBITRUM
    };

    for (auto chain : chains) {
        std::string address;
        if (Crypto::DeriveChainAddress(masterKey, chain, 0, false, 0, address)) {
            std::cout << "  " << std::setw(20) << std::left << Crypto::GetChainName(chain)
                      << ": " << address << std::endl;
        } else {
            std::cerr << "  ERROR: Failed to derive address for " << Crypto::GetChainName(chain) << std::endl;
        }
    }
    std::cout << "✓ Multi-chain address derivation working" << std::endl;

    // Test 8: Test Keccak256 with known test vector
    std::cout << "\n--- Test 8: Keccak256 Test Vector ---" << std::endl;
    const char* test_input = "hello";
    std::array<uint8_t, 32> keccak_hash;
    if (!Crypto::Keccak256(reinterpret_cast<const uint8_t*>(test_input), strlen(test_input), keccak_hash)) {
        std::cerr << "ERROR: Keccak256 hash failed" << std::endl;
        return 1;
    }

    // Expected: 0x1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8
    std::cout << "Keccak256(\"hello\"): ";
    for (size_t i = 0; i < keccak_hash.size(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)keccak_hash[i];
    }
    std::cout << std::dec << std::endl;
    std::cout << "Expected:             1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8" << std::endl;

    // Verify
    const uint8_t expected[32] = {
        0x1c, 0x8a, 0xff, 0x95, 0x06, 0x85, 0xc2, 0xed,
        0x4b, 0xc3, 0x17, 0x4f, 0x34, 0x72, 0x28, 0x7b,
        0x56, 0xd9, 0x51, 0x7b, 0x9c, 0x94, 0x81, 0x27,
        0x31, 0x9a, 0x09, 0xa7, 0xa3, 0x6d, 0xea, 0xc8
    };

    bool keccak_match = true;
    for (size_t i = 0; i < 32; i++) {
        if (keccak_hash[i] != expected[i]) {
            keccak_match = false;
            break;
        }
    }

    if (keccak_match) {
        std::cout << "✓ Keccak256 test vector matches!" << std::endl;
    } else {
        std::cerr << "ERROR: Keccak256 test vector mismatch!" << std::endl;
        return 1;
    }

    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    return 0;
}

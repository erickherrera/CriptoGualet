// fuzz_wallet_operations.cpp - Fuzzer for wallet operations
// Tests: Balance calculations, transaction validation, fee estimation

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#include "../../../backend/core/include/Crypto.h"

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    // Test 1: Balance calculations with fuzzed values
    {
        // Extract multiple uint64_t values for balance testing
        std::vector<uint64_t> balances;
        for (size_t i = 0; i + 8 <= size; i += 8) {
            uint64_t value = 0;
            std::memcpy(&value, data + i, sizeof(uint64_t));
            balances.push_back(value);
        }
        
        // Calculate total balance
        uint64_t total = 0;
        bool overflow = false;
        for (uint64_t bal : balances) {
            if (total > UINT64_MAX - bal) {
                overflow = true;
                break;
            }
            total += bal;
        }
        (void)overflow;
        (void)total;
        
        // Test balance conversions (Satoshis to BTC)
        for (uint64_t satoshis : balances) {
            double btc = static_cast<double>(satoshis) / 100000000.0;
            uint64_t backToSat = static_cast<uint64_t>(btc * 100000000.0);
            (void)backToSat;
        }
    }

    // Test 2: Fee calculation with fuzzed parameters
    {
        if (size >= 16) {
            uint64_t txSize, feeRate;
            std::memcpy(&txSize, data, sizeof(uint64_t));
            std::memcpy(&feeRate, data + 8, sizeof(uint64_t));
            
            // Constrain to reasonable values
            txSize = txSize % 100000;  // Max 100KB
            feeRate = feeRate % 1000000;  // Max 10 sats/vbyte
            
            uint64_t fee = txSize * feeRate;
            (void)fee;
            
            // Test fee rate conversions
            double feePerByte = static_cast<double>(feeRate) / 1000.0;  // sats/KB to sats/byte
            (void)feePerByte;
        }
    }

    // Test 3: Transaction input validation
    {
        // Create fuzzed transaction inputs
        size_t numInputs = (size > 0) ? (data[0] % 10) + 1 : 1;
        
        for (size_t i = 0; i < numInputs && (1 + i * 40) < size; i++) {
            // Extract txid (32 bytes)
            std::vector<uint8_t> txidBytes(data + 1 + i * 40, 
                                         data + std::min(1 + i * 40 + 32, size));
            
            // Convert to hex string
            std::stringstream txid;
            for (uint8_t b : txidBytes) {
                txid << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
            std::string txidStr = txid.str();
            (void)txidStr;
            
            // Extract vout (4 bytes)
            uint32_t vout = 0;
            if (1 + i * 40 + 36 < size) {
                std::memcpy(&vout, data + 1 + i * 40 + 32, sizeof(uint32_t));
            }
            (void)vout;
        }
    }

    // Test 4: Amount validation and dust detection
    {
        if (size >= 8) {
            uint64_t amount;
            std::memcpy(&amount, data, sizeof(uint64_t));
            
            // Bitcoin dust threshold (typically 546 satoshis for P2PKH)
            const uint64_t DUST_THRESHOLD = 546;
            bool isDust = (amount < DUST_THRESHOLD && amount > 0);
            (void)isDust;
            
            // Check for valid amount ranges
            bool validRange = (amount <= 2100000000000000ULL);  // 21M BTC in satoshis
            (void)validRange;
        }
    }

    // Test 5: Address validation with various formats
    {
        // Test different address types
        std::string input(reinterpret_cast<const char*>(data), std::min(size, size_t(100)));
        
        // Bitcoin P2PKH (1...)
        if (input.length() >= 26 && input[0] == '1') {
            std::vector<uint8_t> decoded = Crypto::DecodeBase58(input);
            bool validP2PKH = (decoded.size() == 25);  // 1 version + 20 hash + 4 checksum
            (void)validP2PKH;
        }
        
        // Bitcoin P2SH (3...)
        if (input.length() >= 26 && input[0] == '3') {
            std::vector<uint8_t> decoded = Crypto::DecodeBase58(input);
            bool validP2SH = (decoded.size() == 25);
            (void)validP2SH;
        }
        
        // Bitcoin Bech32 (bc1...)
        if (input.length() >= 14 && input.substr(0, 3) == "bc1") {
            std::string hrp;
            int version;
            std::vector<uint8_t> program;
            bool validBech32 = Crypto::Bech32_Decode(input, hrp, version, program);
            (void)validBech32;
        }
        
        // Ethereum (0x...)
        if (input.length() >= 42 && input.substr(0, 2) == "0x") {
            bool validEth = true;
            for (size_t i = 2; i < 42 && i < input.length(); i++) {
                if (!isxdigit(input[i])) {
                    validEth = false;
                    break;
                }
            }
            (void)validEth;
        }
    }

    // Test 6: HD wallet path validation
    {
        // Test various derivation paths
        const char* paths[] = {
            "m/44'/0'/0'/0/0",      // BIP44 Bitcoin
            "m/44'/60'/0'/0/0",     // BIP44 Ethereum
            "m/49'/0'/0'/0/0",      // BIP49 SegWit
            "m/84'/0'/0'/0/0",      // BIP84 Native SegWit
            "m/0'",                  // Master key
            "m/9999'/9999'/9999'/9999/9999"  // Extreme path
        };
        
        for (const char* path : paths) {
            std::string pathStr(path);
            
            // Validate path format
            bool validFormat = (pathStr.substr(0, 2) == "m/");
            int hardenedCount = 0;
            int normalCount = 0;
            
            for (size_t i = 0; i < pathStr.length(); i++) {
                if (pathStr[i] == '\'') hardenedCount++;
                if (pathStr[i] == '/') normalCount++;
            }
            
            (void)validFormat;
            (void)hardenedCount;
            (void)normalCount;
        }
    }

    // Test 7: Mnemonic phrase validation
    {
        // Extract potential mnemonic words from fuzzed data
        std::string input(reinterpret_cast<const char*>(data), std::min(size, size_t(200)));
        
        // Split by spaces
        std::vector<std::string> words;
        std::stringstream ss(input);
        std::string word;
        while (ss >> word) {
            // Clean the word (remove punctuation)
            std::string clean;
            for (char c : word) {
                if (isalpha(c)) clean += tolower(c);
            }
            if (!clean.empty()) {
                words.push_back(clean);
            }
        }
        
        // Check word count (BIP39 uses 12, 15, 18, 21, or 24 words)
        bool validWordCount = (words.size() == 12 || words.size() == 15 || 
                              words.size() == 18 || words.size() == 21 || 
                              words.size() == 24);
        (void)validWordCount;
    }

    // Test 8: Transaction size estimation
    {
        // Estimate transaction size based on inputs and outputs
        size_t numInputs = (size > 0) ? (data[0] % 10) + 1 : 1;
        size_t numOutputs = (size > 1) ? (data[1] % 5) + 1 : 1;
        
        // Rough size estimation (bytes)
        size_t versionSize = 4;
        size_t inputCountSize = 1;
        size_t outputCountSize = 1;
        size_t locktimeSize = 4;
        
        // P2WPKH input: ~68 vbytes (witness + non-witness)
        size_t inputSize = numInputs * 68;
        
        // P2WPKH output: ~31 bytes
        size_t outputSize = numOutputs * 31;
        
        size_t totalSize = versionSize + inputCountSize + inputSize + 
                          outputCountSize + outputSize + locktimeSize;
        
        (void)totalSize;
    }

    // Test 9: Time lock validation
    {
        if (size >= 4) {
            uint32_t locktime;
            std::memcpy(&locktime, data, sizeof(uint32_t));
            
            // Interpret locktime
            // < 500000000: block height
            // >= 500000000: Unix timestamp
            bool isTimestamp = (locktime >= 500000000);
            (void)isTimestamp;
            
            // Check if locktime is in the past (simplified)
            uint32_t currentTime = 1700000000;  // Approx Nov 2023
            bool expired = (isTimestamp && locktime < currentTime);
            (void)expired;
        }
    }

    // Test 10: Multi-sig validation
    {
        // Test m-of-n multisig configurations
        if (size >= 2) {
            uint8_t m = data[0] % 16;  // Required signatures
            uint8_t n = data[1] % 16;  // Total keys
            
            bool validConfig = (m > 0 && m <= n && n <= 15);
            (void)validConfig;
        }
    }

    return 0;
}

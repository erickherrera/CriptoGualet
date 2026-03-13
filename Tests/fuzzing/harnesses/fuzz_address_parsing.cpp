// fuzz_address_parsing.cpp - Fuzzer for address parsing functions
// Tests: Bech32, Bech32m, Base58Check address validation and decoding

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "../../../backend/core/include/Crypto.h"

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    // Convert to string for address parsing tests
    std::string input(reinterpret_cast<const char*>(data), size);
    
    // Test 1: Bech32/Bech32m decoding (Bitcoin SegWit addresses)
    {
        std::string hrp;
        int version;
        std::vector<uint8_t> program;
        
        // Try to decode as Bech32/Bech32m
        // This should handle both valid and invalid inputs gracefully
        bool result = Crypto::Bech32_Decode(input, hrp, version, program);
        
        // If decoding succeeded, verify the result
        if (result) {
            // Re-encode and verify round-trip
            std::string reencoded = Crypto::Bech32_Encode(hrp, version, program);
            // Note: We don't assert here - fuzzer is looking for crashes/hangs
            (void)reencoded;
        }
    }

    // Test 2: Base58 decoding (legacy Bitcoin addresses)
    {
        std::vector<uint8_t> decoded = Crypto::DecodeBase58(input);
        
        // If we got valid Base58 data, try various operations
        if (!decoded.empty() && decoded.size() >= 20) {
            // Try to interpret as hash160 for P2PKH
            // This tests boundary conditions
            std::vector<uint8_t> hash160(decoded.begin(), decoded.begin() + 20);
            (void)hash160;
        }
        
        // Also test Base58Check decoding (with checksum)
        std::vector<uint8_t> payload;
        bool result = Crypto::DecodeBase58Check(input, payload);
        (void)result;
    }

    // Test 3: Address validation (comprehensive)
    {
        // Test various address formats that might be encountered
        // These are the formats supported by the wallet
        
        // Bitcoin: 1xxx (P2PKH), 3xxx (P2SH), bc1xxx (Bech32)
        // Litecoin: Lxxx, Mxxx, ltc1xxx
        // Ethereum: 0x...
        
        bool isValid = false;
        
        // Check if it looks like a Bitcoin address
        if (input.length() >= 26 && input.length() <= 42) {
            // Try to validate as Bitcoin address
            // This exercises the validation logic without requiring network access
            if ((input[0] == '1' || input[0] == '3') && input.length() >= 26) {
                // Legacy Base58 address
                std::vector<uint8_t> payload;
                isValid = Crypto::DecodeBase58Check(input, payload);
            } else if (input.substr(0, 3) == "bc1") {
                // Bech32 address
                std::string hrp;
                int version;
                std::vector<uint8_t> program;
                isValid = Crypto::Bech32_Decode(input, hrp, version, program);
            }
        }
        
        (void)isValid;
    }

    // Test 4: Edge cases and boundary conditions
    {
        // Test with null bytes in the middle
        if (size > 1) {
            std::string withNull(reinterpret_cast<const char*>(data), size);
            withNull[size / 2] = '\0';  // Inject null byte
            
            std::string hrp;
            int version;
            std::vector<uint8_t> program;
            (void)Crypto::Bech32_Decode(withNull, hrp, version, program);
            
            // Also test Base58 with null
            std::vector<uint8_t> decoded = Crypto::DecodeBase58(withNull);
            (void)decoded;
        }
        
        // Test with very long input
        if (size > 100) {
            std::string longInput(reinterpret_cast<const char*>(data), 100);
            std::string hrp;
            int version;
            std::vector<uint8_t> program;
            (void)Crypto::Bech32_Decode(longInput, hrp, version, program);
            
            std::vector<uint8_t> decoded = Crypto::DecodeBase58(longInput);
            (void)decoded;
        }
    }

    // Test 5: HRP variations (human-readable part)
    {
        // Test different HRPs used in cryptocurrency addresses
        const char* hrps[] = {"bc", "tb", "ltc", "tltc", "xrp", "cosmos"};
        
        for (const char* hrp : hrps) {
            // Try encoding with different witness versions
            for (int version = 0; version <= 16; version++) {
                // Create a dummy witness program of various sizes
                std::vector<uint8_t> program;
                if (version == 0) {
                    // v0 requires 20 or 32 bytes
                    program = std::vector<uint8_t>(20, 0x00);
                } else {
                    // Other versions can be 2-40 bytes
                    program = std::vector<uint8_t>(32, 0x00);
                }
                
                std::string encoded = Crypto::Bech32_Encode(hrp, version, program);
                
                if (!encoded.empty()) {
                    // Try to decode it back
                    std::string decodedHrp;
                    int decodedVersion;
                    std::vector<uint8_t> decodedProgram;
                    (void)Crypto::Bech32_Decode(encoded, decodedHrp, decodedVersion, decodedProgram);
                }
            }
        }
    }

    return 0;  // Return 0 to indicate no error (non-zero would stop fuzzing)
}

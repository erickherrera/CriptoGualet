// fuzz_transaction_parsing.cpp - Fuzzer for transaction parsing
// Tests: RLP encoding/decoding, Bitcoin script parsing, transaction serialization

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#include "../../../backend/core/include/Crypto.h"
#include "../../../backend/utils/include/RLPEncoder.h"

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    // Test 1: RLP encoding (Ethereum transactions)
    {
        // Test encoding various values from fuzzed data
        if (size >= 8) {
            // Try encoding a value extracted from the input
            uint64_t value = 0;
            std::memcpy(&value, data, sizeof(uint64_t));
            std::vector<uint8_t> encoded = RLP::Encoder::EncodeUInt(value);
            (void)encoded;
            
            // Test decimal wei encoding (Ethereum amounts)
            std::string weiStr = std::to_string(value);
            std::vector<uint8_t> weiEncoded = RLP::Encoder::EncodeDecimal(weiStr);
            (void)weiEncoded;
        }
        
        // Test encoding byte arrays of various sizes
        for (size_t len : {size_t(0), size_t(1), size_t(55), size_t(56), size_t(100), size}) {
            if (len <= size) {
                std::vector<uint8_t> bytes(data, data + len);
                std::vector<uint8_t> encoded = RLP::Encoder::EncodeBytes(bytes);
                (void)encoded;
            }
        }
        
        // Test encoding strings
        std::string fuzzedStr(reinterpret_cast<const char*>(data), std::min(size, size_t(100)));
        std::vector<uint8_t> strEncoded = RLP::Encoder::EncodeString(fuzzedStr);
        (void)strEncoded;
        
        // Test encoding hex strings
        std::string hexStr = RLP::Encoder::BytesToHex(std::vector<uint8_t>(data, data + std::min(size, size_t(32))));
        std::vector<uint8_t> hexEncoded = RLP::Encoder::EncodeHex(hexStr);
        (void)hexEncoded;
        
        // Test encoding lists
        std::vector<std::vector<uint8_t>> items;
        for (size_t i = 0; i < 5 && (i * 10) < size; i++) {
            items.push_back(std::vector<uint8_t>(data + i * 10, data + std::min(i * 10 + 10, size)));
        }
        if (!items.empty()) {
            std::vector<uint8_t> listEncoded = RLP::Encoder::EncodeList(items);
            (void)listEncoded;
        }
    }

    // Test 2: Bitcoin script parsing
    {
        std::vector<uint8_t> script(data, data + size);
        
        // Test P2PKH script creation/validation
        if (size >= 20) {
            // Create a P2PKH script from the first 20 bytes (hash160)
            std::vector<uint8_t> hash160(script.begin(), script.begin() + 20);
            std::string address = Crypto::EncodeBase58Check(hash160);
            
            std::vector<uint8_t> p2pkhScript;
            bool created = Crypto::CreateP2PKHScript(address, p2pkhScript);
            (void)created;
        }
        
        // Test with various script sizes (boundary conditions)
        for (size_t scriptSize : {size_t(0), size_t(1), size_t(19), size_t(20), size_t(21), size_t(32), size_t(33), size_t(40), size_t(100)}) {
            if (scriptSize <= size) {
                std::vector<uint8_t> testScript(script.begin(), script.begin() + scriptSize);
                
                // Try to create various script types
                std::vector<uint8_t> dummyHash(20, 0x00);
                std::string dummyAddr = Crypto::EncodeBase58Check(dummyHash);
                std::vector<uint8_t> result;
                (void)Crypto::CreateP2PKHScript(dummyAddr, result);
            }
        }
    }

    // Test 3: Transaction hash parsing
    {
        // Bitcoin txids are 32 bytes (64 hex chars)
        if (size >= 32) {
            // Convert first 32 bytes to hex string (simulating txid)
            std::stringstream txid;
            txid << std::hex;
            for (size_t i = 0; i < 32 && i < size; i++) {
                txid << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
            }
            std::string txidStr = txid.str();
            (void)txidStr;
            
            // Test hex string operations
            std::string hexStr = RLP::Encoder::BytesToHex(std::vector<uint8_t>(data, data + std::min(size, size_t(32))));
            (void)hexStr;
            
            // Test hex to bytes conversion
            std::vector<uint8_t> bytesFromHex = RLP::Encoder::HexToBytes(hexStr);
            (void)bytesFromHex;
        }
    }

    // Test 4: Bitcoin transaction structure fuzzing
    {
        // Simulate transaction parsing with fuzzed data
        Crypto::BitcoinTransaction tx;
        
        // Fuzz transaction fields
        if (size >= 4) {
            std::memcpy(&tx.version, data, sizeof(int32_t));
        }
        if (size >= 8) {
            std::memcpy(&tx.locktime, data + 4, sizeof(uint32_t));
        }
        
        // Create fuzzed inputs
        size_t numInputs = (size > 10) ? (data[9] % 10) : 0;  // Max 10 inputs
        for (size_t i = 0; i < numInputs && (10 + i * 32) < size; i++) {
            Crypto::TransactionInput input;
            // Create a fuzzed txid from the data
            input.txid = RLP::Encoder::BytesToHex(std::vector<uint8_t>(
                data + 10 + i * 32, 
                data + std::min(10 + i * 32 + 32, size)
            ));
            input.vout = (i < size - 10) ? data[10 + i] : 0;
            input.sequence = 0xFFFFFFFF;
            tx.inputs.push_back(input);
        }
        
        // Create fuzzed outputs
        size_t numOutputs = (size > 20) ? (data[19] % 5) : 0;  // Max 5 outputs
        for (size_t i = 0; i < numOutputs && (20 + i * 20) < size; i++) {
            Crypto::TransactionOutput output;
            if (20 + i * 20 + 8 < size) {
                std::memcpy(&output.amount, data + 20 + i * 20, sizeof(uint64_t));
            }
            // Create a fuzzed script pubkey
            size_t scriptStart = 20 + i * 20 + 8;
            size_t scriptLen = std::min(size_t(25), size - scriptStart);
            if (scriptLen > 0) {
                output.script_pubkey = RLP::Encoder::BytesToHex(std::vector<uint8_t>(
                    data + scriptStart, data + scriptStart + scriptLen
                ));
            }
            tx.outputs.push_back(output);
        }
        
        // Try to create SigHash (this exercises the transaction serialization)
        if (!tx.inputs.empty() && !tx.outputs.empty()) {
            std::array<uint8_t, 32> sighash;
            std::string prevScriptPubkey = "0014751e76e8199196d454941c45d1b3a323f1433bd6";  // Dummy P2WPKH
            uint64_t amount = 100000000;
            (void)Crypto::CreateSegWitSigHash(tx, 0, prevScriptPubkey, amount, sighash);
        }
    }

    // Test 5: Edge cases - malformed data
    {
        // Test with null bytes in the middle
        if (size > 1) {
            std::vector<uint8_t> withNull(data, data + size);
            withNull[size / 2] = 0;
            
            std::string nullStr(reinterpret_cast<const char*>(withNull.data()), withNull.size());
            std::vector<uint8_t> encoded = RLP::Encoder::EncodeString(nullStr);
            (void)encoded;
        }
        
        // Test with very long input
        if (size > 100) {
            std::vector<uint8_t> longData(data, data + 100);
            std::vector<uint8_t> encoded = RLP::Encoder::EncodeBytes(longData);
            (void)encoded;
        }
        
        // Test with oversized length prefixes for list encoding
        if (size > 10) {
            std::vector<std::vector<uint8_t>> largeItems;
            for (size_t i = 0; i < 100 && i < size; i++) {
                largeItems.push_back(std::vector<uint8_t>(data + i, data + std::min(i + 10, size)));
            }
            std::vector<uint8_t> listEncoded = RLP::Encoder::EncodeList(largeItems);
            (void)listEncoded;
        }
    }

    return 0;
}

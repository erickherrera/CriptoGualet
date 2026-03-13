// minimal_crash_test.cpp - Isolate transaction parsing crash
// Compile with: clang++ -fsanitize=address -g minimal_crash_test.cpp -o minimal_test

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Include the actual headers
#include "../../backend/core/include/Crypto.h"
#include "../../backend/utils/include/RLPEncoder.h"

// Crash input from fuzzer
const uint8_t crash_input[] = {0x74, 0xfe, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x74,
                               0x74, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xad, 0xe4, 0x31};
const size_t crash_size = sizeof(crash_input);

void test_encode_uint() {
    std::cout << "Testing EncodeUInt..." << std::endl;
    if (crash_size >= 8) {
        uint64_t value = 0;
        std::memcpy(&value, crash_input, sizeof(uint64_t));
        auto encoded = RLP::Encoder::EncodeUInt(value);
        std::cout << "  EncodeUInt passed (size=" << encoded.size() << ")" << std::endl;
    }
}

void test_encode_decimal() {
    std::cout << "Testing EncodeDecimal..." << std::endl;
    if (crash_size >= 8) {
        uint64_t value = 0;
        std::memcpy(&value, crash_input, sizeof(uint64_t));
        std::string weiStr = std::to_string(value);
        auto encoded = RLP::Encoder::EncodeDecimal(weiStr);
        std::cout << "  EncodeDecimal passed (size=" << encoded.size() << ")" << std::endl;
    }
}

void test_encode_bytes() {
    std::cout << "Testing EncodeBytes with various sizes..." << std::endl;
    for (size_t len : {size_t(0), size_t(1), size_t(21), size_t(55), size_t(56)}) {
        if (len <= crash_size) {
            std::vector<uint8_t> bytes(crash_input, crash_input + len);
            auto encoded = RLP::Encoder::EncodeBytes(bytes);
            std::cout << "  EncodeBytes(" << len << ") passed (encoded size=" << encoded.size()
                      << ")" << std::endl;
        }
    }
}

void test_encode_string() {
    std::cout << "Testing EncodeString..." << std::endl;
    std::string fuzzedStr(reinterpret_cast<const char*>(crash_input),
                          std::min(crash_size, size_t(100)));
    auto encoded = RLP::Encoder::EncodeString(fuzzedStr);
    std::cout << "  EncodeString passed (size=" << encoded.size() << ")" << std::endl;
}

void test_bytes_to_hex() {
    std::cout << "Testing BytesToHex..." << std::endl;
    std::vector<uint8_t> bytes(crash_input, crash_input + std::min(crash_size, size_t(32)));
    std::string hexStr = RLP::Encoder::BytesToHex(bytes);
    std::cout << "  BytesToHex passed (hex=" << hexStr << ")" << std::endl;
}

void test_encode_hex() {
    std::cout << "Testing EncodeHex..." << std::endl;
    std::vector<uint8_t> bytes(crash_input, crash_input + std::min(crash_size, size_t(32)));
    std::string hexStr = RLP::Encoder::BytesToHex(bytes);
    auto encoded = RLP::Encoder::EncodeHex(hexStr);
    std::cout << "  EncodeHex passed (size=" << encoded.size() << ")" << std::endl;
}

void test_encode_list() {
    std::cout << "Testing EncodeList..." << std::endl;
    std::vector<std::vector<uint8_t>> items;
    for (size_t i = 0; i < 5 && (i * 10) < crash_size; i++) {
        items.push_back(std::vector<uint8_t>(crash_input + i * 10,
                                             crash_input + std::min(i * 10 + 10, crash_size)));
    }
    if (!items.empty()) {
        auto encoded = RLP::Encoder::EncodeList(items);
        std::cout << "  EncodeList passed (size=" << encoded.size() << ")" << std::endl;
    }
}

void test_base58_encoding() {
    std::cout << "Testing Base58 encoding..." << std::endl;
    if (crash_size >= 20) {
        std::vector<uint8_t> hash160(crash_input, crash_input + 20);
        std::string address = Crypto::EncodeBase58Check(hash160);
        std::cout << "  EncodeBase58Check passed (address=" << address << ")" << std::endl;
    }
}

void test_p2pkh_script() {
    std::cout << "Testing P2PKH script creation..." << std::endl;
    if (crash_size >= 20) {
        std::vector<uint8_t> hash160(crash_input, crash_input + 20);
        std::string address = Crypto::EncodeBase58Check(hash160);
        std::vector<uint8_t> p2pkhScript;
        bool created = Crypto::CreateP2PKHScript(address, p2pkhScript);
        std::cout << "  CreateP2PKHScript passed (created=" << created << ")" << std::endl;
    }
}

void test_transaction_struct() {
    std::cout << "Testing BitcoinTransaction structure..." << std::endl;
    Crypto::BitcoinTransaction tx;

    if (crash_size >= 4) {
        std::memcpy(&tx.version, crash_input, sizeof(int32_t));
    }
    if (crash_size >= 8) {
        std::memcpy(&tx.locktime, crash_input + 4, sizeof(uint32_t));
    }

    // Create fuzzed inputs
    size_t numInputs = (crash_size > 10) ? (crash_input[9] % 10) : 0;
    for (size_t i = 0; i < numInputs && (10 + i * 32) < crash_size; i++) {
        Crypto::TransactionInput input;
        input.txid = RLP::Encoder::BytesToHex(std::vector<uint8_t>(
            crash_input + 10 + i * 32, crash_input + std::min(10 + i * 32 + 32, crash_size)));
        input.vout = (i < crash_size - 10) ? crash_input[10 + i] : 0;
        input.sequence = 0xFFFFFFFF;
        tx.inputs.push_back(input);
    }

    // Create fuzzed outputs
    size_t numOutputs = (crash_size > 20) ? (crash_input[19] % 5) : 0;
    for (size_t i = 0; i < numOutputs && (20 + i * 20) < crash_size; i++) {
        Crypto::TransactionOutput output;
        if (20 + i * 20 + 8 < crash_size) {
            std::memcpy(&output.amount, crash_input + 20 + i * 20, sizeof(uint64_t));
        }
        size_t scriptStart = 20 + i * 20 + 8;
        size_t scriptLen = std::min(size_t(25), crash_size - scriptStart);
        if (scriptLen > 0) {
            output.script_pubkey = RLP::Encoder::BytesToHex(std::vector<uint8_t>(
                crash_input + scriptStart, crash_input + scriptStart + scriptLen));
        }
        tx.outputs.push_back(output);
    }

    std::cout << "  Transaction struct created (inputs=" << tx.inputs.size()
              << ", outputs=" << tx.outputs.size() << ")" << std::endl;

    // Test CreateSegWitSigHash
    if (!tx.inputs.empty() && !tx.outputs.empty()) {
        std::cout << "  Testing CreateSegWitSigHash..." << std::endl;
        std::array<uint8_t, 32> sighash;
        std::string prevScriptPubkey = "0014751e76e8199196d454941c45d1b3a323f1433bd6";
        uint64_t amount = 100000000;
        bool result = Crypto::CreateSegWitSigHash(tx, 0, prevScriptPubkey, amount, sighash);
        std::cout << "  CreateSegWitSigHash passed (result=" << result << ")" << std::endl;
    }
}

int main() {
    std::cout << "=== Minimal Crash Test ===" << std::endl;
    std::cout << "Crash input size: " << crash_size << " bytes" << std::endl;
    std::cout << std::endl;

    try {
        test_encode_uint();
        test_encode_decimal();
        test_encode_bytes();
        test_encode_string();
        test_bytes_to_hex();
        test_encode_hex();
        test_encode_list();
        test_base58_encoding();
        test_p2pkh_script();
        test_transaction_struct();

        std::cout << std::endl << "=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}

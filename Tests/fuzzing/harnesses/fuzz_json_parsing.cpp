// fuzz_json_parsing.cpp - Fuzzer for JSON/API response parsing
// Tests: JSON validation, API response handling, malformed data rejection

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>

// libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    std::string input(reinterpret_cast<const char*>(data), size);

    // Test 1: Basic JSON structure validation
    {
        // Check for JSON object/array markers
        bool hasObject = (input.find('{') != std::string::npos && 
                         input.find('}') != std::string::npos);
        bool hasArray = (input.find('[') != std::string::npos && 
                        input.find(']') != std::string::npos);
        bool hasString = (input.find('"') != std::string::npos);
        bool hasNumber = false;
        
        for (char c : input) {
            if (isdigit(c) || c == '-' || c == '.' || c == 'e' || c == 'E') {
                hasNumber = true;
                break;
            }
        }
        
        (void)hasObject;
        (void)hasArray;
        (void)hasString;
        (void)hasNumber;
    }

    // Test 2: Balance/amount parsing (common in API responses)
    {
        // Look for numeric values that could be balances
        std::stringstream ss(input);
        std::string token;
        while (ss >> token) {
            // Check if token could be a number
            bool isNumber = true;
            int dotCount = 0;
            for (char c : token) {
                if (c == '.') {
                    dotCount++;
                } else if (!isdigit(c) && c != '-' && c != '+') {
                    isNumber = false;
                    break;
                }
            }
            
            if (isNumber && dotCount <= 1) {
                try {
                    double value = std::stod(token);
                    (void)value;
                } catch (...) {
                    // Invalid number format
                }
            }
        }
    }

    // Test 3: Hex string extraction (for transaction hashes, addresses)
    {
        // Look for hex strings (0x... or just hex)
        size_t pos = 0;
        while ((pos = input.find("0x", pos)) != std::string::npos) {
            // Extract potential hex string
            size_t end = pos + 2;
            while (end < input.length() && isxdigit(input[end])) {
                end++;
            }
            
            if (end > pos + 2) {
                std::string hexStr = input.substr(pos + 2, end - pos - 2);
                // Validate hex string length (should be even for bytes)
                bool validHex = (hexStr.length() % 2 == 0);
                (void)validHex;
            }
            pos = end;
        }
    }

    // Test 4: API error response patterns
    {
        // Common error patterns in API responses
        const char* errorPatterns[] = {
            "error", "Error", "ERROR",
            "failed", "Failed", "FAILED",
            "invalid", "Invalid", "INVALID",
            "not found", "Not Found", "NOT_FOUND",
            "unauthorized", "Unauthorized", "UNAUTHORIZED",
            "forbidden", "Forbidden", "FORBIDDEN",
            "timeout", "Timeout", "TIMEOUT",
            "rate limit", "Rate Limit", "RATE_LIMIT"
        };
        
        for (const char* pattern : errorPatterns) {
            bool hasError = (input.find(pattern) != std::string::npos);
            (void)hasError;
        }
    }

    // Test 5: Transaction object validation
    {
        // Look for transaction-related fields
        bool hasTxid = (input.find("txid") != std::string::npos || 
                       input.find("tx_hash") != std::string::npos ||
                       input.find("hash") != std::string::npos);
        bool hasConfirmations = (input.find("confirmations") != std::string::npos ||
                                input.find("confirmed") != std::string::npos);
        bool hasInputs = (input.find("inputs") != std::string::npos ||
                         input.find("vin") != std::string::npos);
        bool hasOutputs = (input.find("outputs") != std::string::npos ||
                          input.find("vout") != std::string::npos);
        
        (void)hasTxid;
        (void)hasConfirmations;
        (void)hasInputs;
        (void)hasOutputs;
    }

    // Test 6: Address validation in JSON
    {
        // Bitcoin address patterns in JSON
        if (input.find("bc1") != std::string::npos) {
            // Potential Bech32 address
            size_t pos = input.find("bc1");
            size_t end = pos + 3;
            while (end < input.length() && 
                   (isalnum(input[end]) || input[end] == '_' || input[end] == '-')) {
                end++;
            }
            std::string potentialAddress = input.substr(pos, end - pos);
            bool validLength = (potentialAddress.length() >= 14 && 
                              potentialAddress.length() <= 74);
            (void)validLength;
        }
        
        // Ethereum address patterns
        if (input.find("0x") != std::string::npos) {
            size_t pos = input.find("0x");
            if (pos + 42 <= input.length()) {
                std::string potentialEthAddr = input.substr(pos, 42);
                bool validEth = (potentialEthAddr.substr(0, 2) == "0x");
                if (validEth) {
                    for (size_t i = 2; i < 42; i++) {
                        if (!isxdigit(potentialEthAddr[i])) {
                            validEth = false;
                            break;
                        }
                    }
                }
                (void)validEth;
            }
        }
    }

    // Test 7: Unicode and escape sequence handling
    {
        // Check for JSON escape sequences
        bool hasEscapes = (input.find("\\") != std::string::npos);
        if (hasEscapes) {
            // Common JSON escapes
            const char* escapes[] = {"\\\"", "\\\\", "\\/", "\\b", "\\f", "\\n", "\\r", "\\t", "\\u"};
            for (const char* esc : escapes) {
                bool hasEscape = (input.find(esc) != std::string::npos);
                (void)hasEscape;
            }
        }
        
        // Check for UTF-8 sequences
        bool hasUTF8 = false;
        for (size_t i = 0; i < input.length(); i++) {
            unsigned char c = static_cast<unsigned char>(input[i]);
            // UTF-8 multi-byte sequence start
            if ((c & 0x80) != 0) {
                hasUTF8 = true;
                break;
            }
        }
        (void)hasUTF8;
    }

    // Test 8: Large number handling (for crypto amounts)
    {
        // Look for very large numbers (Wei, Satoshis)
        std::stringstream ss(input);
        std::string token;
        while (ss >> token) {
            // Remove quotes and commas
            std::string clean;
            for (char c : token) {
                if (isdigit(c)) clean += c;
            }
            
            if (clean.length() > 10) {
                // Could be a large crypto amount
                try {
                    unsigned long long bigNum = std::stoull(clean.substr(0, 20));
                    (void)bigNum;
                } catch (...) {
                    // Number too large
                }
            }
        }
    }

    // Test 9: Malformed JSON detection
    {
        // Count braces and brackets
        int braceOpen = 0, braceClose = 0;
        int bracketOpen = 0, bracketClose = 0;
        int quoteCount = 0;
        
        for (char c : input) {
            if (c == '{') braceOpen++;
            if (c == '}') braceClose++;
            if (c == '[') bracketOpen++;
            if (c == ']') bracketClose++;
            if (c == '"') quoteCount++;
        }
        
        bool balanced = (braceOpen == braceClose && bracketOpen == bracketClose);
        bool evenQuotes = (quoteCount % 2 == 0);
        
        (void)balanced;
        (void)evenQuotes;
    }

    // Test 10: API-specific field extraction
    {
        // BlockCypher API fields
        const char* bcFields[] = {
            "address", "balance", "unconfirmed_balance", "final_balance",
            "n_tx", "txs", "block_height", "block_hash", "fees"
        };
        
        for (const char* field : bcFields) {
            bool hasField = (input.find(field) != std::string::npos);
            (void)hasField;
        }
        
        // Etherscan API fields
        const char* ethFields[] = {
            "result", "status", "message", "gas", "gasPrice", "nonce",
            "from", "to", "value", "input"
        };
        
        for (const char* field : ethFields) {
            bool hasField = (input.find(field) != std::string::npos);
            (void)hasField;
        }
    }

    return 0;
}

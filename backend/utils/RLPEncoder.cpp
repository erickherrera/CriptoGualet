#include "include/RLPEncoder.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace RLP {

std::vector<uint8_t> Encoder::EncodeBytes(const std::vector<uint8_t>& data) {
    return encodeRaw(data);
}

std::vector<uint8_t> Encoder::EncodeString(const std::string& str) {
    std::vector<uint8_t> data(str.begin(), str.end());
    return encodeRaw(data);
}

std::vector<uint8_t> Encoder::EncodeUInt(uint64_t value) {
    if (value == 0) {
        // Empty array for zero
        return encodeRaw(std::vector<uint8_t>());
    }

    auto bytes = toBigEndian(value);
    return encodeRaw(bytes);
}

std::vector<uint8_t> Encoder::EncodeHex(const std::string& hex) {
    auto bytes = HexToBytes(hex);
    return encodeRaw(bytes);
}

std::vector<uint8_t> Encoder::EncodeList(const std::vector<std::vector<uint8_t>>& items) {
    // Concatenate all items
    std::vector<uint8_t> payload;
    for (const auto& item : items) {
        payload.insert(payload.end(), item.begin(), item.end());
    }

    // Encode as list
    if (payload.size() < 56) {
        // Short list: [0xc0 + length, ...data]
        std::vector<uint8_t> result;
        result.push_back(0xc0 + static_cast<uint8_t>(payload.size()));
        result.insert(result.end(), payload.begin(), payload.end());
        return result;
    } else {
        // Long list
        return encodeLength(payload.size(), 0xc0);
    }
}

std::vector<uint8_t> Encoder::HexToBytes(const std::string& hex) {
    std::string clean_hex = hex;

    // Remove 0x prefix if present
    if (clean_hex.size() >= 2 && clean_hex[0] == '0' && (clean_hex[1] == 'x' || clean_hex[1] == 'X')) {
        clean_hex = clean_hex.substr(2);
    }

    // Ensure even length
    if (clean_hex.size() % 2 != 0) {
        clean_hex = "0" + clean_hex;
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(clean_hex.size() / 2);

    for (size_t i = 0; i < clean_hex.size(); i += 2) {
        std::string byte_str = clean_hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

std::string Encoder::BytesToHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << "0x";
    oss << std::hex << std::setfill('0');

    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<int>(byte);
    }

    return oss.str();
}

std::vector<uint8_t> Encoder::encodeRaw(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        // Empty string: 0x80
        return {0x80};
    }

    if (data.size() == 1 && data[0] < 0x80) {
        // Single byte in range [0x00, 0x7f]: encode as itself
        return data;
    }

    if (data.size() < 56) {
        // Short string: [0x80 + length, ...data]
        std::vector<uint8_t> result;
        result.push_back(0x80 + static_cast<uint8_t>(data.size()));
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }

    // Long string: [0xb7 + length_of_length, ...length, ...data]
    auto length_bytes = toBigEndian(data.size());
    std::vector<uint8_t> result;
    result.push_back(0xb7 + static_cast<uint8_t>(length_bytes.size()));
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), data.begin(), data.end());
    return result;
}

std::vector<uint8_t> Encoder::toBigEndian(uint64_t value) {
    if (value == 0) {
        return {};  // Empty array for zero
    }

    std::vector<uint8_t> bytes;

    // Convert to big-endian, removing leading zeros
    while (value > 0) {
        bytes.insert(bytes.begin(), static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }

    return bytes;
}

std::vector<uint8_t> Encoder::encodeLength(size_t length, uint8_t offset) {
    if (length < 56) {
        return {static_cast<uint8_t>(offset + length)};
    }

    auto length_bytes = toBigEndian(length);
    std::vector<uint8_t> result;
    result.push_back(offset + 55 + static_cast<uint8_t>(length_bytes.size()));
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    return result;
}

} // namespace RLP

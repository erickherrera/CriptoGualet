#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace RLP {

/**
 * @brief RLP (Recursive Length Prefix) encoding utility for Ethereum transactions
 *
 * Implements RLP encoding as specified in the Ethereum Yellow Paper
 * Used for encoding transaction data before signing and broadcasting
 */
class Encoder {
public:
    /**
     * @brief Encode a byte array
     * @param data Input data
     * @return RLP-encoded data
     */
    static std::vector<uint8_t> EncodeBytes(const std::vector<uint8_t>& data);

    /**
     * @brief Encode a string
     * @param str Input string
     * @return RLP-encoded data
     */
    static std::vector<uint8_t> EncodeString(const std::string& str);

    /**
     * @brief Encode an unsigned integer (converts to big-endian bytes)
     * @param value Input value
     * @return RLP-encoded data
     */
    static std::vector<uint8_t> EncodeUInt(uint64_t value);

    /**
     * @brief Encode a decimal string (converts to big-endian bytes)
     * @param decimal Decimal string
     * @return RLP-encoded data
     */
    static std::vector<uint8_t> EncodeDecimal(const std::string& decimal);

    /**
     * @brief Encode a hex string (0x... format)
     * @param hex Hex string
     * @return RLP-encoded data
     */
    static std::vector<uint8_t> EncodeHex(const std::string& hex);

    /**
     * @brief Encode a list of items
     * @param items Vector of RLP-encoded items
     * @return RLP-encoded list
     */
    static std::vector<uint8_t> EncodeList(const std::vector<std::vector<uint8_t>>& items);

    /**
     * @brief Convert hex string to bytes (removes 0x prefix if present)
     * @param hex Hex string
     * @return Byte vector
     */
    static std::vector<uint8_t> HexToBytes(const std::string& hex);

    /**
     * @brief Convert bytes to hex string (adds 0x prefix)
     * @param data Byte vector
     * @return Hex string
     */
    static std::string BytesToHex(const std::vector<uint8_t>& data);

private:
    /**
     * @brief Encode raw bytes according to RLP specification
     * @param data Input data
     * @return RLP-encoded data
     */
    static std::vector<uint8_t> encodeRaw(const std::vector<uint8_t>& data);

    /**
     * @brief Convert uint64 to big-endian byte representation (minimal encoding)
     * @param value Input value
     * @return Byte vector
     */
    static std::vector<uint8_t> toBigEndian(uint64_t value);

    /**
     * @brief Encode the length of data
     * @param length Length to encode
     * @param offset Offset value (0x80 for strings, 0xc0 for lists)
     * @return Length prefix
     */
    static std::vector<uint8_t> encodeLength(size_t length, uint8_t offset);
};

} // namespace RLP

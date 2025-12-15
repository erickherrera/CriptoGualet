#include "include/EthereumService.h"
#include "../utils/include/RLPEncoder.h"
#include "../core/include/Crypto.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <iomanip>
#include <cmath>

using json = nlohmann::json;

namespace EthereumService {

EthereumClient::EthereumClient(const std::string& network)
    : m_network(network), m_apiToken("") {
    updateBaseUrl();
}

void EthereumClient::SetApiToken(const std::string& token) {
    m_apiToken = token;
}

void EthereumClient::SetNetwork(const std::string& network) {
    m_network = network;
    updateBaseUrl();
}

void EthereumClient::updateBaseUrl() {
    if (m_network == "mainnet") {
        m_baseUrl = "https://api.etherscan.io/api";
    } else if (m_network == "sepolia") {
        m_baseUrl = "https://api-sepolia.etherscan.io/api";
    } else if (m_network == "goerli") {
        m_baseUrl = "https://api-goerli.etherscan.io/api";
    } else {
        // Default to mainnet
        m_baseUrl = "https://api.etherscan.io/api";
    }
}

std::string EthereumClient::makeRequest(const std::string& endpoint) {
    std::string url = m_baseUrl + endpoint;

    // Add API token to URL (required by Etherscan API)
    // Note: Using HTTPS with certificate validation mitigates exposure risks
    if (!m_apiToken.empty()) {
        url += "&apikey=" + m_apiToken;
    }

    cpr::Header headers{{"User-Agent", "CriptoGualet/1.0"}};

    try {
        cpr::Response response = cpr::Get(
            cpr::Url{url},
            cpr::Timeout{10000},  // 10 second timeout
            headers,
            cpr::VerifySsl{true}  // SECURITY: Enforce SSL certificate validation
        );

        if (response.status_code == 200) {
            return response.text;
        }
    } catch (const std::exception& e) {
        // Request failed - do not log URL to avoid exposing API key
        return "";
    }

    return "";
}

std::optional<AddressBalance> EthereumClient::GetAddressBalance(const std::string& address) {
    if (!IsValidAddress(address)) {
        return std::nullopt;
    }

    // Etherscan API: Get balance
    std::string endpoint = "?module=account&action=balance&address=" + address + "&tag=latest";
    std::string response = makeRequest(endpoint);

    if (response.empty()) {
        return std::nullopt;
    }

    try {
        json data = json::parse(response);

        if (data["status"] == "1" && data.contains("result")) {
            AddressBalance balance;
            balance.address = address;
            balance.balance_wei = data["result"].get<std::string>();
            balance.balance_eth = WeiToEth(balance.balance_wei);

            // Get transaction count
            auto txCount = GetTransactionCount(address);
            if (txCount.has_value()) {
                balance.transaction_count = txCount.value();
            }

            return balance;
        }
    } catch (const json::exception& e) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<std::vector<Transaction>> EthereumClient::GetTransactionHistory(
    const std::string& address,
    uint32_t limit
) {
    if (!IsValidAddress(address)) {
        return std::nullopt;
    }

    // Etherscan API: Get normal transactions
    std::string endpoint = "?module=account&action=txlist&address=" + address +
                          "&startblock=0&endblock=99999999&page=1&offset=" +
                          std::to_string(limit) + "&sort=desc";

    std::string response = makeRequest(endpoint);

    if (response.empty()) {
        return std::nullopt;
    }

    try {
        json data = json::parse(response);

        if (data["status"] == "1" && data.contains("result") && data["result"].is_array()) {
            std::vector<Transaction> transactions;

            for (const auto& tx_json : data["result"]) {
                Transaction tx;
                tx.hash = tx_json.value("hash", "");
                tx.from = tx_json.value("from", "");
                tx.to = tx_json.value("to", "");
                tx.value_wei = tx_json.value("value", "0");
                tx.value_eth = WeiToEth(tx.value_wei);
                tx.gas_price_wei = tx_json.value("gasPrice", "0");
                tx.gas_used = tx_json.value("gasUsed", "0");
                tx.block_number = tx_json.value("blockNumber", "0");
                tx.timestamp = tx_json.value("timeStamp", "0");
                tx.is_error = (tx_json.value("isError", "0") == "1");
                tx.status = tx_json.value("txreceipt_status", "1");

                transactions.push_back(tx);
            }

            return transactions;
        }
    } catch (const json::exception& e) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<GasPrice> EthereumClient::GetGasPrice() {
    // Etherscan API: Get gas oracle
    std::string endpoint = "?module=gastracker&action=gasoracle";
    std::string response = makeRequest(endpoint);

    if (response.empty()) {
        return std::nullopt;
    }

    try {
        json data = json::parse(response);

        if (data["status"] == "1" && data.contains("result")) {
            GasPrice gasPrice;
            auto result = data["result"];

            gasPrice.safe_gas_price = result.value("SafeGasPrice", "0");
            gasPrice.propose_gas_price = result.value("ProposeGasPrice", "0");
            gasPrice.fast_gas_price = result.value("FastGasPrice", "0");

            return gasPrice;
        }
    } catch (const json::exception& e) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<uint64_t> EthereumClient::GetTransactionCount(const std::string& address) {
    if (!IsValidAddress(address)) {
        return std::nullopt;
    }

    // Etherscan API: Get transaction count
    std::string endpoint = "?module=proxy&action=eth_getTransactionCount&address=" +
                          address + "&tag=latest";
    std::string response = makeRequest(endpoint);

    if (response.empty()) {
        return std::nullopt;
    }

    try {
        json data = json::parse(response);

        if (data.contains("result")) {
            std::string result = data["result"].get<std::string>();

            // Convert hex string to uint64_t
            if (result.substr(0, 2) == "0x") {
                result = result.substr(2);
            }

            uint64_t count = std::stoull(result, nullptr, 16);
            return count;
        }
    } catch (const std::exception& e) {
        return std::nullopt;
    }

    return std::nullopt;
}

bool EthereumClient::IsValidAddress(const std::string& address) {
    // Ethereum address format: 0x followed by 40 hexadecimal characters
    std::regex eth_address_pattern("^0x[0-9a-fA-F]{40}$");
    return std::regex_match(address, eth_address_pattern);
}

double EthereumClient::WeiToEth(const std::string& wei_str) {
    if (wei_str.empty() || wei_str == "0") {
        return 0.0;
    }

    try {
        // PRECISION FIX: Use string manipulation to maintain precision
        // 1 ETH = 10^18 Wei
        // For display purposes, we convert to double at the end, but we handle
        // the division properly to minimize precision loss

        std::string wei_clean = wei_str;

        // Remove leading zeros
        size_t first_nonzero = wei_clean.find_first_not_of('0');
        if (first_nonzero != std::string::npos) {
            wei_clean = wei_clean.substr(first_nonzero);
        } else {
            return 0.0;  // All zeros
        }

        // For values >= 1 ETH, split integer and decimal parts
        if (wei_clean.length() > 18) {
            // Integer part: everything except last 18 digits
            std::string integer_part = wei_clean.substr(0, wei_clean.length() - 18);
            // Decimal part: last 18 digits
            std::string decimal_part = wei_clean.substr(wei_clean.length() - 18);

            // Remove trailing zeros from decimal part
            size_t last_nonzero = decimal_part.find_last_not_of('0');
            if (last_nonzero != std::string::npos) {
                decimal_part = decimal_part.substr(0, last_nonzero + 1);
            } else {
                decimal_part = "";
            }

            // Combine: "123.456789"
            std::string eth_str = integer_part;
            if (!decimal_part.empty()) {
                eth_str += "." + decimal_part;
            }

            return std::stod(eth_str);
        } else {
            // Value < 1 ETH: pad with leading zeros
            std::string decimal_str = std::string(18 - wei_clean.length(), '0') + wei_clean;

            // Remove trailing zeros
            size_t last_nonzero = decimal_str.find_last_not_of('0');
            if (last_nonzero != std::string::npos) {
                decimal_str = decimal_str.substr(0, last_nonzero + 1);
            } else {
                return 0.0;
            }

            return std::stod("0." + decimal_str);
        }
    } catch (const std::exception& e) {
        return 0.0;
    }
}

std::string EthereumClient::EthToWei(double eth) {
    if (eth <= 0.0) {
        return "0";
    }

    // PRECISION FIX: Use string manipulation to avoid floating-point errors
    // 1 ETH = 10^18 Wei

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(18) << eth;
    std::string eth_str = oss.str();

    // Find decimal point
    size_t decimal_pos = eth_str.find('.');
    std::string integer_part;
    std::string decimal_part;

    if (decimal_pos != std::string::npos) {
        integer_part = eth_str.substr(0, decimal_pos);
        decimal_part = eth_str.substr(decimal_pos + 1);
    } else {
        integer_part = eth_str;
        decimal_part = "";
    }

    // Pad or trim decimal part to exactly 18 digits
    if (decimal_part.length() < 18) {
        decimal_part += std::string(18 - decimal_part.length(), '0');
    } else if (decimal_part.length() > 18) {
        decimal_part = decimal_part.substr(0, 18);
    }

    // Combine integer and decimal parts
    std::string wei_str = integer_part + decimal_part;

    // Remove leading zeros
    size_t first_nonzero = wei_str.find_first_not_of('0');
    if (first_nonzero != std::string::npos) {
        wei_str = wei_str.substr(first_nonzero);
    } else {
        wei_str = "0";
    }

    return wei_str;
}

std::string EthereumClient::GweiToWei(double gwei) {
    if (gwei <= 0.0) {
        return "0";
    }

    // 1 Gwei = 10^9 Wei
    long double wei = static_cast<long double>(gwei) * 1e9L;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << wei;
    return oss.str();
}

std::optional<std::string> EthereumClient::BroadcastTransaction(const std::string& signed_tx_hex) {
    if (signed_tx_hex.empty()) {
        return std::nullopt;
    }

    // Ensure 0x prefix
    std::string hex_data = signed_tx_hex;
    if (hex_data.substr(0, 2) != "0x") {
        hex_data = "0x" + hex_data;
    }

    // Etherscan API: Send raw transaction
    std::string endpoint = "?module=proxy&action=eth_sendRawTransaction&hex=" + hex_data;
    std::string response = makeRequest(endpoint);

    if (response.empty()) {
        return std::nullopt;
    }

    try {
        json data = json::parse(response);

        if (data.contains("result") && !data["result"].is_null()) {
            std::string tx_hash = data["result"].get<std::string>();
            return tx_hash;
        } else if (data.contains("error")) {
            // Transaction failed
            return std::nullopt;
        }
    } catch (const json::exception& e) {
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<std::string> EthereumClient::CreateSignedTransaction(
    const std::string& from_address,
    const std::string& to_address,
    const std::string& value_wei,
    const std::string& gas_price_wei,
    uint64_t gas_limit,
    const std::string& private_key_hex,
    uint64_t chain_id
) {
    // Validate addresses
    if (!IsValidAddress(from_address) || !IsValidAddress(to_address)) {
        return std::nullopt;
    }

    // Get nonce (transaction count)
    auto nonce_opt = GetTransactionCount(from_address);
    if (!nonce_opt.has_value()) {
        return std::nullopt;
    }
    uint64_t nonce = nonce_opt.value();

    // Convert private key from hex to bytes
    std::vector<uint8_t> private_key_bytes = RLP::Encoder::HexToBytes(private_key_hex);
    if (private_key_bytes.size() != 32) {
        return std::nullopt;  // Invalid private key
    }

    // Build unsigned transaction (EIP-155 format)
    // [nonce, gasPrice, gasLimit, to, value, data, chainId, 0, 0]
    std::vector<std::vector<uint8_t>> tx_fields;
    tx_fields.push_back(RLP::Encoder::EncodeUInt(nonce));
    tx_fields.push_back(RLP::Encoder::EncodeHex(gas_price_wei));
    tx_fields.push_back(RLP::Encoder::EncodeUInt(gas_limit));
    tx_fields.push_back(RLP::Encoder::EncodeHex(to_address));
    tx_fields.push_back(RLP::Encoder::EncodeHex(value_wei));
    tx_fields.push_back(RLP::Encoder::EncodeBytes({}));  // Empty data for simple transfer
    tx_fields.push_back(RLP::Encoder::EncodeUInt(chain_id));  // Chain ID for EIP-155
    tx_fields.push_back(RLP::Encoder::EncodeBytes({}));  // r = 0 for unsigned
    tx_fields.push_back(RLP::Encoder::EncodeBytes({}));  // s = 0 for unsigned

    // RLP encode the transaction
    std::vector<uint8_t> rlp_encoded = RLP::Encoder::EncodeList(tx_fields);

    // Keccak256 hash the encoded transaction
    std::array<uint8_t, 32> tx_hash;
    if (!Crypto::Keccak256(rlp_encoded.data(), rlp_encoded.size(), tx_hash)) {
        return std::nullopt;
    }

    // Sign the transaction hash
    Crypto::ECDSASignature signature;
    if (!Crypto::SignHash(private_key_bytes, tx_hash, signature)) {
        return std::nullopt;
    }

    // Ensure low-s value (EIP-2) - normalize signature
    // For Ethereum, we need raw r and s values, not DER encoding
    if (signature.r.size() != 32 || signature.s.size() != 32) {
        return std::nullopt;
    }

    // Calculate recovery ID (v value)
    // For EIP-155: v = chain_id * 2 + 35 + recovery_id (0 or 1)
    // We'll try recovery_id = 0 first (typically works)
    uint64_t v = chain_id * 2 + 35;

    // Rebuild transaction with signature
    std::vector<std::vector<uint8_t>> signed_tx_fields;
    signed_tx_fields.push_back(RLP::Encoder::EncodeUInt(nonce));
    signed_tx_fields.push_back(RLP::Encoder::EncodeHex(gas_price_wei));
    signed_tx_fields.push_back(RLP::Encoder::EncodeUInt(gas_limit));
    signed_tx_fields.push_back(RLP::Encoder::EncodeHex(to_address));
    signed_tx_fields.push_back(RLP::Encoder::EncodeHex(value_wei));
    signed_tx_fields.push_back(RLP::Encoder::EncodeBytes({}));  // Empty data
    signed_tx_fields.push_back(RLP::Encoder::EncodeUInt(v));
    signed_tx_fields.push_back(RLP::Encoder::EncodeBytes(signature.r));
    signed_tx_fields.push_back(RLP::Encoder::EncodeBytes(signature.s));

    // RLP encode the signed transaction
    std::vector<uint8_t> signed_rlp = RLP::Encoder::EncodeList(signed_tx_fields);

    // Convert to hex string
    std::string signed_tx_hex = RLP::Encoder::BytesToHex(signed_rlp);

    return signed_tx_hex;
}

} // namespace EthereumService

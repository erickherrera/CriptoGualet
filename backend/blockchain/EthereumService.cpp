#include "EthereumService.h"
#include "../core/include/Crypto.h"
#include "../utils/include/RLPEncoder.h"
#include <cpr/cpr.h>
#include <cmath>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

using json = nlohmann::json;

namespace EthereumService {

// ===== EtherscanProvider Implementation =====

EtherscanProvider::EtherscanProvider(const std::string& network, const std::string& apiToken)
    : m_network(network), m_apiToken(apiToken) {
    updateBaseUrl();
}

void EtherscanProvider::updateBaseUrl() {
    static const std::map<std::string, std::string> networkToUrl = {
        {"mainnet", "https://api.etherscan.io/api"},
        {"sepolia", "https://api-sepolia.etherscan.io/api"},
        {"goerli", "https://api-goerli.etherscan.io/api"}};

    auto it = networkToUrl.find(m_network);
    m_baseUrl = (it != networkToUrl.end()) ? it->second : "https://api.etherscan.io/api";
}

std::string EtherscanProvider::makeRequest(const std::string& endpoint) {
    std::string url = m_baseUrl + endpoint;
    if (!m_apiToken.empty()) {
        url += "&apikey=" + m_apiToken;
    }

    cpr::Header headers{{"User-Agent", "CriptoGualet/1.0"}};
    try {
        cpr::Response response = cpr::Get(cpr::Url{url}, cpr::Timeout{10000}, headers, cpr::VerifySsl{true});
        if (response.status_code == 200) return response.text;
    } catch (...) {}
    return "";
}

std::optional<AddressBalance> EtherscanProvider::GetAddressBalance(const std::string& address) {
    std::string endpoint = "?module=account&action=balance&address=" + address + "&tag=latest";
    std::string response = makeRequest(endpoint);
    if (response.empty()) return std::nullopt;

    try {
        json data = json::parse(response);
        if (data["status"] == "1" && data.contains("result")) {
            AddressBalance balance;
            balance.address = address;
            balance.balance_wei = data["result"].get<std::string>();
            balance.balance_eth = EthereumClient::WeiToEth(balance.balance_wei);
            return balance;
        }
    } catch (...) {}
    return std::nullopt;
}

std::optional<std::vector<Transaction>> EtherscanProvider::GetTransactionHistory(const std::string& address, uint32_t limit) {
    std::string endpoint = "?module=account&action=txlist&address=" + address +
                           "&startblock=0&endblock=99999999&page=1&offset=" + std::to_string(limit) + "&sort=desc";
    std::string response = makeRequest(endpoint);
    if (response.empty()) return std::nullopt;

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
                tx.value_eth = EthereumClient::WeiToEth(tx.value_wei);
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
    } catch (...) {}
    return std::nullopt;
}

std::optional<GasPrice> EtherscanProvider::GetGasPrice() {
    std::string endpoint = "?module=gastracker&action=gasoracle";
    std::string response = makeRequest(endpoint);
    if (response.empty()) return std::nullopt;

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
    } catch (...) {}
    return std::nullopt;
}

std::optional<uint64_t> EtherscanProvider::GetTransactionCount(const std::string& address) {
    std::string endpoint = "?module=proxy&action=eth_getTransactionCount&address=" + address + "&tag=latest";
    std::string response = makeRequest(endpoint);
    if (response.empty()) return std::nullopt;

    try {
        json data = json::parse(response);
        if (data.contains("result")) {
            std::string result = data["result"].get<std::string>();
            if (result.substr(0, 2) == "0x") result = result.substr(2);
            return std::stoull(result, nullptr, 16);
        }
    } catch (...) {}
    return std::nullopt;
}

std::optional<TokenInfo> EtherscanProvider::GetTokenInfo(const std::string& contractAddress) {
    std::string endpoint = "?module=token&action=tokeninfo&contractaddress=" + contractAddress;
    std::string response = makeRequest(endpoint);
    if (response.empty()) return std::nullopt;

    try {
        json data = json::parse(response);
        if (data["status"] == "1" && data.contains("result") && data["result"].is_array() && !data["result"].empty()) {
            auto token_data = data["result"][0];
            TokenInfo info;
            info.contract_address = token_data.value("contractAddress", "");
            info.name = token_data.value("tokenName", "");
            info.symbol = token_data.value("symbol", "");
            info.decimals = std::stoi(token_data.value("divisor", "0"));
            return info;
        }
    } catch (...) {}
    return std::nullopt;
}

std::optional<std::string> EtherscanProvider::GetTokenBalance(const std::string& contractAddress, const std::string& userAddress) {
    std::string endpoint = "?module=account&action=tokenbalance&contractaddress=" + contractAddress +
                           "&address=" + userAddress + "&tag=latest";
    std::string response = makeRequest(endpoint);
    if (response.empty()) return std::nullopt;

    try {
        json data = json::parse(response);
        if (data["status"] == "1" && data.contains("result")) return data["result"].get<std::string>();
    } catch (...) {}
    return std::nullopt;
}

std::optional<std::string> EtherscanProvider::BroadcastTransaction(const std::string& signed_tx_hex) {
    std::string hex_data = (signed_tx_hex.substr(0, 2) == "0x") ? signed_tx_hex : "0x" + signed_tx_hex;
    std::string endpoint = "?module=proxy&action=eth_sendRawTransaction&hex=" + hex_data;
    std::string response = makeRequest(endpoint);
    if (response.empty()) return std::nullopt;

    try {
        json data = json::parse(response);
        if (data.contains("result") && !data["result"].is_null()) return data["result"].get<std::string>();
    } catch (...) {}
    return std::nullopt;
}

std::pair<bool, std::string> EtherscanProvider::TestConnection() {
    auto gp = GetGasPrice();
    if (gp) return {true, "Successfully connected to Etherscan"};
    return {false, "Failed to connect to Etherscan"};
}

// ===== EthereumClient Implementation =====

EthereumClient::EthereumClient(const std::string& network) : m_network(network) {
    m_providers.push_back(std::make_unique<EtherscanProvider>(network));
}

void EthereumClient::SetApiToken(const std::string& token) {
    m_apiToken = token;
    SetNetwork(m_network); // Re-initialize with new token
}

void EthereumClient::SetNetwork(const std::string& network) {
    m_network = network;
    ClearProviders();
    m_providers.push_back(std::make_unique<EtherscanProvider>(network, m_apiToken));
}

void EthereumClient::AddProvider(std::unique_ptr<IEthereumProvider> provider) {
    m_providers.insert(m_providers.begin(), std::move(provider));
}

void EthereumClient::ClearProviders() {
    m_providers.clear();
}

std::optional<AddressBalance> EthereumClient::GetAddressBalance(const std::string& address) {
    for (auto& provider : m_providers) {
        auto result = provider->GetAddressBalance(address);
        if (result) return result;
    }
    return std::nullopt;
}

std::optional<std::vector<Transaction>> EthereumClient::GetTransactionHistory(const std::string& address, uint32_t limit) {
    for (auto& provider : m_providers) {
        auto result = provider->GetTransactionHistory(address, limit);
        if (result) return result;
    }
    return std::nullopt;
}

std::optional<GasPrice> EthereumClient::GetGasPrice() {
    for (auto& provider : m_providers) {
        auto result = provider->GetGasPrice();
        if (result) return result;
    }
    return std::nullopt;
}

std::optional<uint64_t> EthereumClient::GetTransactionCount(const std::string& address) {
    for (auto& provider : m_providers) {
        auto result = provider->GetTransactionCount(address);
        if (result) return result;
    }
    return std::nullopt;
}

std::optional<TokenInfo> EthereumClient::GetTokenInfo(const std::string& contractAddress) {
    for (auto& provider : m_providers) {
        auto result = provider->GetTokenInfo(contractAddress);
        if (result) return result;
    }
    return std::nullopt;
}

std::optional<std::string> EthereumClient::GetTokenBalance(const std::string& contractAddress, const std::string& userAddress) {
    for (auto& provider : m_providers) {
        auto result = provider->GetTokenBalance(contractAddress, userAddress);
        if (result) return result;
    }
    return std::nullopt;
}

std::optional<std::string> EthereumClient::BroadcastTransaction(const std::string& signed_tx_hex) {
    for (auto& provider : m_providers) {
        auto result = provider->BroadcastTransaction(signed_tx_hex);
        if (result) return result;
    }
    return std::nullopt;
}

std::optional<std::string> EthereumClient::CreateSignedTransaction(
    const std::string& from_address, const std::string& recipientAddress,
    const std::string& value_wei, const std::string& gas_price_wei, uint64_t gas_limit,
    const std::string& private_key_hex, uint64_t chain_id) {
    
    auto nonce_opt = GetTransactionCount(from_address);
    if (!nonce_opt) return std::nullopt;
    uint64_t nonce = *nonce_opt;

    std::vector<uint8_t> private_key_bytes = RLP::Encoder::HexToBytes(private_key_hex);
    if (private_key_bytes.size() != 32) return std::nullopt;

    std::vector<std::vector<uint8_t>> tx_fields;
    tx_fields.push_back(RLP::Encoder::EncodeUInt(nonce));
    tx_fields.push_back(RLP::Encoder::EncodeDecimal(gas_price_wei));
    tx_fields.push_back(RLP::Encoder::EncodeUInt(gas_limit));
    tx_fields.push_back(RLP::Encoder::EncodeHex(recipientAddress));
    tx_fields.push_back(RLP::Encoder::EncodeDecimal(value_wei));
    tx_fields.push_back(RLP::Encoder::EncodeBytes({}));
    tx_fields.push_back(RLP::Encoder::EncodeUInt(chain_id));
    tx_fields.push_back(RLP::Encoder::EncodeBytes({}));
    tx_fields.push_back(RLP::Encoder::EncodeBytes({}));

    std::vector<uint8_t> rlp_encoded = RLP::Encoder::EncodeList(tx_fields);
    std::array<uint8_t, 32> tx_hash;
    if (!Crypto::Keccak256(rlp_encoded.data(), rlp_encoded.size(), tx_hash)) return std::nullopt;

    Crypto::RecoverableSignature signature;
    if (!Crypto::SignHashRecoverable(private_key_bytes, tx_hash, signature)) return std::nullopt;

    uint64_t v = chain_id * 2 + 35 + signature.recovery_id;
    std::vector<std::vector<uint8_t>> signed_fields;
    signed_fields.push_back(RLP::Encoder::EncodeUInt(nonce));
    signed_fields.push_back(RLP::Encoder::EncodeDecimal(gas_price_wei));
    signed_fields.push_back(RLP::Encoder::EncodeUInt(gas_limit));
    signed_fields.push_back(RLP::Encoder::EncodeHex(recipientAddress));
    signed_fields.push_back(RLP::Encoder::EncodeDecimal(value_wei));
    signed_fields.push_back(RLP::Encoder::EncodeBytes({}));
    signed_fields.push_back(RLP::Encoder::EncodeUInt(v));
    signed_fields.push_back(RLP::Encoder::EncodeBytes(signature.r));
    signed_fields.push_back(RLP::Encoder::EncodeBytes(signature.s));

    std::vector<uint8_t> signed_rlp = RLP::Encoder::EncodeList(signed_fields);
    return RLP::Encoder::BytesToHex(signed_rlp);
}

std::optional<std::string> EthereumClient::CreateEIP1559Transaction(
    const std::string& from_address, const std::string& recipientAddress,
    const std::string& value_wei, const std::string& max_fee_per_gas_wei,
    const std::string& max_priority_fee_per_gas_wei, uint64_t gas_limit,
    const std::string& private_key_hex, uint64_t chain_id) {
    
    auto nonce_opt = GetTransactionCount(from_address);
    if (!nonce_opt) return std::nullopt;
    uint64_t nonce = *nonce_opt;

    std::vector<uint8_t> private_key_bytes = RLP::Encoder::HexToBytes(private_key_hex);
    if (private_key_bytes.size() != 32) return std::nullopt;

    std::vector<std::vector<uint8_t>> tx_fields;
    tx_fields.push_back(RLP::Encoder::EncodeUInt(chain_id));
    tx_fields.push_back(RLP::Encoder::EncodeUInt(nonce));
    tx_fields.push_back(RLP::Encoder::EncodeDecimal(max_priority_fee_per_gas_wei));
    tx_fields.push_back(RLP::Encoder::EncodeDecimal(max_fee_per_gas_wei));
    tx_fields.push_back(RLP::Encoder::EncodeUInt(gas_limit));
    tx_fields.push_back(RLP::Encoder::EncodeHex(recipientAddress));
    tx_fields.push_back(RLP::Encoder::EncodeDecimal(value_wei));
    tx_fields.push_back(RLP::Encoder::EncodeBytes({}));
    tx_fields.push_back(RLP::Encoder::EncodeList({}));

    std::vector<uint8_t> rlp_fields = RLP::Encoder::EncodeList(tx_fields);
    std::vector<uint8_t> hash_input;
    hash_input.push_back(0x02);
    hash_input.insert(hash_input.end(), rlp_fields.begin(), rlp_fields.end());

    std::array<uint8_t, 32> tx_hash;
    if (!Crypto::Keccak256(hash_input.data(), hash_input.size(), tx_hash)) return std::nullopt;

    Crypto::RecoverableSignature signature;
    if (!Crypto::SignHashRecoverable(private_key_bytes, tx_hash, signature)) return std::nullopt;

    std::vector<std::vector<uint8_t>> signed_fields = tx_fields;
    signed_fields.push_back(RLP::Encoder::EncodeUInt(signature.recovery_id));
    signed_fields.push_back(RLP::Encoder::EncodeBytes(signature.r));
    signed_fields.push_back(RLP::Encoder::EncodeBytes(signature.s));

    std::vector<uint8_t> signed_rlp_fields = RLP::Encoder::EncodeList(signed_fields);
    std::vector<uint8_t> final_payload;
    final_payload.push_back(0x02);
    final_payload.insert(final_payload.end(), signed_rlp_fields.begin(), signed_rlp_fields.end());

    return RLP::Encoder::BytesToHex(final_payload);
}

bool EthereumClient::IsValidAddress(const std::string& address) {
    std::regex eth_address_pattern("^0x[0-9a-fA-F]{40}$");
    return std::regex_match(address, eth_address_pattern);
}

double EthereumClient::WeiToEth(const std::string& wei_str) {
    if (wei_str.empty() || wei_str == "0") return 0.0;
    try {
        if (wei_str.length() > 18) {
            std::string integer_part = wei_str.substr(0, wei_str.length() - 18);
            std::string decimal_part = wei_str.substr(wei_str.length() - 18);
            return std::stod(integer_part + "." + decimal_part);
        } else {
            std::string decimal_str = std::string(18 - wei_str.length(), '0') + wei_str;
            return std::stod("0." + decimal_str);
        }
    } catch (...) { return 0.0; }
}

std::string EthereumClient::EthToWei(double eth) {
    if (eth <= 0.0) return "0";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(18) << eth;
    std::string s = oss.str();
    size_t dot = s.find('.');
    std::string integer = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::string decimal = (dot == std::string::npos) ? "" : s.substr(dot + 1);
    if (decimal.length() < 18) decimal += std::string(18 - decimal.length(), '0');
    else decimal = decimal.substr(0, 18);
    std::string res = integer + decimal;
    size_t first = res.find_first_not_of('0');
    return (first == std::string::npos) ? "0" : res.substr(first);
}

std::string EthereumClient::GweiToWei(double gwei) {
    if (gwei <= 0.0) return "0";
    long double wei = static_cast<long double>(gwei) * 1e9L;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0) << wei;
    return oss.str();
}

} // namespace EthereumService

#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

namespace EthereumService {

/**
 * @brief Ethereum address balance information
 */
struct AddressBalance {
    std::string address;
    std::string balance_wei;      // Balance in Wei (smallest unit)
    double balance_eth;            // Balance in ETH
    uint64_t transaction_count;    // Number of transactions

    AddressBalance() : balance_eth(0.0), transaction_count(0) {}
};

/**
 * @brief Ethereum transaction information
 */
struct Transaction {
    std::string hash;
    std::string from;
    std::string to;
    std::string value_wei;
    double value_eth;
    std::string gas_price_wei;
    std::string gas_used;
    std::string block_number;
    std::string timestamp;
    std::string status;  // "1" = success, "0" = failed
    bool is_error;

    Transaction() : value_eth(0.0), is_error(false) {}
};

/**
 * @brief Gas price information
 */
struct GasPrice {
    std::string safe_gas_price;      // Safe (slower) gas price in Gwei
    std::string propose_gas_price;   // Proposed (average) gas price in Gwei
    std::string fast_gas_price;      // Fast gas price in Gwei

    GasPrice() {}
};

/**
 * @brief ERC20 token information
 */
struct TokenInfo {
    std::string contract_address;
    std::string name;
    std::string symbol;
    int decimals;

    TokenInfo() : decimals(0) {}
};

/**
 * @brief Ethereum blockchain service client
 *
 * Provides Ethereum blockchain interaction using Etherscan API
 * Free tier: 5 calls/second, up to 100,000 calls/day
 */
class EthereumClient {
public:
    /**
     * @brief Constructor
     * @param network Network to connect to ("mainnet", "sepolia", "goerli")
     */
    explicit EthereumClient(const std::string& network = "mainnet");

    /**
     * @brief Set API token for authenticated requests
     * @param token Etherscan API token
     */
    void SetApiToken(const std::string& token);

    /**
     * @brief Set network
     * @param network Network name ("mainnet", "sepolia", "goerli")
     */
    void SetNetwork(const std::string& network);

    /**
     * @brief Get balance for an Ethereum address
     * @param address Ethereum address (0x...)
     * @return Optional AddressBalance if successful
     */
    std::optional<AddressBalance> GetAddressBalance(const std::string& address);

    /**
     * @brief Get transaction history for an address
     * @param address Ethereum address
     * @param limit Maximum number of transactions to retrieve
     * @return Optional vector of transactions
     */
    std::optional<std::vector<Transaction>> GetTransactionHistory(
        const std::string& address,
        uint32_t limit = 10
    );

    /**
     * @brief Get current gas price estimates
     * @return Optional GasPrice information
     */
    std::optional<GasPrice> GetGasPrice();

    /**
     * @brief Get transaction count (nonce) for an address
     * @param address Ethereum address
     * @return Optional transaction count
     */
    std::optional<uint64_t> GetTransactionCount(const std::string& address);

    /**
     * @brief Get ERC20 token information
     * @param contractAddress The address of the token contract
     * @return Optional TokenInfo if successful
     */
    std::optional<TokenInfo> GetTokenInfo(const std::string& contractAddress);

    /**
     * @brief Get ERC20 token balance for an address
     * @param contractAddress The address of the token contract
     * @param userAddress The address of the user
     * @return Optional balance as a string (in smallest unit)
     */
    std::optional<std::string> GetTokenBalance(const std::string& contractAddress, const std::string& userAddress);

    /**
     * @brief Send raw signed transaction to the network
     * @param signed_tx_hex Signed transaction in hex format (0x...)
     * @return Optional transaction hash if successful
     */
    std::optional<std::string> BroadcastTransaction(const std::string& signed_tx_hex);

    /**
     * @brief Create and sign an Ethereum transaction
     * @param from_address Sender address
     * @param to_address Recipient address
     * @param value_wei Amount in Wei
     * @param gas_price_wei Gas price in Wei
     * @param gas_limit Gas limit (21000 for simple transfer)
     * @param private_key_hex Private key in hex format
     * @param chain_id Chain ID (1=mainnet, 11155111=sepolia)
     * @return Optional signed transaction hex if successful
     */
    std::optional<std::string> CreateSignedTransaction(
        const std::string& from_address,
        const std::string& to_address,
        const std::string& value_wei,
        const std::string& gas_price_wei,
        uint64_t gas_limit,
        const std::string& private_key_hex,
        uint64_t chain_id
    );

    /**
     * @brief Validate Ethereum address format
     * @param address Address to validate
     * @return true if valid Ethereum address format
     */
    bool IsValidAddress(const std::string& address);

    /**
     * @brief Convert Wei to ETH
     * @param wei_str Wei amount as string
     * @return ETH amount as double
     */
    static double WeiToEth(const std::string& wei_str);

    /**
     * @brief Convert ETH to Wei
     * @param eth ETH amount
     * @return Wei amount as string
     */
    static std::string EthToWei(double eth);

    /**
     * @brief Convert Gwei to Wei
     * @param gwei Gwei amount
     * @return Wei amount as string
     */
    static std::string GweiToWei(double gwei);

private:
    std::string m_network;
    std::string m_apiToken;
    std::string m_baseUrl;

    /**
     * @brief Update base URL based on network
     */
    void updateBaseUrl();

    /**
     * @brief Make HTTP GET request to Etherscan API
     * @param endpoint API endpoint with parameters
     * @return Response body as string
     */
    std::string makeRequest(const std::string& endpoint);
};

} // namespace EthereumService

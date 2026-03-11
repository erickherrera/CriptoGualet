#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace EthereumService {

/**
 * @brief Ethereum address balance information
 */
struct AddressBalance {
    std::string address;
    std::string balance_wei;     // Balance in Wei (smallest unit)
    double balance_eth;          // Balance in ETH
    uint64_t transaction_count;  // Number of transactions

    AddressBalance() : balance_eth(0.0), transaction_count(0) {
    }
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

    Transaction() : value_eth(0.0), is_error(false) {
    }
};

/**
 * @brief Gas price information
 */
struct GasPrice {
    std::string safe_gas_price;     // Safe (slower) gas price in Gwei
    std::string propose_gas_price;  // Proposed (average) gas price in Gwei
    std::string fast_gas_price;     // Fast gas price in Gwei

    GasPrice() {
    }
};

/**
 * @brief ERC20 token information
 */
struct TokenInfo {
    std::string contract_address;
    std::string name;
    std::string symbol;
    int decimals;

    TokenInfo() : decimals(0) {
    }
};

/**
 * @brief Ethereum blockchain service client
 *
 * Provides Ethereum blockchain interaction using Etherscan API
 * Free tier: 5 calls/second, up to 100,000 calls/day
 */
/**
 * @brief Interface for Ethereum blockchain providers
 */
class IEthereumProvider {
  public:
    virtual ~IEthereumProvider() = default;
    virtual std::optional<AddressBalance> GetAddressBalance(const std::string& address) = 0;
    virtual std::optional<std::vector<Transaction>> GetTransactionHistory(const std::string& address,
                                                                          uint32_t limit) = 0;
    virtual std::optional<GasPrice> GetGasPrice() = 0;
    virtual std::optional<uint64_t> GetTransactionCount(const std::string& address) = 0;
    virtual std::optional<TokenInfo> GetTokenInfo(const std::string& contractAddress) = 0;
    virtual std::optional<std::string> GetTokenBalance(const std::string& contractAddress,
                                                       const std::string& userAddress) = 0;
    virtual std::optional<std::string> BroadcastTransaction(const std::string& signed_tx_hex) = 0;
    virtual std::pair<bool, std::string> TestConnection() = 0;
    virtual std::string Name() const = 0;
};

/**
 * @brief Ethereum blockchain service client
 *
 * Coordinates multiple providers and handles fallbacks
 */
class EthereumClient {
  public:
    explicit EthereumClient(const std::string& network = "mainnet");

    void SetApiToken(const std::string& token);
    void SetNetwork(const std::string& network);

    // Provider management
    void AddProvider(std::unique_ptr<IEthereumProvider> provider);
    void ClearProviders();

    // Standard blockchain methods (with fallback logic)
    std::optional<AddressBalance> GetAddressBalance(const std::string& address);
    std::optional<std::vector<Transaction>> GetTransactionHistory(const std::string& address,
                                                                  uint32_t limit = 10);
    std::optional<GasPrice> GetGasPrice();
    std::optional<uint64_t> GetTransactionCount(const std::string& address);
    std::optional<TokenInfo> GetTokenInfo(const std::string& contractAddress);
    std::optional<std::string> GetTokenBalance(const std::string& contractAddress,
                                               const std::string& userAddress);
    std::optional<std::string> BroadcastTransaction(const std::string& signed_tx_hex);

    // Transaction creation
    std::optional<std::string> CreateSignedTransaction(
        const std::string& from_address, const std::string& to_address,
        const std::string& value_wei, const std::string& gas_price_wei, uint64_t gas_limit,
        const std::string& private_key_hex, uint64_t chain_id);

    std::optional<std::string> CreateEIP1559Transaction(
        const std::string& from_address, const std::string& to_address,
        const std::string& value_wei, const std::string& max_fee_per_gas_wei,
        const std::string& max_priority_fee_per_gas_wei, uint64_t gas_limit,
        const std::string& private_key_hex, uint64_t chain_id);

    bool IsValidAddress(const std::string& address);

    // Static utilities
    static double WeiToEth(const std::string& wei_str);
    static std::string EthToWei(double eth);
    static std::string GweiToWei(double gwei);

  private:
    std::string m_network;
    std::string m_apiToken;
    std::vector<std::unique_ptr<IEthereumProvider>> m_providers;

    // Helper to try an operation across providers until one succeeds
    template <typename Func>
    auto tryProviderOp(Func func) -> decltype(func(m_providers[0].get()));
};

/**
 * @brief Etherscan implementation of IEthereumProvider
 */
class EtherscanProvider : public IEthereumProvider {
  public:
    EtherscanProvider(const std::string& network, const std::string& apiToken = "");

    std::optional<AddressBalance> GetAddressBalance(const std::string& address) override;
    std::optional<std::vector<Transaction>> GetTransactionHistory(const std::string& address,
                                                                  uint32_t limit) override;
    std::optional<GasPrice> GetGasPrice() override;
    std::optional<uint64_t> GetTransactionCount(const std::string& address) override;
    std::optional<TokenInfo> GetTokenInfo(const std::string& contractAddress) override;
    std::optional<std::string> GetTokenBalance(const std::string& contractAddress,
                                               const std::string& userAddress) override;
    std::optional<std::string> BroadcastTransaction(const std::string& signed_tx_hex) override;
    std::pair<bool, std::string> TestConnection() override;
    std::string Name() const override {
        return "Etherscan";
    }

  private:
    std::string m_network;
    std::string m_apiToken;
    std::string m_baseUrl;
    void updateBaseUrl();
    std::string makeRequest(const std::string& endpoint);
};

}  // namespace EthereumService

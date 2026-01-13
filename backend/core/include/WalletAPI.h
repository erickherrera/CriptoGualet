#pragma once

#include "../../blockchain/include/BlockCypher.h"
#include "../../blockchain/include/EthereumService.h"
#include "../../repository/include/Repository/TokenRepository.h"
#include "Crypto.h"
#include <functional>
#include <memory>
#include <map>

namespace WalletAPI {

struct SendTransactionResult {
  bool success;
  std::string transaction_hash;
  std::string error_message;
  uint64_t total_fees;
};

struct ReceiveInfo {
  std::string address;
  uint64_t confirmed_balance;
  uint64_t unconfirmed_balance;
  uint32_t transaction_count;
  std::vector<std::string> recent_transactions;
};

class SimpleWallet {
private:
  std::unique_ptr<BlockCypher::BlockCypherClient> client;
  std::string current_network;

public:
  explicit SimpleWallet(const std::string &network = "btc/test3");
  ~SimpleWallet() = default;

  // Configuration
  void SetApiToken(const std::string &token);
  void SetNetwork(const std::string &network);

  // Receive functionality
  ReceiveInfo GetAddressInfo(const std::string &address);
  uint64_t GetBalance(const std::string &address);
  std::vector<std::string> GetTransactionHistory(const std::string &address,
                                                 uint32_t limit = 10);

  // Send functionality
  SendTransactionResult
  SendFunds(const std::vector<std::string> &from_addresses,
            const std::string &to_address, uint64_t amount_satoshis,
            const std::map<std::string, std::vector<uint8_t>> &private_keys,
            uint64_t fee_satoshis = 0);

  // Utility functions
  bool ValidateAddress(const std::string &address);
  uint64_t EstimateTransactionFee();
  uint64_t ConvertBTCToSatoshis(double btc_amount);
  double ConvertSatoshisToBTC(uint64_t satoshis);

  // For testing/demo purposes
  std::string GetNetworkInfo();
};

// Ethereum Send Transaction Result
struct EthereumSendResult {
  bool success;
  std::string transaction_hash;
  std::string error_message;
  std::string total_cost_wei;  // Gas cost + value
  double total_cost_eth;
};

// Result of importing an ERC20 token
struct ImportTokenResult {
    bool success;
    std::string error_message;
    std::optional<EthereumService::TokenInfo> token_info;
};

// Ethereum Wallet class
class EthereumWallet {
private:
  std::unique_ptr<EthereumService::EthereumClient> client;
  std::string current_network;

public:
  explicit EthereumWallet(const std::string &network = "mainnet");
  ~EthereumWallet() = default;

  // Configuration
  void SetApiToken(const std::string &token);
  void SetNetwork(const std::string &network);

  // Receive functionality
  std::optional<EthereumService::AddressBalance> GetAddressInfo(const std::string &address);
  double GetBalance(const std::string &address); // Returns ETH balance
  std::vector<EthereumService::Transaction> GetTransactionHistory(const std::string &address, uint32_t limit = 10);

  // Send functionality
  EthereumSendResult SendFunds(
    const std::string &from_address,
    const std::string &to_address,
    double amount_eth,
    const std::string &private_key_hex,
    const std::string &gas_price_gwei = "",  // Empty = auto-estimate
    uint64_t gas_limit = 21000  // 21000 for simple ETH transfer
  );

  // Gas price estimation
  std::optional<EthereumService::GasPrice> GetGasPrice();

  // Token functionality
  ImportTokenResult importERC20Token(int walletId, const std::string& contractAddress, Repository::TokenRepository& tokenRepo);

  // Utility functions
  bool ValidateAddress(const std::string &address);
  double ConvertWeiToEth(const std::string &wei_str);
  std::string ConvertEthToWei(double eth);

  // For testing/demo purposes
  std::string GetNetworkInfo();
};

// Litecoin Send Transaction Result (similar to Bitcoin)
struct LitecoinSendResult {
  bool success;
  std::string transaction_hash;
  std::string error_message;
  uint64_t total_fees;  // In litoshis (1 LTC = 100,000,000 litoshis)
};

// Litecoin Receive Info (similar to Bitcoin)
struct LitecoinReceiveInfo {
  std::string address;
  uint64_t confirmed_balance;    // In litoshis
  uint64_t unconfirmed_balance;  // In litoshis
  uint32_t transaction_count;
  std::vector<std::string> recent_transactions;
};

// Litecoin Wallet class (uses BlockCypher API which supports Litecoin)
class LitecoinWallet {
private:
  std::unique_ptr<BlockCypher::BlockCypherClient> client;
  std::string current_network;

public:
  explicit LitecoinWallet(const std::string &network = "ltc/main");
  ~LitecoinWallet() = default;

  // Configuration
  void SetApiToken(const std::string &token);
  void SetNetwork(const std::string &network);

  // Receive functionality
  LitecoinReceiveInfo GetAddressInfo(const std::string &address);
  uint64_t GetBalance(const std::string &address);  // Returns litoshis
  std::vector<std::string> GetTransactionHistory(const std::string &address,
                                                 uint32_t limit = 10);

  // Send functionality
  LitecoinSendResult
  SendFunds(const std::vector<std::string> &from_addresses,
            const std::string &to_address, uint64_t amount_litoshis,
            const std::map<std::string, std::vector<uint8_t>> &private_keys,
            uint64_t fee_litoshis = 0);

  // Utility functions
  bool ValidateAddress(const std::string &address);
  uint64_t EstimateTransactionFee();
  uint64_t ConvertLTCToLitoshis(double ltc_amount);
  double ConvertLitoshisToLTC(uint64_t litoshis);

  // For testing/demo purposes
  std::string GetNetworkInfo();
};

} // namespace WalletAPI

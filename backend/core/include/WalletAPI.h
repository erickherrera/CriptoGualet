#pragma once

#include "../../blockchain/include/BlockCypher.h"
#include <functional>
#include <memory>

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
            uint64_t fee_satoshis = 0);

  // Utility functions
  bool ValidateAddress(const std::string &address);
  uint64_t EstimateTransactionFee();
  uint64_t ConvertBTCToSatoshis(double btc_amount);
  double ConvertSatoshisToBTC(uint64_t satoshis);

  // For testing/demo purposes
  std::string GetNetworkInfo();
};

} // namespace WalletAPI

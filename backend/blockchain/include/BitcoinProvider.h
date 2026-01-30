#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace BitcoinProviders {

enum class ProviderType {
  BlockCypher,
  BitcoinRpc
};

struct ProviderConfig {
  ProviderType type = ProviderType::BlockCypher;
  std::string network = "btc/test3";
  std::string apiToken;
  std::string rpcUrl;
  std::string rpcUsername;
  std::string rpcPassword;
  bool allowInsecureHttp = true;
  bool enableFallback = true;
};

struct AddressInfo {
  std::string address;
  uint64_t confirmedBalance = 0;
  uint64_t unconfirmedBalance = 0;
  uint32_t transactionCount = 0;
  std::vector<std::string> recentTransactions;
};

class BitcoinProvider {
public:
  virtual ~BitcoinProvider() = default;
  virtual std::optional<AddressInfo> getAddressInfo(const std::string &address,
                                                    uint32_t limit) = 0;
  virtual std::optional<uint64_t> getBalance(const std::string &address) = 0;
  virtual std::optional<uint64_t> estimateFeeRate() = 0;
  virtual std::pair<bool, std::string> testConnection() = 0;
  virtual std::string name() const = 0;
};

std::unique_ptr<BitcoinProvider> CreateProvider(const ProviderConfig &config);

} // namespace BitcoinProviders

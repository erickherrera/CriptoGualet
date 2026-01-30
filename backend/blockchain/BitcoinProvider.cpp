#include "include/BitcoinProvider.h"
#include "include/BlockCypher.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <sstream>

using json = nlohmann::json;

namespace BitcoinProviders {
namespace {
constexpr double kSatoshisPerBtc = 100000000.0;

uint64_t btcToSatoshis(double btc) {
  return static_cast<uint64_t>(std::llround(btc * kSatoshisPerBtc));
}

double parseBtcValue(const json &value) {
  if (value.is_number_float()) {
    return value.get<double>();
  }
  if (value.is_number_integer()) {
    return static_cast<double>(value.get<int64_t>());
  }
  if (value.is_string()) {
    return std::stod(value.get<std::string>());
  }
  return 0.0;
}

class BlockCypherProvider : public BitcoinProvider {
public:
  BlockCypherProvider(const std::string &network, const std::string &apiToken)
      : m_client(std::make_unique<BlockCypher::BlockCypherClient>(network)) {
    if (!apiToken.empty()) {
      m_client->SetApiToken(apiToken);
    }
  }

  std::optional<AddressInfo> getAddressInfo(const std::string &address,
                                            uint32_t limit) override {
    if (!m_client) {
      return std::nullopt;
    }

    AddressInfo info;
    info.address = address;

    auto balance = m_client->GetAddressBalance(address);
    if (balance) {
      info.confirmedBalance = balance->balance;
      info.unconfirmedBalance = balance->unconfirmed_balance;
      info.transactionCount = balance->n_tx;
    }

    auto txs = m_client->GetAddressTransactions(address, limit);
    if (txs) {
      info.recentTransactions = txs.value();
      if (info.transactionCount == 0) {
        info.transactionCount =
            static_cast<uint32_t>(info.recentTransactions.size());
      }
    }

    if (!balance && !txs) {
      return std::nullopt;
    }

    return info;
  }

  std::optional<uint64_t> getBalance(const std::string &address) override {
    if (!m_client) {
      return std::nullopt;
    }

    auto balance = m_client->GetAddressBalance(address);
    if (!balance) {
      return std::nullopt;
    }

    return balance->balance;
  }

  std::optional<uint64_t> estimateFeeRate() override {
    if (!m_client) {
      return std::nullopt;
    }

    return m_client->EstimateFees();
  }

  std::pair<bool, std::string> testConnection() override {
    if (!m_client) {
      return {false, "Client not initialized."};
    }
    auto fees = m_client->EstimateFees();
    if (fees) {
      return {true, "Successfully connected to BlockCypher."};
    }
    return {false, "Failed to connect to BlockCypher. Check internet connection."};
  }

  std::string name() const override { return "BlockCypher"; }

private:
  std::unique_ptr<BlockCypher::BlockCypherClient> m_client;
};

class BitcoinRpcProvider : public BitcoinProvider {
public:
  explicit BitcoinRpcProvider(const ProviderConfig &config)
      : m_rpcUrl(config.rpcUrl), m_rpcUsername(config.rpcUsername),
        m_rpcPassword(config.rpcPassword) {}

  std::optional<AddressInfo> getAddressInfo(const std::string &address,
                                            uint32_t limit) override {
    auto confirmedResult = call("getreceivedbyaddress", {address, 1});
    auto totalResult = call("getreceivedbyaddress", {address, 0});

    if (!confirmedResult && !totalResult) {
      return std::nullopt;
    }

    AddressInfo info;
    info.address = address;

    if (confirmedResult) {
      info.confirmedBalance =
          btcToSatoshis(parseBtcValue(*confirmedResult));
    }

    if (totalResult) {
      uint64_t totalBalance =
          btcToSatoshis(parseBtcValue(*totalResult));
      if (totalBalance >= info.confirmedBalance) {
        info.unconfirmedBalance = totalBalance - info.confirmedBalance;
      }
    }

    auto txsResult = call("listtransactions",
                          {"*", static_cast<int>(limit), 0, true});
    if (txsResult && txsResult->is_array()) {
      for (const auto &tx : *txsResult) {
        if (!tx.is_object()) {
          continue;
        }
        if (tx.contains("address") && tx["address"].is_string() &&
            tx["address"].get<std::string>() == address) {
          if (tx.contains("txid") && tx["txid"].is_string()) {
            info.recentTransactions.push_back(tx["txid"].get<std::string>());
          }
        }
      }
    }

    info.transactionCount =
        static_cast<uint32_t>(info.recentTransactions.size());

    return info;
  }

  std::optional<uint64_t> getBalance(const std::string &address) override {
    auto confirmedResult = call("getreceivedbyaddress", {address, 1});
    if (!confirmedResult) {
      return std::nullopt;
    }

    return btcToSatoshis(parseBtcValue(*confirmedResult));
  }

  std::optional<uint64_t> estimateFeeRate() override {
    auto result = call("estimatesmartfee", {6});
    if (!result || !result->is_object() || !result->contains("feerate")) {
      return std::nullopt;
    }

    double feeRateBtc = parseBtcValue((*result)["feerate"]);
    return btcToSatoshis(feeRateBtc);
  }

  std::pair<bool, std::string> testConnection() override {
    auto result = call("getblockchaininfo", {});
    if (result) {
      return {true, "Successfully connected to Bitcoin RPC."};
    }
    return {false, "Failed to connect to Bitcoin RPC node. Check URL and credentials."};
  }

  std::string name() const override { return "Bitcoin RPC"; }

private:
  std::optional<json> call(const std::string &method, json params) {
    if (m_rpcUrl.empty()) {
      return std::nullopt;
    }

    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "criptogualet"},
        {"method", method},
        {"params", params}};

    cpr::Header headers{{"Content-Type", "application/json"}};
    cpr::Response response;

    if (!m_rpcUsername.empty()) {
      response = cpr::Post(cpr::Url{m_rpcUrl}, cpr::Body{payload.dump()},
                           headers,
                           cpr::Authentication{m_rpcUsername, m_rpcPassword,
                                               cpr::AuthMode::BASIC},
                           cpr::Timeout{10000});
    } else {
      response = cpr::Post(cpr::Url{m_rpcUrl}, cpr::Body{payload.dump()},
                           headers, cpr::Timeout{10000});
    }

    if (response.status_code < 200 || response.status_code >= 300 ||
        response.text.empty()) {
      return std::nullopt;
    }

    json parsed;
    try {
      parsed = json::parse(response.text);
    } catch (const std::exception &) {
      return std::nullopt;
    }

    if (parsed.contains("error") && !parsed["error"].is_null()) {
      return std::nullopt;
    }

    if (!parsed.contains("result")) {
      return std::nullopt;
    }

    return parsed["result"];
  }

  std::string m_rpcUrl;
  std::string m_rpcUsername;
  std::string m_rpcPassword;
};

} // namespace

std::unique_ptr<BitcoinProvider> CreateProvider(const ProviderConfig &config) {
  switch (config.type) {
  case ProviderType::BitcoinRpc:
    if (!config.allowInsecureHttp &&
        config.rpcUrl.rfind("http://", 0) == 0) {
      return nullptr;
    }
    return std::make_unique<BitcoinRpcProvider>(config);
  case ProviderType::BlockCypher:
  default:
    return std::make_unique<BlockCypherProvider>(config.network, config.apiToken);
  }
}

} // namespace BitcoinProviders

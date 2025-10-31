#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace BlockCypher {

struct AddressBalance {
    std::string address;
    uint64_t balance;
    uint64_t unconfirmed_balance;
    uint32_t n_tx;
    std::string final_balance_str;
    std::string unconfirmed_balance_str;
};

struct TransactionInput {
    std::string prev_hash;
    uint32_t output_index;
    std::string script;
    uint64_t output_value;
    uint32_t sequence;
    std::vector<std::string> addresses;
    std::string script_type;
    uint32_t age;
};

struct TransactionOutput {
    uint64_t value;
    std::string script;
    std::vector<std::string> addresses;
    std::string script_type;
    uint32_t spent_by_index;
};

struct Transaction {
    std::string hash;
    uint64_t total;
    uint64_t fees;
    uint32_t size;
    uint32_t vsize;
    std::string preference;
    std::string relayed_by;
    std::string received;
    uint32_t ver;
    uint32_t lock_time;
    bool double_spend;
    uint32_t vin_sz;
    uint32_t vout_sz;
    uint32_t confirmations;
    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;
};

struct CreateTransactionRequest {
    std::vector<std::string> input_addresses;
    std::vector<std::pair<std::string, uint64_t>> outputs; // address, value pairs
    uint64_t fees = 0; // Optional, will be calculated if 0
};

struct CreateTransactionResponse {
    Transaction tx;
    std::vector<std::string> tosign;
    std::vector<std::string> signatures;
    std::vector<std::string> pubkeys;
    std::string errors;
};

class BlockCypherClient {
private:
    std::string base_url;
    std::string api_token;
    std::string network; // "btc/main", "btc/test3", etc.

public:
    explicit BlockCypherClient(const std::string& network = "btc/main", const std::string& token = "");

    // Address operations
    std::optional<AddressBalance> GetAddressBalance(const std::string& address);
    std::optional<std::vector<std::string>> GetAddressTransactions(const std::string& address, uint32_t limit = 50);

    // Transaction operations
    std::optional<Transaction> GetTransaction(const std::string& tx_hash);
    std::optional<CreateTransactionResponse> CreateTransaction(const CreateTransactionRequest& request);
    std::optional<std::string> SendSignedTransaction(const CreateTransactionResponse& signed_tx);
    std::optional<std::string> SendRawTransaction(const std::string& hex);

    // Utility
    bool IsValidAddress(const std::string& address);
    std::optional<uint64_t> EstimateFees();

    // Configuration
    void SetApiToken(const std::string& token);
    void SetNetwork(const std::string& network);

private:
    std::string MakeRequest(const std::string& endpoint, const std::string& method = "GET", const std::string& payload = "");
    std::string BuildUrl(const std::string& endpoint);
};

} // namespace BlockCypher
#include "BlockCypher.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <regex>

#include "Repository/Logger.h"

using json = nlohmann::json;

namespace BlockCypher {

BlockCypherClient::BlockCypherClient(const std::string& networkIdentifier,
                                     const std::string& apiToken)
    : base_url("https://api.blockcypher.com/v1/"), network(networkIdentifier), api_token(apiToken) {
}

std::string BlockCypherClient::BuildUrl(const std::string& endpoint) {
    std::string url = base_url + network + "/" + endpoint;
    if (!api_token.empty()) {
        char separator = (endpoint.find('?') != std::string::npos) ? '&' : '?';
        url += separator + "token=" + api_token;
    }
    return url;
}

std::string BlockCypherClient::MakeRequest(const std::string& endpointPath,
                                           const std::string& httpMethod,
                                           const std::string& requestPayload) {
    try {
        std::string url = BuildUrl(endpointPath);

        cpr::Response response;
        bool isGetRequest = (httpMethod == "GET");
        bool isPostRequest = (httpMethod == "POST");

        if (!isGetRequest && !isPostRequest) {
            return "";
        }

        if (isGetRequest) {
            response = cpr::Get(cpr::Url{url});
        } else {
            response = cpr::Post(cpr::Url{url}, cpr::Body{requestPayload},
                                 cpr::Header{{"Content-Type", "application/json"}});
        }

        if (response.status_code >= 200 && response.status_code < 300) {
            return response.text;
        } else {
            REPO_LOG_ERROR("BlockCypher", "HTTP Error " + std::to_string(response.status_code) +
                                              ": " + response.text);
            return "";
        }
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher", "Request error: " + std::string(e.what()));
        return "";
    }
}

std::optional<AddressBalance> BlockCypherClient::GetAddressBalance(const std::string& address) {
    try {
        std::string response = MakeRequest("addrs/" + address + "/balance");
        if (response.empty()) {
            return std::nullopt;
        }

        json j = json::parse(response);

        AddressBalance balance;
        balance.address = j.value("address", "");
        balance.balance = j.value("balance", 0ULL);
        balance.unconfirmed_balance = j.value("unconfirmed_balance", 0ULL);
        balance.n_tx = j.value("n_tx", 0U);
        balance.final_balance_str = std::to_string(balance.balance);
        balance.unconfirmed_balance_str = std::to_string(balance.unconfirmed_balance);

        return balance;
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher", "Error parsing address balance: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<std::vector<std::string>> BlockCypherClient::GetAddressTransactions(
    const std::string& address, uint32_t limit) {
    try {
        std::string endpoint = "addrs/" + address + "?limit=" + std::to_string(limit);
        std::string response = MakeRequest(endpoint);
        if (response.empty()) {
            return std::nullopt;
        }

        json j = json::parse(response);
        std::vector<std::string> tx_hashes;

        if (j.contains("txrefs") && j["txrefs"].is_array()) {
            for (const auto& tx_ref : j["txrefs"]) {
                if (tx_ref.contains("tx_hash")) {
                    tx_hashes.push_back(tx_ref["tx_hash"]);
                }
            }
        }

        return tx_hashes;
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher",
                       "Error parsing address transactions: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<Transaction> BlockCypherClient::GetTransaction(const std::string& tx_hash) {
    try {
        std::string response = MakeRequest("txs/" + tx_hash);
        if (response.empty()) {
            return std::nullopt;
        }

        json j = json::parse(response);

        Transaction tx;
        tx.hash = j.value("hash", "");
        tx.total = j.value("total", 0ULL);
        tx.fees = j.value("fees", 0ULL);
        tx.size = j.value("size", 0U);
        tx.vsize = j.value("vsize", 0U);
        tx.preference = j.value("preference", "");
        tx.relayed_by = j.value("relayed_by", "");
        tx.received = j.value("received", "");
        tx.ver = j.value("ver", 0U);
        tx.lock_time = j.value("lock_time", 0U);
        tx.double_spend = j.value("double_spend", false);
        tx.vin_sz = j.value("vin_sz", 0U);
        tx.vout_sz = j.value("vout_sz", 0U);
        tx.confirmations = j.value("confirmations", 0U);

        // Parse inputs
        if (j.contains("inputs") && j["inputs"].is_array()) {
            for (const auto& input_json : j["inputs"]) {
                TransactionInput input;
                input.prev_hash = input_json.value("prev_hash", "");
                input.output_index = input_json.value("output_index", 0U);
                input.script = input_json.value("script", "");
                input.output_value = input_json.value("output_value", 0ULL);
                input.sequence = input_json.value("sequence", 0U);
                input.script_type = input_json.value("script_type", "");
                input.age = input_json.value("age", 0U);

                if (input_json.contains("addresses") && input_json["addresses"].is_array()) {
                    for (const auto& addr : input_json["addresses"]) {
                        input.addresses.push_back(addr);
                    }
                }

                tx.inputs.push_back(input);
            }
        }

        // Parse outputs
        if (j.contains("outputs") && j["outputs"].is_array()) {
            for (const auto& output_json : j["outputs"]) {
                TransactionOutput output;
                output.value = output_json.value("value", 0ULL);
                output.script = output_json.value("script", "");
                output.script_type = output_json.value("script_type", "");
                output.spent_by_index = output_json.value("spent_by", 0U);

                if (output_json.contains("addresses") && output_json["addresses"].is_array()) {
                    for (const auto& addr : output_json["addresses"]) {
                        output.addresses.push_back(addr);
                    }
                }

                tx.outputs.push_back(output);
            }
        }

        return tx;
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher", "Error parsing transaction: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<CreateTransactionResponse> BlockCypherClient::CreateTransaction(
    const CreateTransactionRequest& request) {
    try {
        json payload;

        // Build inputs
        payload["inputs"] = json::array();
        for (const auto& addr : request.input_addresses) {
            payload["inputs"].push_back({{"addresses", {addr}}});
        }

        // Build outputs
        payload["outputs"] = json::array();
        for (const auto& output : request.outputs) {
            payload["outputs"].push_back({{"addresses", {output.first}}, {"value", output.second}});
        }

        // Add fees if specified
        if (request.fees > 0) {
            payload["fees"] = request.fees;
        }

        std::string response = MakeRequest("txs/new", "POST", payload.dump());
        if (response.empty()) {
            return std::nullopt;
        }

        json j = json::parse(response);

        CreateTransactionResponse create_response;

        // Check for errors
        if (j.contains("errors") && j["errors"].is_array() && !j["errors"].empty()) {
            create_response.errors = j["errors"][0].value("error", "Unknown error");
            return create_response;
        }

        // Parse transaction
        if (j.contains("tx")) {
            auto tx_opt = GetTransaction(j["tx"]["hash"]);
            if (tx_opt) {
                create_response.tx = tx_opt.value();
            }
        }

        // Parse signing data
        if (j.contains("tosign") && j["tosign"].is_array()) {
            for (const auto& item : j["tosign"]) {
                create_response.tosign.push_back(item);
            }
        }

        if (j.contains("signatures") && j["signatures"].is_array()) {
            for (const auto& item : j["signatures"]) {
                create_response.signatures.push_back(item);
            }
        }

        if (j.contains("pubkeys") && j["pubkeys"].is_array()) {
            for (const auto& item : j["pubkeys"]) {
                create_response.pubkeys.push_back(item);
            }
        }

        return create_response;
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher", "Error creating transaction: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<std::string> BlockCypherClient::SendSignedTransaction(
    const CreateTransactionResponse& signed_tx) {
    try {
        json payload;

        // Build the payload with signatures and public keys
        payload["tosign"] = json::array();
        for (const auto& tosign_item : signed_tx.tosign) {
            payload["tosign"].push_back(tosign_item);
        }

        payload["signatures"] = json::array();
        for (const auto& sig : signed_tx.signatures) {
            payload["signatures"].push_back(sig);
        }

        payload["pubkeys"] = json::array();
        for (const auto& pubkey : signed_tx.pubkeys) {
            payload["pubkeys"].push_back(pubkey);
        }

        std::string response = MakeRequest("txs/send", "POST", payload.dump());
        if (response.empty()) {
            return std::nullopt;
        }

        json j = json::parse(response);

        // Check for errors
        if (j.contains("errors") && j["errors"].is_array() && !j["errors"].empty()) {
            REPO_LOG_ERROR("BlockCypher",
                           "BlockCypher error: " + j["errors"][0].value("error", "Unknown error"));
            return std::nullopt;
        }

        if (j.contains("tx") && j["tx"].contains("hash")) {
            return j["tx"]["hash"];
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher", "Error sending signed transaction: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<std::string> BlockCypherClient::SendRawTransaction(const std::string& hex) {
    try {
        json payload;
        payload["tx"] = hex;

        std::string response = MakeRequest("txs/push", "POST", payload.dump());
        if (response.empty()) {
            return std::nullopt;
        }

        json j = json::parse(response);

        if (j.contains("tx") && j["tx"].contains("hash")) {
            return j["tx"]["hash"];
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher", "Error sending raw transaction: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool BlockCypherClient::IsValidAddress(const std::string& address) {
    // Basic Bitcoin address validation
    if (address.empty())
        return false;

    // Bitcoin mainnet addresses
    if (network == "btc/main") {
        // Legacy addresses (1...)
        if (address[0] == '1' && address.length() >= 26 && address.length() <= 35) {
            return true;
        }
        // P2SH addresses (3...)
        if (address[0] == '3' && address.length() >= 26 && address.length() <= 35) {
            return true;
        }
        // Bech32 addresses (bc1...)
        if (address.substr(0, 3) == "bc1" && address.length() >= 42 && address.length() <= 62) {
            return true;
        }
    }
    // Bitcoin testnet addresses
    else if (network == "btc/test3") {
        // Testnet legacy (m... or n...)
        if ((address[0] == 'm' || address[0] == 'n') && address.length() >= 26 &&
            address.length() <= 35) {
            return true;
        }
        // Testnet P2SH (2...)
        if (address[0] == '2' && address.length() >= 26 && address.length() <= 35) {
            return true;
        }
        // Testnet Bech32 (tb1...)
        if (address.substr(0, 3) == "tb1" && address.length() >= 42 && address.length() <= 62) {
            return true;
        }
    }

    return false;
}

std::optional<uint64_t> BlockCypherClient::EstimateFees() {
    try {
        std::string response = MakeRequest("");
        if (response.empty()) {
            return std::nullopt;
        }

        json j = json::parse(response);

        if (j.contains("medium_fee_per_kb")) {
            return j["medium_fee_per_kb"];
        }

        return std::nullopt;
    } catch (const std::exception& e) {
        REPO_LOG_ERROR("BlockCypher", "Error estimating fees: " + std::string(e.what()));
        return std::nullopt;
    }
}

void BlockCypherClient::SetApiToken(const std::string& token) {
    api_token = token;
}

void BlockCypherClient::SetNetwork(const std::string& network) {
    this->network = network;
}

}  // namespace BlockCypher
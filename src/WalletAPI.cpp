#include "../include/WalletAPI.h"
#include <iostream>
#include <cmath>

namespace WalletAPI {

SimpleWallet::SimpleWallet(const std::string& network)
    : current_network(network) {
    client = std::make_unique<BlockCypher::BlockCypherClient>(network);
}

void SimpleWallet::SetApiToken(const std::string& token) {
    if (client) {
        client->SetApiToken(token);
    }
}

void SimpleWallet::SetNetwork(const std::string& network) {
    current_network = network;
    if (client) {
        client->SetNetwork(network);
    }
}

ReceiveInfo SimpleWallet::GetAddressInfo(const std::string& address) {
    ReceiveInfo info;
    info.address = address;
    info.confirmed_balance = 0;
    info.unconfirmed_balance = 0;
    info.transaction_count = 0;

    if (!client) {
        return info;
    }

    // Get balance information
    auto balance_result = client->GetAddressBalance(address);
    if (balance_result) {
        info.confirmed_balance = balance_result->balance;
        info.unconfirmed_balance = balance_result->unconfirmed_balance;
        info.transaction_count = balance_result->n_tx;
    }

    // Get recent transactions
    auto tx_result = client->GetAddressTransactions(address, 10);
    if (tx_result) {
        info.recent_transactions = tx_result.value();
    }

    return info;
}

uint64_t SimpleWallet::GetBalance(const std::string& address) {
    if (!client) {
        return 0;
    }

    auto balance_result = client->GetAddressBalance(address);
    if (balance_result) {
        return balance_result->balance;
    }

    return 0;
}

std::vector<std::string> SimpleWallet::GetTransactionHistory(const std::string& address, uint32_t limit) {
    if (!client) {
        return {};
    }

    auto tx_result = client->GetAddressTransactions(address, limit);
    if (tx_result) {
        return tx_result.value();
    }

    return {};
}

SendTransactionResult SimpleWallet::SendFunds(
    const std::vector<std::string>& from_addresses,
    const std::string& to_address,
    uint64_t amount_satoshis,
    uint64_t fee_satoshis) {

    SendTransactionResult result;
    result.success = false;
    result.total_fees = 0;

    if (!client) {
        result.error_message = "BlockCypher client not initialized";
        return result;
    }

    // Validate addresses
    for (const auto& addr : from_addresses) {
        if (!client->IsValidAddress(addr)) {
            result.error_message = "Invalid source address: " + addr;
            return result;
        }
    }

    if (!client->IsValidAddress(to_address)) {
        result.error_message = "Invalid destination address: " + to_address;
        return result;
    }

    // Check balances
    uint64_t total_available = 0;
    for (const auto& addr : from_addresses) {
        auto balance_result = client->GetAddressBalance(addr);
        if (balance_result) {
            total_available += balance_result->balance;
        }
    }

    // Estimate fees if not provided
    if (fee_satoshis == 0) {
        auto fee_estimate = client->EstimateFees();
        if (fee_estimate) {
            // Rough estimate: base fee per KB for average transaction size (~250 bytes)
            fee_satoshis = (fee_estimate.value() * 250) / 1000;
        } else {
            fee_satoshis = 10000; // Default 10k satoshis (~$4 at $40k BTC)
        }
    }

    if (total_available < amount_satoshis + fee_satoshis) {
        result.error_message = "Insufficient funds. Available: " + std::to_string(total_available) +
                              " satoshis, Required: " + std::to_string(amount_satoshis + fee_satoshis) + " satoshis";
        return result;
    }

    // Create transaction request
    BlockCypher::CreateTransactionRequest tx_request;
    tx_request.input_addresses = from_addresses;
    tx_request.outputs.emplace_back(to_address, amount_satoshis);
    tx_request.fees = fee_satoshis;

    // Create the transaction
    auto create_result = client->CreateTransaction(tx_request);
    if (!create_result) {
        result.error_message = "Failed to create transaction";
        return result;
    }

    if (!create_result->errors.empty()) {
        result.error_message = "Transaction creation error: " + create_result->errors;
        return result;
    }

    // Note: In a real wallet, you would need to:
    // 1. Sign the transaction using private keys
    // 2. Submit the signed transaction using SendRawTransaction
    //
    // For this example, we're showing the transaction creation process
    // The actual signing requires private key management which should be
    // handled securely by the wallet's cryptographic components

    result.success = true;
    result.transaction_hash = create_result->tx.hash;
    result.total_fees = fee_satoshis;
    result.error_message = "Transaction created successfully. Note: Signing and broadcasting not implemented in this demo.";

    return result;
}

bool SimpleWallet::ValidateAddress(const std::string& address) {
    if (!client) {
        return false;
    }
    return client->IsValidAddress(address);
}

uint64_t SimpleWallet::EstimateTransactionFee() {
    if (!client) {
        return 10000; // Default fallback
    }

    auto fee_estimate = client->EstimateFees();
    if (fee_estimate) {
        // Estimate for average transaction size (~250 bytes)
        return (fee_estimate.value() * 250) / 1000;
    }

    return 10000; // Default fallback
}

uint64_t SimpleWallet::ConvertBTCToSatoshis(double btc_amount) {
    return static_cast<uint64_t>(std::round(btc_amount * 100000000.0));
}

double SimpleWallet::ConvertSatoshisToBTC(uint64_t satoshis) {
    return static_cast<double>(satoshis) / 100000000.0;
}

std::string SimpleWallet::GetNetworkInfo() {
    return "Connected to BlockCypher API - Network: " + current_network;
}

} // namespace WalletAPI
#include "WalletAPI.h"
#include "Crypto.h"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <sstream>

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
    const std::map<std::string, std::vector<uint8_t>>& private_keys,
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

    // Verify we have private keys for all input addresses
    for (const auto& addr : from_addresses) {
        if (private_keys.find(addr) == private_keys.end()) {
            result.error_message = "Missing private key for address: " + addr;
            return result;
        }
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

    // Create the transaction skeleton
    auto create_result = client->CreateTransaction(tx_request);
    if (!create_result) {
        result.error_message = "Failed to create transaction";
        return result;
    }

    if (!create_result->errors.empty()) {
        result.error_message = "Transaction creation error: " + create_result->errors;
        return result;
    }

    // Sign the transaction
    // BlockCypher provides hashes to sign in the 'tosign' array
    if (create_result->tosign.empty()) {
        result.error_message = "No hashes to sign in transaction";
        return result;
    }

    // Sign each hash
    create_result->signatures.clear();
    create_result->pubkeys.clear();

    for (size_t i = 0; i < create_result->tosign.size(); i++) {
        // Get the hash to sign (hex string from BlockCypher)
        std::string tosign_hex = create_result->tosign[i];

        // Convert hex string to bytes
        std::array<uint8_t, 32> hash_bytes;
        for (size_t j = 0; j < 32; j++) {
            std::string byte_str = tosign_hex.substr(j * 2, 2);
            hash_bytes[j] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        }

        // Get the private key for this input (use first address key for now)
        // In a more sophisticated implementation, you'd map inputs to specific addresses
        const auto& priv_key = private_keys.begin()->second;

        // Sign the hash
        Crypto::ECDSASignature signature;
        if (!Crypto::SignHash(priv_key, hash_bytes, signature)) {
            result.error_message = "Failed to sign transaction hash " + std::to_string(i);
            return result;
        }

        // Convert signature to hex string
        std::stringstream sig_hex;
        sig_hex << std::hex << std::setfill('0');
        for (uint8_t byte : signature.der_encoded) {
            sig_hex << std::setw(2) << static_cast<int>(byte);
        }
        create_result->signatures.push_back(sig_hex.str());

        // Extract public key from private key
        std::vector<uint8_t> public_key;
        if (!Crypto::DerivePublicKey(priv_key, public_key)) {
            result.error_message = "Failed to derive public key from private key";
            return result;
        }

        // Convert public key to hex string
        std::stringstream pubkey_hex;
        pubkey_hex << std::hex << std::setfill('0');
        for (uint8_t byte : public_key) {
            pubkey_hex << std::setw(2) << static_cast<int>(byte);
        }
        create_result->pubkeys.push_back(pubkey_hex.str());
    }

    // Send the signed transaction
    auto send_result = client->SendSignedTransaction(*create_result);
    if (!send_result) {
        result.error_message = "Failed to broadcast transaction";
        return result;
    }

    // Success!
    result.success = true;
    result.transaction_hash = send_result.value();
    result.total_fees = fee_satoshis;
    result.error_message = "Transaction signed and broadcast successfully";

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
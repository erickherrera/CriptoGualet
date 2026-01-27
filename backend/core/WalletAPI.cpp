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
    provider_config.type = BitcoinProviderType::BlockCypher;
    provider_config.enableFallback = true;
    ApplyProviderConfig(provider_config);
}

void SimpleWallet::SetApiToken(const std::string& token) {
    api_token = token;
    if (client) {
        client->SetApiToken(token);
    }
    ApplyProviderConfig(provider_config);
}

void SimpleWallet::SetNetwork(const std::string& network) {
    current_network = network;
    if (client) {
        client->SetNetwork(network);
    }
    ApplyProviderConfig(provider_config);
}

void SimpleWallet::ApplyProviderConfig(const BitcoinProviderConfig& config) {
    provider_config = config;

    BitcoinProviders::ProviderConfig providerConfig;
    providerConfig.network = current_network;
    providerConfig.apiToken = api_token;
    providerConfig.rpcUrl = provider_config.rpcUrl;
    providerConfig.rpcUsername = provider_config.rpcUsername;
    providerConfig.rpcPassword = provider_config.rpcPassword;
    providerConfig.allowInsecureHttp = provider_config.allowInsecureHttp;
    providerConfig.enableFallback = provider_config.enableFallback;
    providerConfig.type = (provider_config.type == BitcoinProviderType::BitcoinRpc)
                              ? BitcoinProviders::ProviderType::BitcoinRpc
                              : BitcoinProviders::ProviderType::BlockCypher;

    provider = BitcoinProviders::CreateProvider(providerConfig);

    if (!provider) {
        BitcoinProviders::ProviderConfig fallbackConfig;
        fallbackConfig.type = BitcoinProviders::ProviderType::BlockCypher;
        fallbackConfig.network = current_network;
        fallbackConfig.apiToken = api_token;
        provider = BitcoinProviders::CreateProvider(fallbackConfig);
    }

    if (provider_config.enableFallback &&
        providerConfig.type != BitcoinProviders::ProviderType::BlockCypher) {
        BitcoinProviders::ProviderConfig fallbackConfig;
        fallbackConfig.type = BitcoinProviders::ProviderType::BlockCypher;
        fallbackConfig.network = current_network;
        fallbackConfig.apiToken = api_token;
        fallbackProvider = BitcoinProviders::CreateProvider(fallbackConfig);
    } else {
        fallbackProvider.reset();
    }
}

ReceiveInfo SimpleWallet::GetAddressInfo(const std::string& address) {
    ReceiveInfo info;
    info.address = address;
    info.confirmed_balance = 0;
    info.unconfirmed_balance = 0;
    info.transaction_count = 0;

    std::optional<BitcoinProviders::AddressInfo> providerInfo;
    if (provider) {
        providerInfo = provider->getAddressInfo(address, 10);
    }
    if (!providerInfo && fallbackProvider) {
        providerInfo = fallbackProvider->getAddressInfo(address, 10);
    }

    if (providerInfo) {
        info.confirmed_balance = providerInfo->confirmedBalance;
        info.unconfirmed_balance = providerInfo->unconfirmedBalance;
        info.transaction_count = providerInfo->transactionCount;
        info.recent_transactions = providerInfo->recentTransactions;
    }

    return info;
}

uint64_t SimpleWallet::GetBalance(const std::string& address) {
    std::optional<uint64_t> balance;
    if (provider) {
        balance = provider->getBalance(address);
    }
    if (!balance && fallbackProvider) {
        balance = fallbackProvider->getBalance(address);
    }

    return balance.value_or(0);
}

std::vector<std::string> SimpleWallet::GetTransactionHistory(const std::string& address, uint32_t limit) {
    std::optional<BitcoinProviders::AddressInfo> providerInfo;
    if (provider) {
        providerInfo = provider->getAddressInfo(address, limit);
    }
    if (!providerInfo && fallbackProvider) {
        providerInfo = fallbackProvider->getAddressInfo(address, limit);
    }

    if (providerInfo) {
        return providerInfo->recentTransactions;
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
    std::optional<uint64_t> feeRate;
    if (provider) {
        feeRate = provider->estimateFeeRate();
    }
    if (!feeRate && fallbackProvider) {
        feeRate = fallbackProvider->estimateFeeRate();
    }

    if (feeRate) {
        return (feeRate.value() * 250) / 1000;
    }

    if (client) {
        auto fee_estimate = client->EstimateFees();
        if (fee_estimate) {
            return (fee_estimate.value() * 250) / 1000;
        }
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
    std::string providerName = "BlockCypher";
    if (provider) {
        providerName = provider->name();
    }
    return "Connected to " + providerName + " - Network: " + current_network;
}

// ===== Ethereum Wallet Implementation =====

EthereumWallet::EthereumWallet(const std::string& network)
    : current_network(network) {
    client = std::make_unique<EthereumService::EthereumClient>(network);
}

void EthereumWallet::SetApiToken(const std::string& token) {
    if (client) {
        client->SetApiToken(token);
    }
}

void EthereumWallet::SetNetwork(const std::string& network) {
    current_network = network;
    if (client) {
        client->SetNetwork(network);
    }
}

std::optional<EthereumService::AddressBalance> EthereumWallet::GetAddressInfo(const std::string& address) {
    if (!client) {
        return std::nullopt;
    }
    return client->GetAddressBalance(address);
}

double EthereumWallet::GetBalance(const std::string& address) {
    if (!client) {
        return 0.0;
    }

    auto balance_result = client->GetAddressBalance(address);
    if (balance_result) {
        return balance_result->balance_eth;
    }

    return 0.0;
}

std::vector<EthereumService::Transaction> EthereumWallet::GetTransactionHistory(const std::string& address, uint32_t limit) {
    if (!client) {
        return {};
    }

    auto tx_result = client->GetTransactionHistory(address, limit);
    if (tx_result) {
        return tx_result.value();
    }

    return {};
}

std::optional<EthereumService::GasPrice> EthereumWallet::GetGasPrice() {
    if (!client) {
        return std::nullopt;
    }
    return client->GetGasPrice();
}

bool EthereumWallet::ValidateAddress(const std::string& address) {
    if (!client) {
        return false;
    }
    return client->IsValidAddress(address);
}

double EthereumWallet::ConvertWeiToEth(const std::string& wei_str) {
    return EthereumService::EthereumClient::WeiToEth(wei_str);
}

std::string EthereumWallet::ConvertEthToWei(double eth) {
    return EthereumService::EthereumClient::EthToWei(eth);
}

ImportTokenResult EthereumWallet::importERC20Token(int walletId, const std::string& contractAddress, Repository::TokenRepository& tokenRepo) {
    if (!client) {
        return {false, "Ethereum client not initialized."};
    }

    if (!client->IsValidAddress(contractAddress)) {
        return {false, "Invalid contract address."};
    }

    // Check if token already exists in the repository
    auto existingToken = tokenRepo.getToken(walletId, contractAddress);
    if (existingToken.success) {
        // If token already exists, fetch its info to return it
        auto tokenInfoOpt = client->GetTokenInfo(contractAddress);
        if (tokenInfoOpt) {
            return {false, "Token already imported.", tokenInfoOpt.value()};
        }
        // If we can't get info, but it's in the DB, it's a strange state
        return {false, "Token already exists in DB, but could not fetch info."};
    }

    // Get token info from blockchain
    auto tokenInfoOpt = client->GetTokenInfo(contractAddress);
    if (!tokenInfoOpt.has_value()) {
        return {false, "Failed to retrieve token information from the blockchain."};
    }

    auto& tokenInfo = tokenInfoOpt.value();

    // Save token to repository
    auto createResult = tokenRepo.createToken(walletId, tokenInfo.contract_address, tokenInfo.symbol, tokenInfo.name, tokenInfo.decimals);
    if (!createResult.success) {
        return {false, "Failed to save token to the database."};
    }

    return {true, "Token imported successfully.", tokenInfo};
}

std::optional<std::string> EthereumWallet::GetTokenBalance(const std::string& walletAddress, const std::string& contractAddress) {
    if (!client) {
        return std::nullopt;
    }
    return client->GetTokenBalance(walletAddress, contractAddress);
}

std::optional<EthereumService::TokenInfo> EthereumWallet::GetTokenInfo(const std::string& contractAddress) {
    if (!client) {
        return std::nullopt;
    }
    return client->GetTokenInfo(contractAddress);
}

EthereumSendResult EthereumWallet::SendFunds(
    const std::string& from_address,
    const std::string& to_address,
    double amount_eth,
    const std::string& private_key_hex,
    const std::string& gas_price_gwei,
    uint64_t gas_limit
) {
    EthereumSendResult result;
    result.success = false;
    result.total_cost_eth = 0.0;

    if (!client) {
        result.error_message = "Ethereum client not initialized";
        return result;
    }

    // Validate addresses
    if (!client->IsValidAddress(from_address)) {
        result.error_message = "Invalid source address";
        return result;
    }

    if (!client->IsValidAddress(to_address)) {
        result.error_message = "Invalid destination address";
        return result;
    }

    // Validate private key format
    if (private_key_hex.empty()) {
        result.error_message = "Private key is required";
        return result;
    }

    // Check balance
    auto balance_info = client->GetAddressBalance(from_address);
    if (!balance_info.has_value()) {
        result.error_message = "Failed to retrieve balance for source address";
        return result;
    }

    // Convert amount to Wei
    std::string value_wei = EthereumService::EthereumClient::EthToWei(amount_eth);

    // Get or estimate gas price
    std::string gas_price_wei;
    if (gas_price_gwei.empty()) {
        // Auto-estimate using "propose" (average) gas price
        auto gas_price_result = client->GetGasPrice();
        if (!gas_price_result.has_value()) {
            result.error_message = "Failed to estimate gas price";
            return result;
        }

        // Use proposed gas price (convert Gwei to Wei)
        double propose_gwei = std::stod(gas_price_result->propose_gas_price);
        gas_price_wei = EthereumService::EthereumClient::GweiToWei(propose_gwei);
    } else {
        // Use provided gas price (convert Gwei to Wei)
        double gwei = std::stod(gas_price_gwei);
        gas_price_wei = EthereumService::EthereumClient::GweiToWei(gwei);
    }

    // Calculate total cost (value + gas fees)
    // Gas cost = gas_price * gas_limit
    double gas_price_wei_double = std::stod(gas_price_wei);
    double total_gas_cost_wei = gas_price_wei_double * gas_limit;
    double value_wei_double = std::stod(value_wei);
    double total_cost_wei_double = value_wei_double + total_gas_cost_wei;

    // Check if balance is sufficient
    double balance_wei_double = std::stod(balance_info->balance_wei);
    if (balance_wei_double < total_cost_wei_double) {
        result.error_message = "Insufficient funds. Balance: " +
                              std::to_string(balance_info->balance_eth) +
                              " ETH, Required: " +
                              std::to_string(EthereumService::EthereumClient::WeiToEth(std::to_string(static_cast<uint64_t>(total_cost_wei_double)))) +
                              " ETH (including gas)";
        return result;
    }

    // Determine chain ID based on network
    uint64_t chain_id = 1;  // Mainnet
    if (current_network == "sepolia") {
        chain_id = 11155111;
    } else if (current_network == "goerli") {
        chain_id = 5;
    }

    // Create and sign transaction
    auto signed_tx = client->CreateSignedTransaction(
        from_address,
        to_address,
        value_wei,
        gas_price_wei,
        gas_limit,
        private_key_hex,
        chain_id
    );

    if (!signed_tx.has_value()) {
        result.error_message = "Failed to create signed transaction";
        return result;
    }

    // Broadcast transaction
    auto tx_hash_result = client->BroadcastTransaction(signed_tx.value());
    if (!tx_hash_result.has_value()) {
        result.error_message = "Failed to broadcast transaction to network";
        return result;
    }

    // Success!
    result.success = true;
    result.transaction_hash = tx_hash_result.value();
    result.total_cost_wei = std::to_string(static_cast<uint64_t>(total_cost_wei_double));
    result.total_cost_eth = EthereumService::EthereumClient::WeiToEth(result.total_cost_wei);

    return result;
}

std::string EthereumWallet::GetNetworkInfo() {
    return "Connected to Etherscan API - Network: " + current_network;
}

// ===== Litecoin Wallet Implementation =====

LitecoinWallet::LitecoinWallet(const std::string& network)
    : current_network(network) {
    // BlockCypher supports Litecoin with "ltc/main" network
    client = std::make_unique<BlockCypher::BlockCypherClient>(network);
}

void LitecoinWallet::SetApiToken(const std::string& token) {
    if (client) {
        client->SetApiToken(token);
    }
}

void LitecoinWallet::SetNetwork(const std::string& network) {
    current_network = network;
    if (client) {
        client->SetNetwork(network);
    }
}

LitecoinReceiveInfo LitecoinWallet::GetAddressInfo(const std::string& address) {
    LitecoinReceiveInfo info;
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

uint64_t LitecoinWallet::GetBalance(const std::string& address) {
    if (!client) {
        return 0;
    }

    auto balance_result = client->GetAddressBalance(address);
    if (balance_result) {
        return balance_result->balance;
    }

    return 0;
}

std::vector<std::string> LitecoinWallet::GetTransactionHistory(const std::string& address, uint32_t limit) {
    if (!client) {
        return {};
    }

    auto tx_result = client->GetAddressTransactions(address, limit);
    if (tx_result) {
        return tx_result.value();
    }

    return {};
}

LitecoinSendResult LitecoinWallet::SendFunds(
    const std::vector<std::string>& from_addresses,
    const std::string& to_address,
    uint64_t amount_litoshis,
    const std::map<std::string, std::vector<uint8_t>>& private_keys,
    uint64_t fee_litoshis) {

    LitecoinSendResult result;
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
    if (fee_litoshis == 0) {
        auto fee_estimate = client->EstimateFees();
        if (fee_estimate) {
            // Rough estimate: base fee per KB for average transaction size (~250 bytes)
            fee_litoshis = (fee_estimate.value() * 250) / 1000;
        } else {
            fee_litoshis = 10000; // Default 10k litoshis
        }
    }

    if (total_available < amount_litoshis + fee_litoshis) {
        result.error_message = "Insufficient funds. Available: " + std::to_string(total_available) +
                              " litoshis, Required: " + std::to_string(amount_litoshis + fee_litoshis) + " litoshis";
        return result;
    }

    // Create transaction request
    BlockCypher::CreateTransactionRequest tx_request;
    tx_request.input_addresses = from_addresses;
    tx_request.outputs.emplace_back(to_address, amount_litoshis);
    tx_request.fees = fee_litoshis;

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
    if (create_result->tosign.empty()) {
        result.error_message = "No hashes to sign in transaction";
        return result;
    }

    // Sign each hash
    create_result->signatures.clear();
    create_result->pubkeys.clear();

    for (size_t i = 0; i < create_result->tosign.size(); i++) {
        std::string tosign_hex = create_result->tosign[i];

        // Convert hex string to bytes
        std::array<uint8_t, 32> hash_bytes;
        for (size_t j = 0; j < 32; j++) {
            std::string byte_str = tosign_hex.substr(j * 2, 2);
            hash_bytes[j] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        }

        // Get the private key for this input
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
    result.total_fees = fee_litoshis;
    result.error_message = "Transaction signed and broadcast successfully";

    return result;
}

bool LitecoinWallet::ValidateAddress(const std::string& address) {
    if (!client) {
        return false;
    }
    return client->IsValidAddress(address);
}

uint64_t LitecoinWallet::EstimateTransactionFee() {
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

uint64_t LitecoinWallet::ConvertLTCToLitoshis(double ltc_amount) {
    return static_cast<uint64_t>(std::round(ltc_amount * 100000000.0));
}

double LitecoinWallet::ConvertLitoshisToLTC(uint64_t litoshis) {
    return static_cast<double>(litoshis) / 100000000.0;
}

std::string LitecoinWallet::GetNetworkInfo() {
    return "Connected to BlockCypher API - Network: " + current_network;
}

} // namespace WalletAPI
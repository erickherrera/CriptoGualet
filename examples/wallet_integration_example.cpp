/*
 * Example: Integrating BlockCypher API with the CriptoGualet Wallet
 *
 * This file demonstrates how to integrate the BlockCypher API functionality
 * into your existing Qt wallet application.
 */

#include "../include/WalletAPI.h"
#include "../include/BlockCypher.h"
#include <iostream>
#include <memory>

class WalletWithBlockchain {
private:
    std::unique_ptr<WalletAPI::SimpleWallet> blockchain_api;
    std::string api_token;
    bool testnet_mode;

public:
    WalletWithBlockchain(bool use_testnet = true) : testnet_mode(use_testnet) {
        std::string network = use_testnet ? "btc/test3" : "btc/main";
        blockchain_api = std::make_unique<WalletAPI::SimpleWallet>(network);
    }

    // Configuration methods
    void SetBlockCypherToken(const std::string& token) {
        api_token = token;
        if (blockchain_api) {
            blockchain_api->SetApiToken(token);
        }
    }

    void SwitchNetwork(bool use_testnet) {
        testnet_mode = use_testnet;
        std::string network = use_testnet ? "btc/test3" : "btc/main";
        if (blockchain_api) {
            blockchain_api->SetNetwork(network);
        }
    }

    // Wallet operations that would integrate with your Qt UI

    /**
     * Check balance for display in wallet UI
     */
    uint64_t CheckWalletBalance(const std::string& address) {
        if (!blockchain_api) return 0;

        return blockchain_api->GetBalance(address);
    }

    /**
     * Get formatted balance string for UI display
     */
    std::string GetFormattedBalance(const std::string& address) {
        uint64_t balance = CheckWalletBalance(address);
        double btc_balance = blockchain_api->ConvertSatoshisToBTC(balance);

        return std::to_string(btc_balance) + " BTC (" + std::to_string(balance) + " satoshis)";
    }

    /**
     * Refresh transaction history for display in Qt list widget
     */
    std::vector<std::string> RefreshTransactionHistory(const std::string& address, uint32_t limit = 20) {
        if (!blockchain_api) return {};

        return blockchain_api->GetTransactionHistory(address, limit);
    }

    /**
     * Prepare a transaction for sending (returns info for user confirmation)
     */
    struct TransactionPreview {
        bool can_send;
        std::string error_message;
        uint64_t amount_satoshis;
        uint64_t estimated_fee;
        uint64_t total_required;
        uint64_t available_balance;
        std::string from_address;
        std::string to_address;
    };

    TransactionPreview PrepareSendTransaction(
        const std::string& from_address,
        const std::string& to_address,
        double btc_amount) {

        TransactionPreview preview;
        preview.can_send = false;
        preview.from_address = from_address;
        preview.to_address = to_address;

        if (!blockchain_api) {
            preview.error_message = "Blockchain API not initialized";
            return preview;
        }

        // Validate addresses
        if (!blockchain_api->ValidateAddress(from_address)) {
            preview.error_message = "Invalid source address";
            return preview;
        }

        if (!blockchain_api->ValidateAddress(to_address)) {
            preview.error_message = "Invalid destination address";
            return preview;
        }

        // Convert amount and estimate fees
        preview.amount_satoshis = blockchain_api->ConvertBTCToSatoshis(btc_amount);
        preview.estimated_fee = blockchain_api->EstimateTransactionFee();
        preview.total_required = preview.amount_satoshis + preview.estimated_fee;
        preview.available_balance = blockchain_api->GetBalance(from_address);

        if (preview.available_balance < preview.total_required) {
            preview.error_message = "Insufficient funds. Available: " +
                                  std::to_string(preview.available_balance) +
                                  " satoshis, Required: " +
                                  std::to_string(preview.total_required) + " satoshis";
            return preview;
        }

        preview.can_send = true;
        return preview;
    }

    /**
     * Execute the transaction (after user confirmation)
     */
    WalletAPI::SendTransactionResult ExecuteSendTransaction(const TransactionPreview& preview) {
        if (!preview.can_send || !blockchain_api) {
            WalletAPI::SendTransactionResult result;
            result.success = false;
            result.error_message = "Cannot execute transaction: " + preview.error_message;
            return result;
        }

        std::vector<std::string> from_addresses = {preview.from_address};
        return blockchain_api->SendFunds(from_addresses, preview.to_address,
                                       preview.amount_satoshis, preview.estimated_fee);
    }

    /**
     * Get comprehensive address information for wallet dashboard
     */
    WalletAPI::ReceiveInfo GetAddressDashboardInfo(const std::string& address) {
        if (!blockchain_api) {
            WalletAPI::ReceiveInfo empty_info;
            empty_info.address = address;
            return empty_info;
        }

        return blockchain_api->GetAddressInfo(address);
    }

    /**
     * Utility methods for Qt UI integration
     */

    bool IsTestnetMode() const { return testnet_mode; }

    std::string GetNetworkDisplayName() const {
        return testnet_mode ? "Bitcoin Testnet" : "Bitcoin Mainnet";
    }

    std::string GetApiStatus() const {
        if (!blockchain_api) return "API Not Initialized";
        return blockchain_api->GetNetworkInfo();
    }
};

// Example usage functions that show how to integrate with Qt signals/slots

void ExampleQtIntegration() {
    // This would typically be done in your Qt application class

    WalletWithBlockchain wallet(true); // Use testnet for safety

    // Set API token (you'd get this from user settings or environment)
    // wallet.SetBlockCypherToken("your_api_token_here");

    std::cout << "=== Qt Integration Example ===" << std::endl;
    std::cout << "Network: " << wallet.GetNetworkDisplayName() << std::endl;
    std::cout << "Status: " << wallet.GetApiStatus() << std::endl;

    // Example address (in real app, this would come from your wallet)
    std::string user_address = "mzBc4XEFSdzCDcTxAgf6EZXgsZWpztRhef";

    // Check balance (could be called on Qt timer for auto-refresh)
    std::cout << "\nBalance Check:" << std::endl;
    std::cout << "Address: " << user_address << std::endl;
    std::cout << "Balance: " << wallet.GetFormattedBalance(user_address) << std::endl;

    // Get transaction history (for populating Qt list widget)
    std::cout << "\nTransaction History:" << std::endl;
    auto transactions = wallet.RefreshTransactionHistory(user_address, 5);
    for (size_t i = 0; i < transactions.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << transactions[i] << std::endl;
    }

    // Prepare transaction (when user clicks "Send")
    std::cout << "\nTransaction Preparation:" << std::endl;
    auto preview = wallet.PrepareSendTransaction(
        user_address,
        "mjSk1Ny9spzU2fouzYgLqGUD8U41iR35QN",
        0.001
    );

    std::cout << "Can send: " << (preview.can_send ? "Yes" : "No") << std::endl;
    if (!preview.can_send) {
        std::cout << "Error: " << preview.error_message << std::endl;
    } else {
        std::cout << "Amount: " << preview.amount_satoshis << " satoshis" << std::endl;
        std::cout << "Fee: " << preview.estimated_fee << " satoshis" << std::endl;
        std::cout << "Total: " << preview.total_required << " satoshis" << std::endl;

        // In Qt app, you'd show a confirmation dialog here
        std::cout << "Ready to execute (not executing in example)" << std::endl;
    }
}

int main() {
    std::cout << "BlockCypher API - Qt Wallet Integration Example" << std::endl;
    std::cout << "===============================================" << std::endl;

    try {
        ExampleQtIntegration();

        std::cout << "\n=== Integration Guide ===" << std::endl;
        std::cout << "To integrate this into your Qt wallet:" << std::endl;
        std::cout << "1. Add WalletWithBlockchain as a member of your main window" << std::endl;
        std::cout << "2. Use Qt timers to periodically refresh balance and transactions" << std::endl;
        std::cout << "3. Connect send button to PrepareSendTransaction -> show confirmation -> ExecuteSendTransaction" << std::endl;
        std::cout << "4. Use Qt's signal/slot mechanism to update UI when operations complete" << std::endl;
        std::cout << "5. Handle network errors gracefully with try-catch blocks" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Example failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
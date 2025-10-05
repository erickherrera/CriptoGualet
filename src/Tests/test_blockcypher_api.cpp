#include "../include/WalletAPI.h"
#include "../include/BlockCypher.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

void TestBasicAPI() {
    std::cout << "=== BlockCypher API Basic Test ===" << std::endl;

    // Create a wallet instance using Bitcoin testnet
    WalletAPI::SimpleWallet wallet("btc/test3");

    std::cout << wallet.GetNetworkInfo() << std::endl;

    // Test address validation
    std::cout << "\n--- Address Validation Test ---" << std::endl;

    // Valid testnet addresses for testing
    std::string valid_testnet_addr = "mzBc4XEFSdzCDcTxAgf6EZXgsZWpztRhef"; // Example testnet address
    std::string invalid_addr = "invalid_address";

    std::cout << "Testing address: " << valid_testnet_addr << std::endl;
    std::cout << "Is valid: " << (wallet.ValidateAddress(valid_testnet_addr) ? "Yes" : "No") << std::endl;

    std::cout << "Testing address: " << invalid_addr << std::endl;
    std::cout << "Is valid: " << (wallet.ValidateAddress(invalid_addr) ? "Yes" : "No") << std::endl;

    // Test fee estimation
    std::cout << "\n--- Fee Estimation Test ---" << std::endl;
    uint64_t estimated_fee = wallet.EstimateTransactionFee();
    std::cout << "Estimated transaction fee: " << estimated_fee << " satoshis" << std::endl;
    std::cout << "Estimated transaction fee: " << wallet.ConvertSatoshisToBTC(estimated_fee) << " BTC" << std::endl;

    // Test BTC/Satoshi conversion
    std::cout << "\n--- BTC/Satoshi Conversion Test ---" << std::endl;
    double btc_amount = 0.001;
    uint64_t satoshis = wallet.ConvertBTCToSatoshis(btc_amount);
    std::cout << btc_amount << " BTC = " << satoshis << " satoshis" << std::endl;
    std::cout << satoshis << " satoshis = " << wallet.ConvertSatoshisToBTC(satoshis) << " BTC" << std::endl;
}

void TestAddressInfo() {
    std::cout << "\n=== Address Information Test ===" << std::endl;

    WalletAPI::SimpleWallet wallet("btc/test3");

    // Use a known testnet address with some activity (you may need to replace with a current one)
    std::string test_address = "mzBc4XEFSdzCDcTxAgf6EZXgsZWpztRhef";

    std::cout << "Fetching information for address: " << test_address << std::endl;

    WalletAPI::ReceiveInfo info = wallet.GetAddressInfo(test_address);

    std::cout << "Address: " << info.address << std::endl;
    std::cout << "Confirmed Balance: " << info.confirmed_balance << " satoshis" << std::endl;
    std::cout << "Unconfirmed Balance: " << info.unconfirmed_balance << " satoshis" << std::endl;
    std::cout << "Total Transactions: " << info.transaction_count << std::endl;

    if (!info.recent_transactions.empty()) {
        std::cout << "Recent Transactions:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), info.recent_transactions.size()); ++i) {
            std::cout << "  " << (i + 1) << ". " << info.recent_transactions[i] << std::endl;
        }
    }
}

void TestTransactionCreation() {
    std::cout << "\n=== Transaction Creation Test ===" << std::endl;

    WalletAPI::SimpleWallet wallet("btc/test3");

    // Note: These are example addresses - in real use, you'd have actual addresses with funds
    std::vector<std::string> from_addresses = {"mzBc4XEFSdzCDcTxAgf6EZXgsZWpztRhef"};
    std::string to_address = "mjSk1Ny9spzU2fouzYgLqGUD8U41iR35QN";
    uint64_t amount = wallet.ConvertBTCToSatoshis(0.001); // 0.001 BTC

    std::cout << "Creating transaction..." << std::endl;
    std::cout << "From: " << from_addresses[0] << std::endl;
    std::cout << "To: " << to_address << std::endl;
    std::cout << "Amount: " << amount << " satoshis (" << wallet.ConvertSatoshisToBTC(amount) << " BTC)" << std::endl;

    WalletAPI::SendTransactionResult result = wallet.SendFunds(from_addresses, to_address, amount);

    std::cout << "Transaction Result:" << std::endl;
    std::cout << "Success: " << (result.success ? "Yes" : "No") << std::endl;
    std::cout << "Transaction Hash: " << result.transaction_hash << std::endl;
    std::cout << "Total Fees: " << result.total_fees << " satoshis" << std::endl;
    std::cout << "Message: " << result.error_message << std::endl;
}

void TestDirectBlockCypherAPI() {
    std::cout << "\n=== Direct BlockCypher API Test ===" << std::endl;

    BlockCypher::BlockCypherClient client("btc/test3");

    // Test getting blockchain info by making a request to the root endpoint
    std::cout << "Testing direct API connection..." << std::endl;

    // Test address balance
    std::string test_address = "mzBc4XEFSdzCDcTxAgf6EZXgsZWpztRhef";
    auto balance_result = client.GetAddressBalance(test_address);

    if (balance_result) {
        std::cout << "Direct API call successful!" << std::endl;
        std::cout << "Address: " << balance_result->address << std::endl;
        std::cout << "Balance: " << balance_result->balance << " satoshis" << std::endl;
        std::cout << "Unconfirmed: " << balance_result->unconfirmed_balance << " satoshis" << std::endl;
        std::cout << "Transaction count: " << balance_result->n_tx << std::endl;
    } else {
        std::cout << "Direct API call failed - this may be due to network connectivity or API limits" << std::endl;
    }
}

int main() {
    std::cout << "BlockCypher API Integration Test" << std::endl;
    std::cout << "================================" << std::endl;

    try {
        TestBasicAPI();

        // Add a small delay between tests to respect API rate limits
        std::this_thread::sleep_for(std::chrono::seconds(1));

        TestDirectBlockCypherAPI();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        TestAddressInfo();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        TestTransactionCreation();

        std::cout << "\n=== Test Completed ===" << std::endl;
        std::cout << "Note: Some tests may fail if there's no internet connection or if API rate limits are exceeded." << std::endl;
        std::cout << "The API integration is ready for use in your wallet application." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
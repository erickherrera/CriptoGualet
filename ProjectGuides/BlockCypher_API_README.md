# BlockCypher API Integration for CriptoGualet

This document describes the BlockCypher API integration that provides send/receive functionality for Bitcoin transactions in your CriptoGualet wallet application.

## Overview

The integration consists of three main components:

1. **BlockCypher.h/cpp** - Low-level BlockCypher REST API client
2. **WalletAPI.h/cpp** - High-level wallet operations wrapper
3. **Test and example files** - Demonstrations and testing utilities

## Features

### Send Functionality
- Create Bitcoin transactions
- Estimate transaction fees
- Validate addresses before sending
- Support for multiple input addresses
- Automatic fee calculation

### Receive Functionality
- Check address balances (confirmed and unconfirmed)
- Retrieve transaction history
- Monitor address activity
- Get comprehensive address information

### Utility Functions
- Bitcoin/Satoshi conversions
- Address validation
- Network switching (mainnet/testnet)
- Fee estimation

## Quick Start

### 1. Dependencies

The integration requires these vcpkg packages:
- `cpr` - HTTP client library
- `nlohmann-json` - JSON parsing

These are already added to `vcpkg.json`.

### 2. Basic Usage

```cpp
#include "WalletAPI.h"

// Create wallet instance (testnet for safety)
WalletAPI::SimpleWallet wallet("btc/test3");

// Optional: Set API token for higher rate limits
wallet.SetApiToken("your_blockcypher_token");

// Check balance
uint64_t balance = wallet.GetBalance("your_bitcoin_address");
std::cout << "Balance: " << balance << " satoshis" << std::endl;

// Get address information
WalletAPI::ReceiveInfo info = wallet.GetAddressInfo("your_bitcoin_address");
std::cout << "Confirmed: " << info.confirmed_balance << " satoshis" << std::endl;
std::cout << "Unconfirmed: " << info.unconfirmed_balance << " satoshis" << std::endl;

// Prepare transaction
std::vector<std::string> from_addresses = {"source_address"};
WalletAPI::SendTransactionResult result = wallet.SendFunds(
    from_addresses,
    "destination_address",
    100000, // 0.001 BTC in satoshis
    10000   // fee in satoshis
);

if (result.success) {
    std::cout << "Transaction created: " << result.transaction_hash << std::endl;
} else {
    std::cout << "Error: " << result.error_message << std::endl;
}
```

### 3. API Token Setup

For production use, obtain a free API token from [BlockCypher](https://blockcypher.com):

1. Sign up at https://blockcypher.com
2. Get your API token from the dashboard
3. Set it in your application:

```cpp
wallet.SetApiToken("your_api_token_here");
```

**Rate Limits:**
- Without token: 3 requests/sec, 200 requests/hour
- With free token: 5 requests/sec, 2000 requests/hour

## Network Configuration

### Bitcoin Testnet (Recommended for Development)
```cpp
WalletAPI::SimpleWallet wallet("btc/test3");
```

### Bitcoin Mainnet (Production)
```cpp
WalletAPI::SimpleWallet wallet("btc/main");
```

### Other Supported Networks
- Litecoin: `"litecoin/main"`
- Dogecoin: `"dogecoin/main"`
- Dash: `"dash/main"`

## Testing

### Build and Run Tests

```bash
# Build the test executable
cmake --build . --target test_blockcypher_api

# Run the test
./bin/test_blockcypher_api
```

### Test Coverage

The test includes:
- Basic API connectivity
- Address validation
- Balance checking
- Transaction creation (without signing)
- Fee estimation
- BTC/Satoshi conversions

## Integration with Qt Wallet

See `examples/wallet_integration_example.cpp` for a complete example of integrating the API with your Qt application.

### Key Integration Points

1. **Balance Display**: Use `GetBalance()` or `GetAddressInfo()` with Qt timers for real-time updates
2. **Transaction History**: Use `GetTransactionHistory()` to populate Qt list widgets
3. **Send Dialog**: Use `PrepareSendTransaction()` for validation, then `ExecuteSendTransaction()` after user confirmation
4. **Address Validation**: Use `ValidateAddress()` for real-time input validation

### Qt Signal/Slot Example

```cpp
class WalletWindow : public QMainWindow {
    Q_OBJECT

private:
    WalletAPI::SimpleWallet* blockchain_api;
    QTimer* refresh_timer;

private slots:
    void refreshBalance() {
        uint64_t balance = blockchain_api->GetBalance(current_address);
        ui->balanceLabel->setText(QString::number(balance) + " satoshis");
    }

    void sendButtonClicked() {
        // Validate and prepare transaction
        // Show confirmation dialog
        // Execute if confirmed
    }
};
```

## Security Considerations

### Important Notes

1. **Private Key Management**: This integration handles transaction creation but NOT signing. You must implement secure private key management and transaction signing separately.

2. **Testnet First**: Always test on Bitcoin testnet before using mainnet.

3. **API Token Security**: Store API tokens securely, never hardcode them.

4. **Error Handling**: Always check return values and handle network errors gracefully.

### Transaction Signing

The API creates unsigned transactions. To complete a transaction:

1. Use the `CreateTransaction()` method to get an unsigned transaction
2. Sign the transaction using your private key management system
3. Use `SendRawTransaction()` to broadcast the signed transaction

## Error Handling

Common error scenarios and handling:

```cpp
// Network connectivity
auto result = wallet.GetBalance(address);
if (result == 0) {
    // Could be no balance OR network error
    // Check with GetAddressInfo for more details
}

// Invalid addresses
if (!wallet.ValidateAddress(address)) {
    // Show error to user before proceeding
}

// Insufficient funds
auto send_result = wallet.SendFunds(from, to, amount);
if (!send_result.success) {
    // Display send_result.error_message to user
}
```

## API Reference

### WalletAPI::SimpleWallet

| Method | Description | Returns |
|--------|-------------|---------|
| `GetBalance(address)` | Get confirmed balance | `uint64_t` (satoshis) |
| `GetAddressInfo(address)` | Get comprehensive address info | `ReceiveInfo` struct |
| `GetTransactionHistory(address, limit)` | Get recent transactions | `vector<string>` |
| `SendFunds(from, to, amount, fee)` | Create transaction | `SendTransactionResult` |
| `ValidateAddress(address)` | Check address validity | `bool` |
| `EstimateTransactionFee()` | Get current fee estimate | `uint64_t` (satoshis) |
| `ConvertBTCToSatoshis(btc)` | Convert BTC to satoshis | `uint64_t` |
| `ConvertSatoshisToBTC(satoshis)` | Convert satoshis to BTC | `double` |

### BlockCypher::BlockCypherClient

Lower-level API for advanced usage:

| Method | Description |
|--------|-------------|
| `GetAddressBalance(address)` | Detailed balance info |
| `GetTransaction(hash)` | Full transaction details |
| `CreateTransaction(request)` | Create unsigned transaction |
| `SendRawTransaction(hex)` | Broadcast signed transaction |

## Support and Resources

- [BlockCypher API Documentation](https://www.blockcypher.com/dev/)
- [Bitcoin Testnet Faucet](https://bitcoinfaucet.uo1.net/) - Get test coins
- [Example Integration](examples/wallet_integration_example.cpp)
- [API Test Suite](src/Tests/test_blockcypher_api.cpp)

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure vcpkg dependencies are installed
2. **Network Errors**: Check internet connectivity and API limits
3. **Invalid Addresses**: Use testnet addresses with testnet network
4. **Rate Limiting**: Get an API token for higher limits

### Debug Tips

- Enable verbose logging in the HTTP client for debugging
- Test with known addresses first (see test file examples)
- Use Bitcoin testnet for all development and testing
- Check BlockCypher status page for API outages
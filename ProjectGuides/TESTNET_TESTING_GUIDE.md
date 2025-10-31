# CriptoGualet Testnet Testing Guide

Good news: **Your application is already configured for Bitcoin Testnet**, which means all funds are FAKE and worthless. You can test freely without risking real money!

## What is Bitcoin Testnet?

- **Testnet** (`btc/test3`) = Fake Bitcoin for testing - **SAFE** ✅
- **Mainnet** (`btc/main`) = Real Bitcoin with real money - **DANGEROUS** ⚠️

Your app uses: `btc/test3` (testnet) - you're safe!

## How to Test the Application

### Step 1: Build and Run the Application

```bash
# Configure CMake
cmake --preset=default

# Build debug version
cmake --build out/build/win-clang-x64-debug

# Run the application
./out/build/win-clang-x64-debug/frontend/qt/CriptoGualetQt.exe
```

### Step 2: Create a Test Account

1. Launch the application
2. Click "Register" or switch to the Register tab
3. Create a test account with:
   - Username: `testuser`
   - Email: `test@example.com`
   - Password: `Test123!`
4. **IMPORTANT**: Save your seed phrase when prompted!

### Step 3: Get Your Testnet Bitcoin Address

1. Log in with your test account
2. Click "Receive Bitcoin" or view your wallet address
3. Your address will look like:
   - `mzBc4XEFSdzCDcTxAgf6EZXgsZWpztRhef` (starts with 'm' or 'n')
   - `tb1q...` (Bech32 testnet address)
4. Copy this address - you'll need it for the next step

### Step 4: Get Free Testnet Bitcoin (Fake Funds)

Visit any of these **Bitcoin Testnet Faucets** to get free test coins:

#### Recommended Faucets:

1. **Coinfaucet.eu** (Most reliable)
   - URL: https://coinfaucet.eu/en/btc-testnet/
   - Amount: ~0.001-0.01 tBTC
   - Requirements: Usually just solve a captcha

2. **Bitcoin Testnet Sandbox**
   - URL: https://bitcoinfaucet.uo1.net/
   - Amount: ~0.001 tBTC
   - Requirements: Captcha

3. **Testnet-Faucet.com**
   - URL: https://testnet-faucet.com/btc-testnet/
   - Amount: Varies
   - Requirements: Captcha

4. **Bitcoin Core Project Faucet**
   - URL: https://testnet.qc.to/
   - Amount: ~0.01 tBTC
   - Requirements: None

#### How to Use a Faucet:

1. Go to one of the faucet websites above
2. Paste your testnet Bitcoin address (the one from Step 3)
3. Complete the captcha
4. Click "Send" or "Get Testnet BTC"
5. Wait 5-15 minutes for the transaction to confirm

### Step 5: Check Your Balance

Back in the CriptoGualet application:

1. Click "View Balance" or "Refresh"
2. Wait a few moments for the API to fetch data
3. You should see:
   - **Confirmed Balance**: The testnet Bitcoin you received
   - **Unconfirmed Balance**: Any pending transactions
   - **Transaction Count**: Number of transactions

### Step 6: Test Sending Bitcoin

**WARNING**: Even though this is testnet (fake money), the sending functionality may not be fully implemented yet. Test carefully!

1. Click "Send Bitcoin"
2. Enter:
   - **Recipient Address**: Another testnet address (you can use a second wallet or a faucet return address)
   - **Amount**: A small amount like 0.0001 tBTC
3. Review the transaction details
4. Confirm (if implemented)

## Understanding Testnet Addresses

### Valid Testnet Address Formats:

- **Legacy**: Starts with `m` or `n`
  - Example: `mzBc4XEFSdzCDcTxAgf6EZXgsZWpztRhef`
- **P2SH**: Starts with `2`
  - Example: `2MzQwSSnBHWHqSAqtTVQ6v47XtaisrJa1Vc`
- **Bech32**: Starts with `tb1`
  - Example: `tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx`

### INVALID for Testnet (These are MAINNET - Real Money!):

- Starts with `1` (mainnet legacy)
- Starts with `3` (mainnet P2SH)
- Starts with `bc1` (mainnet Bech32)

## Running the Included Tests

Your project includes comprehensive tests that use testnet:

### Run BlockCypher API Tests

```bash
# Build tests
cmake --build out/build/win-clang-x64-debug

# Run BlockCypher API tests (uses testnet)
./out/build/win-clang-x64-debug/Tests/test_blockcypher_api.exe
```

This test will:
- Validate testnet addresses
- Check address balances
- Fetch transactions
- Test fee estimation
- Create transaction skeletons (without broadcasting)

### Run BIP39 Seed Tests

```bash
./out/build/win-clang-x64-debug/Tests/test_bip39.exe
```

This tests the seed phrase generation (offline, no network needed).

## Testnet vs Mainnet - Configuration

### Your App is Set to Testnet

In `frontend/qt/CriptoGualetQt.cpp` (line 43):
```cpp
m_wallet = std::make_unique<WalletAPI::SimpleWallet>("btc/test3");
```

### To Switch to Mainnet (⚠️ ONLY WHEN READY FOR PRODUCTION)

Change to:
```cpp
m_wallet = std::make_unique<WalletAPI::SimpleWallet>("btc/main");
```

**DO NOT** switch to mainnet unless you're ready to use real Bitcoin!

## Testing Checklist

- [x] ✅ Application is configured for testnet
- [ ] Register a new test account
- [ ] View and copy your testnet wallet address
- [ ] Get free testnet Bitcoin from a faucet
- [ ] Wait for testnet transaction to confirm (5-15 minutes)
- [ ] Check your balance in the app
- [ ] View transaction history
- [ ] Test sending Bitcoin (if implemented)
- [ ] Test receiving Bitcoin again
- [ ] Test with multiple accounts

## Troubleshooting

### "No balance showing after getting testnet BTC"

1. **Wait longer**: Testnet confirmations can take 10-30 minutes
2. **Check BlockCypher**: Visit https://live.blockcypher.com/btc-testnet/ and search for your address
3. **Verify address**: Make sure you copied the correct address
4. **Faucet limits**: Some faucets have daily limits

### "Address validation fails"

- Make sure your address starts with `m`, `n`, `2`, or `tb1` (testnet)
- Addresses starting with `1`, `3`, or `bc1` are mainnet (real money)

### "Transaction creation fails"

This is expected if:
- Your address has no testnet Bitcoin yet
- You haven't gotten funds from a faucet
- The BlockCypher API is rate-limiting (wait and try again)

### "API rate limiting"

BlockCypher has rate limits:
- **Without API token**: 3 requests/second, 200 requests/hour
- **With API token**: Higher limits

To add an API token (optional):
1. Sign up at https://www.blockcypher.com/
2. Get your API token
3. Call `m_wallet->SetApiToken("your-token-here")` in your code

## Important Reminders

1. **Testnet Bitcoin is WORTHLESS** - you can't convert it to real money
2. **Testnet Bitcoin is FREE** - use faucets to get unlimited test coins
3. **Test freely** - you can't lose real money on testnet
4. **Don't store real Bitcoin** - this app is for testing only right now
5. **Testnet can reset** - occasionally testnet chains reset, don't rely on persistence

## What You Can Test

✅ **Safe to test:**
- Account registration
- Seed phrase generation
- Wallet address creation
- Receiving testnet Bitcoin
- Checking balances
- Viewing transaction history
- QR code generation
- Multiple accounts
- Sending testnet Bitcoin (when implemented)

❌ **Don't test with:**
- Real Bitcoin
- Real money
- Production wallets
- Other people's funds

## Next Steps After Testing

Once you're comfortable with testnet:

1. Test all features thoroughly
2. Fix any bugs you find
3. Add more tests
4. Consider adding a UI toggle for testnet/mainnet
5. Add clear warnings when switching to mainnet
6. Conduct security audits
7. Only then consider mainnet deployment

## Need More Test Bitcoin?

If you run out of testnet Bitcoin:
- Just visit a faucet again (they usually have daily limits)
- Use multiple faucets
- Create multiple test addresses
- Most faucets give 0.001-0.01 tBTC per request

## Summary

🎉 **You're all set!** Your application is safely configured for testnet. Get some free testnet Bitcoin from a faucet and start testing!

Remember: **TESTNET = FAKE MONEY = SAFE FOR TESTING** ✅

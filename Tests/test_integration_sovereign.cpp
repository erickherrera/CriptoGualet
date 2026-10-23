#include "../backend/core/include/Auth.h"
#include "../backend/core/include/Crypto.h"
#include "../backend/database/include/Database/DatabaseManager.h"
#include "../backend/repository/include/Repository/TransactionRepository.h"
#include "../backend/repository/include/Repository/UserRepository.h"
#include "../backend/repository/include/Repository/WalletRepository.h"
#include "../backend/utils/include/RLPEncoder.h"
#include "TestUtils.h"

#include <secp256k1.h>
#include <array>
#include <iostream>
#include <string>
#include <vector>

const std::string TEST_DB_PATH = "test_sovereign_integration.db";

// ============================================================================
// The Sovereign Transaction Lifecycle Integration Test
// ============================================================================

void testDeterministicKeyRecovery(std::array<uint8_t, 64>& out_seed) {
    TEST_START("1. Zero-Knowledge Recovery & Cross-Chain Derivation");

    std::vector<std::string> mnemonic = {"abandon", "abandon", "abandon", "abandon",
                                         "abandon", "abandon", "abandon", "abandon",
                                         "abandon", "abandon", "abandon", "about"};

    // Derive Seed
    TEST_ASSERT(Crypto::BIP39_SeedFromMnemonic(mnemonic, "", out_seed), "Seed generation failed");

    Crypto::BIP32ExtendedKey master;
    TEST_ASSERT(Crypto::BIP32_MasterKeyFromSeed(out_seed, master), "Master key derivation failed");

    // Derive Bitcoin BIP84 (Native SegWit)
    std::string btc_address;
    TEST_ASSERT(Crypto::BIP84_GetAddress(master, 0, false, 0, btc_address, false),
                "BTC derivation failed");
    TEST_ASSERT(btc_address == "bc1qcr8te4kr609gcawutmrza0j4xv80jy8z306fyu",
                "BTC address mismatch");

    // Derive Ethereum EIP-1559 (BIP44/60)
    std::string eth_address;
    TEST_ASSERT(Crypto::BIP44_GetEthereumAddress(master, 0, false, 0, eth_address),
                "ETH derivation failed");
    TEST_ASSERT(eth_address == "0x9858EfFD232B4033E47d90003D41EC34EcaEda94",
                "ETH address mismatch");

    std::cout << "    BTC Address: " << btc_address << std::endl;
    std::cout << "    ETH Address: " << eth_address << std::endl;

    TEST_PASS();
}

void testCrossLibraryValidation(const std::array<uint8_t, 64>& seed) {
    TEST_START("2. Cross-Library Signature Validation");

    Crypto::BIP32ExtendedKey master;
    Crypto::BIP32_MasterKeyFromSeed(seed, master);

    // Derive BTC Private Key for m/84'/0'/0'/0/0
    Crypto::BIP32ExtendedKey btc_key;
    TEST_ASSERT(Crypto::BIP84_DeriveAddressKey(master, 0, false, 0, btc_key, false),
                "BTC key derivation failed");

    // Create a mock BTC SegWit transaction
    Crypto::BitcoinTransaction tx;
    tx.version = 2;
    tx.locktime = 0;

    Crypto::TransactionInput input;
    input.txid = "0000000000000000000000000000000000000000000000000000000000000000";
    input.vout = 0;
    input.sequence = Crypto::TransactionInput::SEQUENCE_FINAL;
    tx.inputs.push_back(input);

    Crypto::TransactionOutput output;
    output.amount = 50000;
    output.script_pubkey = "0014751e76e8199196d454941c45d1b3a323f1433bd6";
    tx.outputs.push_back(output);

    std::array<uint8_t, 32> sighash;
    std::string prev_script = "0014751e76e8199196d454941c45d1b3a323f1433bd6";
    TEST_ASSERT(Crypto::CreateSegWitSigHash(tx, 0, prev_script, 100000, sighash),
                "SigHash calculation failed");

    // Sign the transaction
    Crypto::ECDSASignature signature;
    TEST_ASSERT(Crypto::SignHash(btc_key.key, sighash, signature), "Transaction signing failed");

    // --- RAW SECP256K1 VERIFICATION (The Anchor) ---
    secp256k1_context* ctx =
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

    // Create public key struct from private key
    secp256k1_pubkey pubkey;
    TEST_ASSERT(secp256k1_ec_pubkey_create(ctx, &pubkey, btc_key.key.data()) == 1,
                "Raw pubkey creation failed");

    // Parse DER signature
    secp256k1_ecdsa_signature raw_sig;
    TEST_ASSERT(secp256k1_ecdsa_signature_parse_der(ctx, &raw_sig, signature.der_encoded.data(),
                                                    signature.der_encoded.size()) == 1,
                "Raw DER parsing failed");

    // Verify
    int verify_result = secp256k1_ecdsa_verify(ctx, &raw_sig, sighash.data(), &pubkey);
    secp256k1_context_destroy(ctx);

    TEST_ASSERT(verify_result == 1,
                "Raw secp256k1 cross-verification failed! Signature is invalid.");
    std::cout << "    Raw secp256k1 verification: SUCCESS" << std::endl;

    TEST_PASS();
}

void testStateSynchronization() {
    TEST_START("3. State Synchronization & Persistence");

    Database::DatabaseManager& dbManager = Database::DatabaseManager::getInstance();
    TestUtils::initializeTestDatabase(dbManager, TEST_DB_PATH,
                                      TestUtils::STANDARD_TEST_ENCRYPTION_KEY);

    Repository::UserRepository userRepo(dbManager);
    Repository::WalletRepository walletRepo(dbManager);
    Repository::TransactionRepository txRepo(dbManager);

    auto userResult = userRepo.createUser("sovereign_user", "SecurePass123!");
    TEST_ASSERT(userResult.hasValue(), "User creation failed");
    int userId = userResult->id;

    auto walletResult = walletRepo.createWallet(userId, "Main BTC", "bitcoin");
    TEST_ASSERT(walletResult.hasValue(), "Wallet creation failed");
    int walletId = walletResult->id;

    // Simulate transaction broadcast success and database transition
    Repository::Transaction tx;
    tx.walletId = walletId;
    tx.txid = "mock_broadcasted_txid_12345";
    tx.amountSatoshis = 50000;
    tx.feeSatoshis = 1000;
    tx.direction = "outgoing";
    tx.toAddress = "bc1qtest12345";
    tx.confirmationCount = 0;
    tx.isConfirmed = false;  // Pending state

    auto txAddResult = txRepo.addTransaction(tx);
    TEST_ASSERT(txAddResult.hasValue(), "Transaction persistence failed");

    // Simulate block arrival
    auto updateResult =
        txRepo.updateTransactionConfirmation(tx.txid, 800000, "0000000000000000000mockhash", 6);
    TEST_ASSERT(updateResult.hasValue() && *updateResult, "Confirmation update failed");

    auto retrievedTx = txRepo.getTransactionByTxid(tx.txid);
    TEST_ASSERT(retrievedTx.hasValue() && retrievedTx->isConfirmed,
                "Database state not correctly synchronized to Confirmed");

    std::cout << "    Transaction successfully persisted and transitioned to Confirmed."
              << std::endl;

    TestUtils::shutdownTestEnvironment(dbManager, TEST_DB_PATH);
    TEST_PASS();
}

void testMemorySanitization() {
    TEST_START("4. Memory Sanitization");

    // Simulate sensitive memory load
    std::vector<uint8_t> sensitive_key = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78};
    size_t original_capacity = sensitive_key.capacity();

    // Perform wipe
    Crypto::SecureWipeVector(sensitive_key);

    TEST_ASSERT(sensitive_key.empty(), "Vector size should be 0 after wipe");

    // Note: While we can't safely read freed memory, SecureWipeVector guarantees
    // the memory was overwritten before reallocation/freeing.
    std::cout << "    SecureWipeVector successfully cleared the container." << std::endl;

    // Test string wipe
    std::string sensitive_seed = "abandon abandon abandon abandon about";
    Crypto::SecureWipeString(sensitive_seed);
    TEST_ASSERT(sensitive_seed.empty(), "String should be empty after wipe");
    std::cout << "    SecureWipeString successfully cleared the string." << std::endl;

    TEST_PASS();
}

int main() {
    TestUtils::printTestHeader("The Sovereign Transaction Lifecycle Integration Test");

    std::array<uint8_t, 64> seed;
    testDeterministicKeyRecovery(seed);
    testCrossLibraryValidation(seed);
    testStateSynchronization();
    testMemorySanitization();

    TestUtils::printTestSummary("Sovereign Integration");

    return (TestGlobals::g_testsFailed == 0) ? 0 : 1;
}

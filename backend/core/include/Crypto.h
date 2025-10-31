#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Crypto {

// === Random Number Generation ===
bool RandBytes(void *buf, size_t len);

// === Base64 Encoding/Decoding ===
std::string B64Encode(const std::vector<uint8_t> &data);
std::vector<uint8_t> B64Decode(const std::string &s);

// === Hash Functions ===
bool SHA256(const uint8_t *data, size_t len, std::array<uint8_t, 32> &out);
bool RIPEMD160(const uint8_t *data, size_t len, std::array<uint8_t, 20> &out);
bool HMAC_SHA256(const std::vector<uint8_t> &key, const uint8_t *data, size_t data_len, std::vector<uint8_t> &out);
bool HMAC_SHA512(const std::vector<uint8_t> &key, const uint8_t *data, size_t data_len, std::vector<uint8_t> &out);

// === Key Derivation Functions ===
bool PBKDF2_HMAC_SHA256(const std::string &password, const uint8_t *salt, size_t salt_len, 
                        uint32_t iterations, std::vector<uint8_t> &out_key, size_t dk_len = 32);
bool PBKDF2_HMAC_SHA512(const std::string &password, const uint8_t *salt, size_t salt_len, 
                        uint32_t iterations, std::vector<uint8_t> &out_key, size_t dk_len = 64);

// === Windows DPAPI Functions ===
bool DPAPI_Protect(const std::vector<uint8_t> &plaintext, const std::string &entropy_str, std::vector<uint8_t> &ciphertext);
bool DPAPI_Unprotect(const std::vector<uint8_t> &ciphertext, const std::string &entropy_str, std::vector<uint8_t> &plaintext);

// === BIP-39 Cryptographic Functions ===
bool GenerateEntropy(size_t bits, std::vector<uint8_t> &out);
bool MnemonicFromEntropy(const std::vector<uint8_t> &entropy, const std::vector<std::string> &wordlist, std::vector<std::string> &outMnemonic);
bool ValidateMnemonic(const std::vector<std::string> &mnemonic, const std::vector<std::string> &wordlist);
bool BIP39_SeedFromMnemonic(const std::vector<std::string> &mnemonic, const std::string &passphrase, std::array<uint8_t, 64> &outSeed);

// === Utility Functions ===
bool ConstantTimeEquals(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b);

// === Memory Security Functions ===
void SecureZeroMemory(void *ptr, size_t size);
void SecureWipeVector(std::vector<uint8_t> &vec);
void SecureWipeString(std::string &str);

// === Secure Key Derivation ===
bool DeriveWalletKey(const std::string &password, const std::vector<uint8_t> &salt,
                     std::vector<uint8_t> &derived_key, size_t key_length = 32);
bool DeriveDBEncryptionKey(const std::string &password, const std::vector<uint8_t> &salt,
                          std::vector<uint8_t> &db_key);

// === AES-GCM Encryption/Decryption ===
bool AES_GCM_Encrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> &plaintext,
                     const std::vector<uint8_t> &aad, std::vector<uint8_t> &ciphertext,
                     std::vector<uint8_t> &iv, std::vector<uint8_t> &tag);
bool AES_GCM_Decrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> &ciphertext,
                     const std::vector<uint8_t> &aad, const std::vector<uint8_t> &iv,
                     const std::vector<uint8_t> &tag, std::vector<uint8_t> &plaintext);

// === Database Encryption Functions ===
bool EncryptDBData(const std::vector<uint8_t> &key, const std::vector<uint8_t> &data,
                   std::vector<uint8_t> &encrypted_blob);
bool DecryptDBData(const std::vector<uint8_t> &key, const std::vector<uint8_t> &encrypted_blob,
                   std::vector<uint8_t> &data);

// === Encrypted Seed Storage ===
struct EncryptedSeed {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> encrypted_data;
  std::vector<uint8_t> verification_hash;
};

bool EncryptSeedPhrase(const std::string &password, const std::vector<std::string> &mnemonic,
                       EncryptedSeed &encrypted_seed);
bool DecryptSeedPhrase(const std::string &password, const EncryptedSeed &encrypted_seed,
                       std::vector<std::string> &mnemonic);

// === Secure Random Salt Generation ===
bool GenerateSecureSalt(std::vector<uint8_t> &salt, size_t size = 32);

// === Database Key Management ===
struct DatabaseKeyInfo {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> key_verification_hash;
  uint32_t iteration_count;
};

bool CreateDatabaseKey(const std::string &password, DatabaseKeyInfo &key_info,
                       std::vector<uint8_t> &database_key);
bool VerifyDatabaseKey(const std::string &password, const DatabaseKeyInfo &key_info,
                       std::vector<uint8_t> &database_key);

// === BIP32 Hierarchical Deterministic Key Derivation ===
struct BIP32ExtendedKey {
  std::vector<uint8_t> chainCode;  // 32 bytes
  std::vector<uint8_t> key;         // 32 bytes private key or 33 bytes public key
  uint8_t depth;
  uint32_t fingerprint;
  uint32_t childNumber;
  bool isPrivate;
};

// Generate master extended key from BIP39 seed
bool BIP32_MasterKeyFromSeed(const std::array<uint8_t, 64> &seed,
                              BIP32ExtendedKey &masterKey);

// Derive child key from parent (hardened derivation if index >= 0x80000000)
bool BIP32_DeriveChild(const BIP32ExtendedKey &parent, uint32_t index,
                       BIP32ExtendedKey &child);

// Derive key from BIP44 path (e.g., "m/44'/0'/0'/0/0")
bool BIP32_DerivePath(const BIP32ExtendedKey &master, const std::string &path,
                      BIP32ExtendedKey &derived);

// Generate Bitcoin address from extended public key
bool BIP32_GetBitcoinAddress(const BIP32ExtendedKey &extKey, std::string &address, bool testnet = false);

// Get WIF (Wallet Import Format) private key
bool BIP32_GetWIF(const BIP32ExtendedKey &extKey, std::string &wif, bool testnet = false);

// === Transaction Signing Functions ===

// ECDSA signature structure (DER encoded)
struct ECDSASignature {
  std::vector<uint8_t> r;  // R component (32 bytes)
  std::vector<uint8_t> s;  // S component (32 bytes)
  std::vector<uint8_t> der_encoded;  // DER encoded signature for Bitcoin
};

// Sign a 32-byte hash with a private key (returns DER-encoded signature)
bool SignHash(const std::vector<uint8_t> &private_key, const std::array<uint8_t, 32> &hash,
              ECDSASignature &signature);

// Verify a signature against a public key and hash
bool VerifySignature(const std::vector<uint8_t> &public_key, const std::array<uint8_t, 32> &hash,
                     const ECDSASignature &signature);

// Derive public key from private key (compressed format, 33 bytes)
bool DerivePublicKey(const std::vector<uint8_t> &private_key, std::vector<uint8_t> &public_key);

// === Bitcoin Transaction Helper Functions ===

// UTXO (Unspent Transaction Output) structure
struct UTXO {
  std::string txid;            // Transaction ID (hex string)
  uint32_t vout;               // Output index
  uint64_t amount;             // Amount in satoshis
  std::string address;         // Address that can spend this UTXO
  std::string script_pubkey;   // Script public key (hex)
  uint32_t confirmations;      // Number of confirmations
};

// Transaction input structure
struct TransactionInput {
  std::string txid;            // Previous transaction ID
  uint32_t vout;               // Previous output index
  std::string script_sig;      // Signature script (hex)
  uint32_t sequence;           // Sequence number (0xFFFFFFFF for final)
};

// Transaction output structure
struct TransactionOutput {
  uint64_t amount;             // Amount in satoshis
  std::string script_pubkey;   // Public key script (hex)
  std::string address;         // Recipient address
};

// Bitcoin transaction structure
struct BitcoinTransaction {
  uint32_t version;                            // Transaction version (usually 1 or 2)
  std::vector<TransactionInput> inputs;        // Transaction inputs
  std::vector<TransactionOutput> outputs;      // Transaction outputs
  uint32_t locktime;                           // Lock time (0 for immediate)
  std::string raw_hex;                         // Raw transaction hex
  std::string txid;                            // Transaction ID (after signing)
};

// Create a P2PKH script pubkey from an address
bool CreateP2PKHScript(const std::string &address, std::vector<uint8_t> &script);

// Create a transaction hash for signing (single input, using SIGHASH_ALL)
bool CreateTransactionSigHash(const BitcoinTransaction &tx, size_t input_index,
                               const std::string &prev_script_pubkey,
                               std::array<uint8_t, 32> &sighash);

// Sign a Bitcoin transaction input
bool SignTransactionInput(BitcoinTransaction &tx, size_t input_index,
                          const std::vector<uint8_t> &private_key,
                          const std::vector<uint8_t> &public_key,
                          const std::string &prev_script_pubkey);

// Serialize a transaction to raw hex
bool SerializeTransaction(const BitcoinTransaction &tx, std::string &raw_hex);

// Calculate transaction ID from raw hex
bool CalculateTransactionID(const std::string &raw_hex, std::string &txid);

// === BIP44 Helper Functions ===

// BIP44 defines the hierarchical structure: m / purpose' / coin_type' / account' / change / address_index
// For Bitcoin mainnet: m/44'/0'/0'/0/0 (first receiving address)
// For Bitcoin testnet: m/44'/1'/0'/0/0

// Derive a BIP44 address key from master key
bool BIP44_DeriveAddressKey(const BIP32ExtendedKey &master, uint32_t account,
                             bool change, uint32_t address_index,
                             BIP32ExtendedKey &address_key, bool testnet = false);

// Derive Bitcoin address from BIP44 path
bool BIP44_GetAddress(const BIP32ExtendedKey &master, uint32_t account,
                      bool change, uint32_t address_index,
                      std::string &address, bool testnet = false);

// Generate multiple addresses for an account
bool BIP44_GenerateAddresses(const BIP32ExtendedKey &master, uint32_t account,
                              bool change, uint32_t start_index, uint32_t count,
                              std::vector<std::string> &addresses, bool testnet = false);

// === UTXO Management Functions ===

// UTXO set for an address
struct UTXOSet {
  std::string address;
  std::vector<UTXO> utxos;
  uint64_t total_amount;  // Sum of all UTXO amounts
  uint32_t utxo_count;
};

// Select UTXOs for a transaction (coin selection algorithm)
struct CoinSelection {
  std::vector<UTXO> selected_utxos;
  uint64_t total_input;      // Total input amount
  uint64_t target_amount;    // Amount to send
  uint64_t fee;              // Transaction fee
  uint64_t change_amount;    // Change to return
  bool has_change;           // Whether change output is needed
};

// Simple coin selection algorithm (largest first)
bool SelectCoins(const std::vector<UTXO> &available_utxos,
                 uint64_t target_amount, uint64_t fee_per_byte,
                 CoinSelection &selection);

// Estimate transaction size in bytes (for fee calculation)
uint64_t EstimateTransactionSize(size_t input_count, size_t output_count);

// Calculate total fee for a transaction
uint64_t CalculateFee(size_t input_count, size_t output_count, uint64_t fee_per_byte);

// Create a complete unsigned transaction from UTXOs
bool CreateUnsignedTransaction(const std::vector<UTXO> &inputs,
                                const std::string &recipient_address,
                                uint64_t send_amount,
                                const std::string &change_address,
                                uint64_t change_amount,
                                BitcoinTransaction &tx);

} // namespace Crypto

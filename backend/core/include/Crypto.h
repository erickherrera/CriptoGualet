#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Crypto {

// === Random Number Generation ===
bool RandBytes(void *buf, size_t len);
std::string GenerateSecureRandomString(size_t byteLength);

// === Base64 Encoding/Decoding ===
std::string B64Encode(const std::vector<uint8_t> &data);
std::vector<uint8_t> B64Decode(const std::string &s);

// === Base58 Encoding/Decoding ===
std::string EncodeBase58(const std::vector<uint8_t> &data);
std::vector<uint8_t> DecodeBase58(const std::string &str);
std::string EncodeBase58Check(const std::vector<uint8_t> &data);
bool DecodeBase58Check(const std::string &address, std::vector<uint8_t> &payload);

// === Hash Functions ===
bool SHA256(const uint8_t *data, size_t len, std::array<uint8_t, 32> &out);
bool RIPEMD160(const uint8_t *data, size_t len, std::array<uint8_t, 20> &out);
bool Keccak256(const uint8_t *data, size_t len, std::array<uint8_t, 32> &out);
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
bool LoadBIP39Wordlist(std::vector<std::string> &wordlist);
bool GenerateEntropy(size_t bits, std::vector<uint8_t> &out);
bool MnemonicFromEntropy(const std::vector<uint8_t> &entropy, const std::vector<std::string> &wordlist, std::vector<std::string> &outMnemonic);
bool ValidateMnemonic(const std::vector<std::string> &mnemonic, const std::vector<std::string> &wordlist);
bool BIP39_SeedFromMnemonic(const std::vector<std::string> &mnemonic, const std::string &passphrase, std::array<uint8_t, 64> &outSeed);

// === Utility Functions ===
bool ConstantTimeEquals(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b);

// === Memory Security Functions ===
void SecureClear(void *ptr, size_t size);
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

// Import extended key from Base58Check string (XPUB/XPRV)
bool ImportExtendedKey(const std::string &encoded, BIP32ExtendedKey &outKey);

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

// ECDSA signature with recovery ID (for Ethereum)
struct RecoverableSignature {
  std::vector<uint8_t> r;  // R component (32 bytes)
  std::vector<uint8_t> s;  // S component (32 bytes)
  int recovery_id;         // Recovery ID (0-3, usually 0 or 1)
};

// Sign a 32-byte hash with a private key (returns DER-encoded signature)
bool SignHash(const std::vector<uint8_t> &private_key, const std::array<uint8_t, 32> &hash,
              ECDSASignature &signature);

// Sign a 32-byte hash with a private key (returns recoverable signature for Ethereum)
bool SignHashRecoverable(const std::vector<uint8_t> &private_key, const std::array<uint8_t, 32> &hash,
                         RecoverableSignature &signature);

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

// === Ethereum Functions ===

// === EIP-55 Checksum Functions ===

// Convert Ethereum address to EIP-55 checksum format
// Input: "0x..." or raw hex address (with or without 0x prefix)
// Output: EIP-55 checksummed address with 0x prefix
bool EIP55_ToChecksumAddress(const std::string &address, std::string &checksummed);

// Validate that an Ethereum address has correct EIP-55 checksum
// Returns true if address has valid checksum or is all lowercase/uppercase
// Returns false if checksum is invalid
bool EIP55_ValidateChecksumAddress(const std::string &address);

// Generate Ethereum address from extended public key
// Ethereum addresses are 20 bytes (40 hex chars) with 0x prefix
// Format: 0x + last 20 bytes of Keccak256(public_key)
// Returns address in EIP-55 checksum format
bool BIP32_GetEthereumAddress(const BIP32ExtendedKey &extKey, std::string &address);

// Derive Ethereum address key from BIP44 path
// Ethereum BIP44 path: m/44'/60'/0'/0/0 (coin_type = 60)
bool BIP44_DeriveEthereumAddressKey(const BIP32ExtendedKey &master, uint32_t account,
                                     bool change, uint32_t address_index,
                                     BIP32ExtendedKey &address_key);

// Derive Ethereum address from BIP44 path
bool BIP44_GetEthereumAddress(const BIP32ExtendedKey &master, uint32_t account,
                               bool change, uint32_t address_index,
                               std::string &address);

// Generate multiple Ethereum addresses for an account
bool BIP44_GenerateEthereumAddresses(const BIP32ExtendedKey &master, uint32_t account,
                                      bool change, uint32_t start_index, uint32_t count,
                                      std::vector<std::string> &addresses);

// === Multi-Chain Helper Functions ===

// Chain type enumeration
enum class ChainType {
  BITCOIN,
  BITCOIN_TESTNET,
  LITECOIN,
  LITECOIN_TESTNET,
  ETHEREUM,
  ETHEREUM_TESTNET,  // Sepolia, Goerli
  BNB_CHAIN,
  POLYGON,
  AVALANCHE,
  ARBITRUM,
  OPTIMISM,
  BASE
};

// Get BIP44 coin type for a chain
uint32_t GetCoinType(ChainType chain);

// Get chain name as string
std::string GetChainName(ChainType chain);

// Derive address for any supported chain
bool DeriveChainAddress(const BIP32ExtendedKey &master, ChainType chain,
                         uint32_t account, bool change, uint32_t address_index,
                         std::string &address);

// === Chain-Aware Address Validation ===

// Check if an address format is valid for a specific chain
bool IsValidAddressFormat(const std::string &address, ChainType chain);

// Try to detect which chain type an address belongs to
// Returns std::nullopt if address format cannot be determined
std::optional<ChainType> DetectAddressChain(const std::string &address);

// Check if a chain uses EVM-compatible addresses
bool IsEVMChain(ChainType chain);

// Check if a chain uses Bitcoin-style addresses
bool IsBitcoinChain(ChainType chain);

// === TOTP (Time-based One-Time Password) Functions ===
// Compatible with Google Authenticator, Authy, Microsoft Authenticator, etc.

// HMAC-SHA1 for TOTP (RFC 4226/6238 requires SHA1)
bool HMAC_SHA1(const std::vector<uint8_t> &key, const uint8_t *data, size_t data_len, 
               std::vector<uint8_t> &out);

// Generate a random TOTP secret (20 bytes = 160 bits, standard for most authenticators)
bool GenerateTOTPSecret(std::vector<uint8_t> &secret, size_t length = 20);

// Base32 encoding/decoding for TOTP secrets
std::string Base32Encode(const std::vector<uint8_t> &data);
std::vector<uint8_t> Base32Decode(const std::string &encoded);

// Generate TOTP code for the current time (or specified timestamp)
// Returns 6-digit code as string (with leading zeros if needed)
// timestep: number of seconds per code (default 30 for most apps)
// digits: number of digits in code (default 6)
std::string GenerateTOTP(const std::vector<uint8_t> &secret, 
                         uint64_t timestamp = 0,  // 0 = use current time
                         uint32_t timestep = 30,
                         uint32_t digits = 6);

// Verify a TOTP code with time drift tolerance
// window: number of time periods to check before/after current (default 1 = ±30 seconds)
bool VerifyTOTP(const std::vector<uint8_t> &secret,
                const std::string &code,
                uint32_t window = 1,
                uint32_t timestep = 30,
                uint32_t digits = 6);

// Generate otpauth:// URI for QR code generation
// Format: otpauth://totp/Label?secret=BASE32SECRET&issuer=Issuer
std::string GenerateTOTPUri(const std::string &secret_base32,
                            const std::string &account_name,
                            const std::string &issuer = "CriptoGualet");

} // namespace Crypto

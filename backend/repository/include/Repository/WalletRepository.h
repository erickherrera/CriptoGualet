#pragma once

#include "Repository/RepositoryTypes.h"
#include "Repository/Logger.h"
#include "Database/DatabaseManager.h"
#include "Crypto.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>

// Forward declaration
struct sqlite3_stmt;

namespace Repository {

// Forward declarations
struct WalletSummary;
struct WalletStats;

/**
 * @brief Repository for wallet-related database operations
 *
 * Provides comprehensive wallet management including:
 * - Wallet creation and management
 * - Address generation and tracking
 * - Balance calculation and caching
 * - Encrypted seed storage and retrieval
 * - Multi-currency support
 */
class WalletRepository {
public:
    /**
     * @brief Constructor
     * @param dbManager Reference to the database manager
     */
    explicit WalletRepository(Database::DatabaseManager& dbManager);

    /**
     * @brief Create a new wallet for a user
     * @param userId User ID who owns the wallet
     * @param walletName Name of the wallet
     * @param walletType Type of wallet (bitcoin, litecoin, etc.)
     * @param derivationPath Optional BIP44 derivation path
     * @param extendedPublicKey Optional master public key
     * @return Result containing the created wallet or error information
     */
    Result<Wallet> createWallet(int userId, const std::string& walletName,
                               const std::string& walletType = "bitcoin",
                               const std::optional<std::string>& derivationPath = std::nullopt,
                               const std::optional<std::string>& extendedPublicKey = std::nullopt);

    /**
     * @brief Get wallet by ID
     * @param walletId Wallet ID to retrieve
     * @return Result containing the wallet or error information
     */
    Result<Wallet> getWalletById(int walletId);

    /**
     * @brief Get all wallets for a user
     * @param userId User ID
     * @param includeInactive Whether to include inactive wallets
     * @return Result containing list of wallets
     */
    Result<std::vector<Wallet>> getWalletsByUserId(int userId, bool includeInactive = false);

    /**
     * @brief PHASE 3: Get wallets by type for a user
     * @param userId User ID
     * @param walletType Wallet type (bitcoin, ethereum, etc.)
     * @param includeInactive Whether to include inactive wallets
     * @return Result containing list of wallets matching the type
     */
    Result<std::vector<Wallet>> getWalletsByType(int userId, const std::string& walletType,
                                                 bool includeInactive = false);

    /**
     * @brief Get wallet by name for a specific user
     * @param userId User ID
     * @param walletName Wallet name
     * @return Result containing the wallet or error information
     */
    Result<Wallet> getWalletByName(int userId, const std::string& walletName);

    /**
     * @brief Update wallet information
     * @param walletId Wallet ID to update
     * @param walletName New wallet name (optional)
     * @param derivationPath New derivation path (optional)
     * @param extendedPublicKey New extended public key (optional)
     * @return Result indicating success or failure
     */
    Result<bool> updateWallet(int walletId,
                             const std::optional<std::string>& walletName = std::nullopt,
                             const std::optional<std::string>& derivationPath = std::nullopt,
                             const std::optional<std::string>& extendedPublicKey = std::nullopt);

    /**
     * @brief Set wallet active status
     * @param walletId Wallet ID
     * @param isActive New active status
     * @return Result indicating success or failure
     */
    Result<bool> setWalletActive(int walletId, bool isActive);

    /**
     * @brief Delete wallet (soft delete - marks as inactive)
     * @param walletId Wallet ID to delete
     * @return Result indicating success or failure
     */
    Result<bool> deleteWallet(int walletId);

    /**
     * @brief Generate and store a new address for a wallet
     * @param walletId Wallet ID
     * @param isChange Whether this is a change address (false = receiving)
     * @param label Optional label for the address
     * @return Result containing the generated address
     */
    Result<Address> generateAddress(int walletId, bool isChange = false,
                                   const std::optional<std::string>& label = std::nullopt);

    /**
     * @brief Get address by address string
     * @param addressStr Address string to search for
     * @return Result containing the address or error information
     */
    Result<Address> getAddressByString(const std::string& addressStr);

    /**
     * @brief Get all addresses for a wallet
     * @param walletId Wallet ID
     * @param isChange Optional filter for address type (nullopt = all)
     * @return Result containing list of addresses
     */
    Result<std::vector<Address>> getAddressesByWallet(int walletId,
                                                     const std::optional<bool>& isChange = std::nullopt);

    /**
     * @brief Update address label
     * @param addressId Address ID
     * @param label New label for the address
     * @return Result indicating success or failure
     */
    Result<bool> updateAddressLabel(int addressId, const std::string& label);

    /**
     * @brief Update address balance (called when transactions are processed)
     * @param addressId Address ID
     * @param balanceSatoshis New balance in satoshis
     * @return Result indicating success or failure
     */
    Result<bool> updateAddressBalance(int addressId, int64_t balanceSatoshis);

    /**
     * @brief Get wallet summary with balance information
     * @param walletId Wallet ID
     * @return Result containing wallet summary
     */
    Result<WalletSummary> getWalletSummary(int walletId);

    /**
     * @brief Get next available address index for a wallet
     * @param walletId Wallet ID
     * @param isChange Whether to get next change address index
     * @return Result containing the next address index
     */
    Result<int> getNextAddressIndex(int walletId, bool isChange);

    /**
     * @brief Store encrypted seed for a user
     * @param userId User ID
     * @param password User password for encryption
     * @param mnemonic BIP39 mnemonic words
     * @return Result indicating success or failure
     */
    Result<bool> storeEncryptedSeed(int userId, const std::string& password,
                                   const std::vector<std::string>& mnemonic);

    /**
     * @brief Retrieve and decrypt seed for a user
     * @param userId User ID
     * @param password User password for decryption
     * @return Result containing the decrypted mnemonic words
     */
    Result<std::vector<std::string>> retrieveDecryptedSeed(int userId, const std::string& password);

    /**
     * @brief Mark seed backup as confirmed
     * @param userId User ID
     * @return Result indicating success or failure
     */
    Result<bool> confirmSeedBackup(int userId);

    /**
     * @brief Check if user has a seed stored
     * @param userId User ID
     * @return Result indicating if seed exists
     */
    Result<bool> hasSeedStored(int userId);

    /**
     * @brief Get wallet statistics for a user
     * @param userId User ID
     * @return Result containing wallet statistics
     */
    Result<WalletStats> getWalletStats(int userId);

    // === UTXO Management Functions ===

    /**
     * @brief Add a new UTXO to the database
     * @param walletId Wallet ID
     * @param addressId Address ID
     * @param txid Transaction ID
     * @param vout Output index
     * @param amountSatoshis Amount in satoshis
     * @param scriptPubKey Script public key (hex)
     * @param confirmations Number of confirmations
     * @param blockHeight Optional block height
     * @return Result containing the created UTXO
     */
    Result<UTXO> addUTXO(int walletId, int addressId, const std::string& txid,
                        uint32_t vout, int64_t amountSatoshis,
                        const std::string& scriptPubKey, uint32_t confirmations,
                        const std::optional<int>& blockHeight = std::nullopt);

    /**
     * @brief Get all unspent UTXOs for a wallet
     * @param walletId Wallet ID
     * @param minConfirmations Minimum number of confirmations (default: 1)
     * @return Result containing list of unspent UTXOs
     */
    Result<std::vector<UTXO>> getUnspentUTXOs(int walletId, uint32_t minConfirmations = 1);

    /**
     * @brief Get all unspent UTXOs for a specific address
     * @param addressId Address ID
     * @param minConfirmations Minimum number of confirmations (default: 1)
     * @return Result containing list of unspent UTXOs
     */
    Result<std::vector<UTXO>> getUnspentUTXOsByAddress(int addressId, uint32_t minConfirmations = 1);

    /**
     * @brief Mark UTXO as spent
     * @param utxoId UTXO ID
     * @param spentInTxid Transaction ID where it was spent
     * @return Result indicating success or failure
     */
    Result<bool> markUTXOAsSpent(int utxoId, const std::string& spentInTxid);

    /**
     * @brief Mark multiple UTXOs as spent
     * @param utxoIds List of UTXO IDs
     * @param spentInTxid Transaction ID where they were spent
     * @return Result indicating success or failure
     */
    Result<bool> markUTXOsAsSpent(const std::vector<int>& utxoIds, const std::string& spentInTxid);

    /**
     * @brief Update UTXO confirmation count
     * @param txid Transaction ID
     * @param confirmations New confirmation count
     * @return Result indicating success or failure
     */
    Result<bool> updateUTXOConfirmations(const std::string& txid, uint32_t confirmations);

    /**
     * @brief Get total spendable balance for a wallet
     * @param walletId Wallet ID
     * @param minConfirmations Minimum number of confirmations (default: 1)
     * @return Result containing total spendable balance in satoshis
     */
    Result<int64_t> getSpendableBalance(int walletId, uint32_t minConfirmations = 1);

    /**
     * @brief Check if a UTXO exists (to avoid duplicates)
     * @param txid Transaction ID
     * @param vout Output index
     * @return Result indicating if UTXO exists
     */
    Result<bool> utxoExists(const std::string& txid, uint32_t vout);

    /**
     * @brief Get UTXO by transaction ID and output index
     * @param txid Transaction ID
     * @param vout Output index
     * @return Result containing the UTXO
     */
    Result<UTXO> getUTXOByTxidVout(const std::string& txid, uint32_t vout);

private:
    /**
     * @brief Convert database row to Wallet object
     * @param stmt SQLite statement handle
     * @return Wallet object populated from database row
     */
    Wallet mapRowToWallet(sqlite3_stmt* stmt);

    /**
     * @brief Convert database row to Address object
     * @param stmt SQLite statement handle
     * @return Address object populated from database row
     */
    Address mapRowToAddress(sqlite3_stmt* stmt);

    /**
     * @brief Convert database row to UTXO object
     * @param stmt SQLite statement handle
     * @return UTXO object populated from database row
     */
    UTXO mapRowToUTXO(sqlite3_stmt* stmt);

    /**
     * @brief Validate wallet name format and requirements
     * @param walletName Wallet name to validate
     * @return Result indicating if wallet name is valid
     */
    Result<bool> validateWalletName(const std::string& walletName);

    /**
     * @brief Validate wallet type
     * @param walletType Wallet type to validate
     * @return Result indicating if wallet type is valid
     */
    Result<bool> validateWalletType(const std::string& walletType);

    /**
     * @brief Generate a Bitcoin address (simplified implementation)
     * @param walletId Wallet ID
     * @param addressIndex Address index
     * @param isChange Whether this is a change address
     * @return Generated address string
     */
    std::string generateBitcoinAddress(int walletId, int addressIndex, bool isChange);

    /**
     * @brief Calculate total wallet balance from addresses
     * @param walletId Wallet ID
     * @return Total balance in satoshis
     */
    int64_t calculateWalletBalance(int walletId);

private:
    Database::DatabaseManager& m_dbManager;
    static constexpr const char* COMPONENT_NAME = "WalletRepository";
    static constexpr int MIN_WALLET_NAME_LENGTH = 1;
    static constexpr int MAX_WALLET_NAME_LENGTH = 100;
    static constexpr int MAX_ADDRESS_GAP = 20; // BIP44 address gap limit
};

/**
 * @brief Wallet summary with balance and statistics
 */
struct WalletSummary {
    Wallet wallet;
    int64_t totalBalanceSatoshis;
    int addressCount;
    int usedAddressCount;
    int transactionCount;
    std::optional<std::chrono::system_clock::time_point> lastTransactionDate;

    WalletSummary() : totalBalanceSatoshis(0), addressCount(0), usedAddressCount(0), transactionCount(0) {}
};

/**
 * @brief Wallet statistics for a user
 */
struct WalletStats {
    int totalWallets;
    int activeWallets;
    int64_t totalBalanceSatoshis;
    std::vector<std::pair<std::string, int>> walletsByType; // wallet_type, count
    bool hasSeedBackup;

    WalletStats() : totalWallets(0), activeWallets(0), totalBalanceSatoshis(0), hasSeedBackup(false) {}
};

} // namespace Repository

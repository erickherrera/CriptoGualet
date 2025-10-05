#pragma once

#include "RepositoryTypes.h"
#include "Logger.h"
#include "../Database/DatabaseManager.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>

// Forward declaration
struct sqlite3_stmt;

namespace Repository {

// Forward declarations for structs defined at the end of this file
struct TransactionInput;
struct TransactionOutput;
struct TransactionStats;
struct WalletBalance;
struct UTXO;
struct TransactionSearchCriteria;

/**
 * @brief Repository for transaction-related database operations
 *
 * Provides comprehensive transaction management including:
 * - Transaction storage and retrieval
 * - Transaction input/output management
 * - Balance calculation and updates
 * - Transaction history with filtering and pagination
 * - Confirmation tracking
 */
class TransactionRepository {
public:
    /**
     * @brief Constructor
     * @param dbManager Reference to the database manager
     */
    explicit TransactionRepository(Database::DatabaseManager& dbManager);

    /**
     * @brief Add a new transaction to the database
     * @param transaction Transaction data to store
     * @return Result containing the created transaction with assigned ID
     */
    Result<Transaction> addTransaction(const Transaction& transaction);

    /**
     * @brief Get transaction by transaction ID (txid)
     * @param txid Transaction ID to search for
     * @return Result containing the transaction or error information
     */
    Result<Transaction> getTransactionByTxid(const std::string& txid);

    /**
     * @brief Get transaction by database ID
     * @param transactionId Database transaction ID
     * @return Result containing the transaction or error information
     */
    Result<Transaction> getTransactionById(int transactionId);

    /**
     * @brief Get all transactions for a wallet
     * @param walletId Wallet ID
     * @param params Pagination parameters
     * @param direction Optional filter by direction (incoming/outgoing/internal)
     * @param confirmedOnly Whether to return only confirmed transactions
     * @return Result containing paginated transaction list
     */
    Result<PaginatedResult<Transaction>> getTransactionsByWallet(
        int walletId,
        const PaginationParams& params = PaginationParams(),
        const std::optional<std::string>& direction = std::nullopt,
        bool confirmedOnly = false);

    /**
     * @brief Get transactions for a specific address
     * @param address Address string to search for
     * @param params Pagination parameters
     * @return Result containing paginated transaction list
     */
    Result<PaginatedResult<Transaction>> getTransactionsByAddress(
        const std::string& address,
        const PaginationParams& params = PaginationParams());

    /**
     * @brief Update transaction confirmation status
     * @param txid Transaction ID
     * @param blockHeight Block height where transaction was confirmed
     * @param blockHash Block hash
     * @param confirmationCount Number of confirmations
     * @return Result indicating success or failure
     */
    Result<bool> updateTransactionConfirmation(const std::string& txid,
                                              int blockHeight,
                                              const std::string& blockHash,
                                              int confirmationCount);

    /**
     * @brief Mark transaction as confirmed
     * @param txid Transaction ID
     * @param confirmedAt Confirmation timestamp
     * @return Result indicating success or failure
     */
    Result<bool> confirmTransaction(const std::string& txid,
                                   const std::optional<std::chrono::system_clock::time_point>& confirmedAt = std::nullopt);

    /**
     * @brief Update transaction memo/note
     * @param transactionId Database transaction ID
     * @param memo New memo text
     * @return Result indicating success or failure
     */
    Result<bool> updateTransactionMemo(int transactionId, const std::string& memo);

    /**
     * @brief Get transaction statistics for a wallet
     * @param walletId Wallet ID
     * @return Result containing transaction statistics
     */
    Result<TransactionStats> getTransactionStats(int walletId);

    /**
     * @brief Get recent transactions across all wallets for a user
     * @param userId User ID
     * @param limit Maximum number of transactions to return
     * @return Result containing recent transactions
     */
    Result<std::vector<Transaction>> getRecentTransactionsForUser(int userId, int limit = 10);

    /**
     * @brief Calculate total balance for a wallet based on transactions
     * @param walletId Wallet ID
     * @return Result containing balance information
     */
    Result<WalletBalance> calculateWalletBalance(int walletId);

    /**
     * @brief Get pending (unconfirmed) transactions for a wallet
     * @param walletId Wallet ID
     * @return Result containing list of pending transactions
     */
    Result<std::vector<Transaction>> getPendingTransactions(int walletId);

    /**
     * @brief Search transactions by various criteria
     * @param searchCriteria Search parameters
     * @param params Pagination parameters
     * @return Result containing paginated search results
     */
    Result<PaginatedResult<Transaction>> searchTransactions(
        const TransactionSearchCriteria& searchCriteria,
        const PaginationParams& params = PaginationParams());

    /**
     * @brief Add transaction input details
     * @param transactionId Database transaction ID
     * @param inputs Vector of transaction inputs
     * @return Result indicating success or failure
     */
    Result<bool> addTransactionInputs(int transactionId, const std::vector<TransactionInput>& inputs);

    /**
     * @brief Add transaction output details
     * @param transactionId Database transaction ID
     * @param outputs Vector of transaction outputs
     * @return Result indicating success or failure
     */
    Result<bool> addTransactionOutputs(int transactionId, const std::vector<TransactionOutput>& outputs);

    /**
     * @brief Get transaction inputs
     * @param transactionId Database transaction ID
     * @return Result containing transaction inputs
     */
    Result<std::vector<TransactionInput>> getTransactionInputs(int transactionId);

    /**
     * @brief Get transaction outputs
     * @param transactionId Database transaction ID
     * @return Result containing transaction outputs
     */
    Result<std::vector<TransactionOutput>> getTransactionOutputs(int transactionId);

    /**
     * @brief Mark transaction output as spent
     * @param transactionId Database transaction ID
     * @param outputIndex Output index within the transaction
     * @param spentInTxid Transaction ID where this output was spent
     * @return Result indicating success or failure
     */
    Result<bool> markOutputAsSpent(int transactionId, int outputIndex, const std::string& spentInTxid);

    /**
     * @brief Get unspent transaction outputs (UTXOs) for a wallet
     * @param walletId Wallet ID
     * @param minAmount Minimum amount in satoshis (optional)
     * @return Result containing list of UTXOs
     */
    Result<std::vector<UTXO>> getUTXOs(int walletId, const std::optional<int64_t>& minAmount = std::nullopt);

    /**
     * @brief Delete old transactions (for cleanup)
     * @param walletId Wallet ID
     * @param olderThan Delete transactions older than this timestamp
     * @param keepCount Minimum number of recent transactions to keep
     * @return Result indicating number of deleted transactions
     */
    Result<int> deleteOldTransactions(int walletId,
                                     const std::chrono::system_clock::time_point& olderThan,
                                     int keepCount = 100);

private:
    /**
     * @brief Convert database row to Transaction object
     * @param stmt SQLite statement handle
     * @return Transaction object populated from database row
     */
    Transaction mapRowToTransaction(sqlite3_stmt* stmt);

    /**
     * @brief Convert database row to TransactionInput object
     * @param stmt SQLite statement handle
     * @return TransactionInput object populated from database row
     */
    TransactionInput mapRowToTransactionInput(sqlite3_stmt* stmt);

    /**
     * @brief Convert database row to TransactionOutput object
     * @param stmt SQLite statement handle
     * @return TransactionOutput object populated from database row
     */
    TransactionOutput mapRowToTransactionOutput(sqlite3_stmt* stmt);

    /**
     * @brief Validate transaction data
     * @param transaction Transaction to validate
     * @return Result indicating if transaction is valid
     */
    Result<bool> validateTransaction(const Transaction& transaction);

    /**
     * @brief Update wallet address balances based on transaction
     * @param transaction Transaction data
     * @return Result indicating success or failure
     */
    Result<bool> updateAddressBalances(const Transaction& transaction);

    /**
     * @brief Build WHERE clause for transaction search
     * @param criteria Search criteria
     * @param params Vector to store bind parameters
     * @return SQL WHERE clause string
     */
    std::string buildSearchWhereClause(const TransactionSearchCriteria& criteria,
                                      std::vector<std::string>& params);

private:
    Database::DatabaseManager& m_dbManager;
    static constexpr const char* COMPONENT_NAME = "TransactionRepository";
    static constexpr int MIN_CONFIRMATIONS_FOR_CONFIRMED = 6;
};

/**
 * @brief Transaction input details
 */
struct TransactionInput {
    int id;
    int transactionId;
    int inputIndex;
    std::string prevTxid;
    int prevOutputIndex;
    std::optional<std::string> scriptSig;
    uint32_t sequence;
    std::optional<std::string> address;
    std::optional<int64_t> amountSatoshis;

    TransactionInput() : id(0), transactionId(0), inputIndex(0), prevOutputIndex(0), sequence(0xFFFFFFFF) {}
};

/**
 * @brief Transaction output details
 */
struct TransactionOutput {
    int id;
    int transactionId;
    int outputIndex;
    std::optional<std::string> scriptPubkey;
    std::optional<std::string> address;
    int64_t amountSatoshis;
    bool isSpent;
    std::optional<std::string> spentInTxid;

    TransactionOutput() : id(0), transactionId(0), outputIndex(0), amountSatoshis(0), isSpent(false) {}
};

/**
 * @brief Transaction statistics for a wallet
 */
struct TransactionStats {
    int totalTransactions;
    int confirmedTransactions;
    int pendingTransactions;
    int64_t totalReceived;
    int64_t totalSent;
    int64_t totalFees;
    std::optional<std::chrono::system_clock::time_point> firstTransaction;
    std::optional<std::chrono::system_clock::time_point> lastTransaction;

    TransactionStats() : totalTransactions(0), confirmedTransactions(0), pendingTransactions(0),
                        totalReceived(0), totalSent(0), totalFees(0) {}
};

/**
 * @brief Wallet balance information
 */
struct WalletBalance {
    int64_t confirmedBalance;
    int64_t unconfirmedBalance;
    int64_t totalBalance;
    int utxoCount;

    WalletBalance() : confirmedBalance(0), unconfirmedBalance(0), totalBalance(0), utxoCount(0) {}
};

/**
 * @brief Unspent Transaction Output (UTXO)
 */
struct UTXO {
    int transactionId;
    int outputIndex;
    std::string txid;
    std::string address;
    int64_t amountSatoshis;
    int confirmationCount;
    bool isConfirmed;

    UTXO() : transactionId(0), outputIndex(0), amountSatoshis(0), confirmationCount(0), isConfirmed(false) {}
};

/**
 * @brief Transaction search criteria
 */
struct TransactionSearchCriteria {
    std::optional<int> walletId;
    std::optional<std::string> direction;
    std::optional<std::string> address;
    std::optional<int64_t> minAmount;
    std::optional<int64_t> maxAmount;
    std::optional<std::chrono::system_clock::time_point> fromDate;
    std::optional<std::chrono::system_clock::time_point> toDate;
    std::optional<bool> confirmedOnly;
    std::optional<std::string> memo;

    TransactionSearchCriteria() = default;
};

} // namespace Repository

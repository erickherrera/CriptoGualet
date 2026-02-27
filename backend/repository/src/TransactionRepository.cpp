#include "../include/Repository/TransactionRepository.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

namespace Repository {

TransactionRepository::TransactionRepository(Database::DatabaseManager& dbManager) : m_dbManager(dbManager) {
    REPO_LOG_INFO(COMPONENT_NAME, "TransactionRepository initialized");
}

Result<Transaction> TransactionRepository::addTransaction(const Transaction& transaction) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "addTransaction");

    // Validate transaction data
    auto validation = validateTransaction(transaction);
    if (!validation) {
        return Result<Transaction>(validation.error(), validation.errorCode);
    }

    // Begin transaction
    auto dbTransaction = m_dbManager.beginTransaction();
    if (!dbTransaction) {
        REPO_LOG_ERROR(COMPONENT_NAME, "Failed to begin transaction", dbTransaction.message);
        return Result<Transaction>("Database transaction error", 500);
    }

    try {
        // Insert transaction into database
        const std::string sql = R"(
            INSERT INTO transactions
            (wallet_id, txid, block_height, block_hash, amount_satoshis, fee_satoshis,
             direction, from_address, to_address, confirmation_count, is_confirmed, created_at, memo)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP, ?)
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            m_dbManager.rollbackTransaction();
            return Result<Transaction>("Failed to prepare transaction insertion statement", 500);
        }

        // Bind parameters
        sqlite3_bind_int(stmt, 1, transaction.walletId);
        sqlite3_bind_text(stmt, 2, transaction.txid.c_str(), -1, SQLITE_STATIC);

        if (transaction.blockHeight.has_value()) {
            sqlite3_bind_int(stmt, 3, *transaction.blockHeight);
        } else {
            sqlite3_bind_null(stmt, 3);
        }

        if (transaction.blockHash.has_value()) {
            sqlite3_bind_text(stmt, 4, transaction.blockHash->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 4);
        }

        sqlite3_bind_int64(stmt, 5, transaction.amountSatoshis);
        sqlite3_bind_int64(stmt, 6, transaction.feeSatoshis);
        sqlite3_bind_text(stmt, 7, transaction.direction.c_str(), -1, SQLITE_STATIC);

        if (transaction.fromAddress.has_value()) {
            sqlite3_bind_text(stmt, 8, transaction.fromAddress->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 8);
        }

        if (transaction.toAddress.has_value()) {
            sqlite3_bind_text(stmt, 9, transaction.toAddress->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 9);
        }

        sqlite3_bind_int(stmt, 10, transaction.confirmationCount);
        sqlite3_bind_int(stmt, 11, transaction.isConfirmed ? 1 : 0);

        if (transaction.memo.has_value()) {
            sqlite3_bind_text(stmt, 12, transaction.memo->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 12);
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            m_dbManager.rollbackTransaction();
            return Result<Transaction>("Failed to insert transaction", 500);
        }

        int transactionId = static_cast<int>(sqlite3_last_insert_rowid(m_dbManager.getHandle()));
        sqlite3_finalize(stmt);

        // Update address balances
        auto balanceUpdateResult = updateAddressBalances(transaction);
        if (!balanceUpdateResult) {
            m_dbManager.rollbackTransaction();
            REPO_LOG_ERROR(COMPONENT_NAME, "Failed to update address balances", balanceUpdateResult.error());
            return Result<Transaction>("Failed to update address balances", 500);
        }

        // Commit transaction
        auto commitResult = m_dbManager.commitTransaction();
        if (!commitResult) {
            REPO_LOG_ERROR(COMPONENT_NAME, "Failed to commit transaction", commitResult.message);
            return Result<Transaction>("Failed to commit transaction", 500);
        }

        // Retrieve and return created transaction
        auto transactionResult = getTransactionById(transactionId);
        if (transactionResult) {
            REPO_LOG_INFO(COMPONENT_NAME, "Transaction added successfully",
                         "TXID: " + transaction.txid + ", Amount: " + std::to_string(transaction.amountSatoshis));
        }

        return transactionResult;

    } catch (const std::exception& e) {
        m_dbManager.rollbackTransaction();
        REPO_LOG_ERROR(COMPONENT_NAME, "Exception during transaction creation", e.what());
        return Result<Transaction>("Internal error during transaction creation", 500);
    }
}

Result<Transaction> TransactionRepository::getTransactionByTxid(const std::string& txid) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getTransactionByTxid");

    const std::string sql = R"(
        SELECT id, wallet_id, txid, block_height, block_hash, amount_satoshis, fee_satoshis,
               direction, from_address, to_address, confirmation_count, is_confirmed, created_at, confirmed_at, memo
        FROM transactions
        WHERE txid = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Transaction>("Failed to prepare transaction query", 500);
    }

    sqlite3_bind_text(stmt, 1, txid.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        Transaction transaction = mapRowToTransaction(stmt);
        sqlite3_finalize(stmt);
        return Result<Transaction>(transaction);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<Transaction>("Transaction not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<Transaction>("Database error while retrieving transaction", 500);
    }
}

Result<Transaction> TransactionRepository::getTransactionById(int transactionId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getTransactionById");

    const std::string sql = R"(
        SELECT id, wallet_id, txid, block_height, block_hash, amount_satoshis, fee_satoshis,
               direction, from_address, to_address, confirmation_count, is_confirmed, created_at, confirmed_at, memo
        FROM transactions
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Transaction>("Failed to prepare transaction query", 500);
    }

    sqlite3_bind_int(stmt, 1, transactionId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        Transaction transaction = mapRowToTransaction(stmt);
        sqlite3_finalize(stmt);
        return Result<Transaction>(transaction);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<Transaction>("Transaction not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<Transaction>("Database error while retrieving transaction", 500);
    }
}

Result<PaginatedResult<Transaction>> TransactionRepository::getTransactionsByWallet(
    int walletId,
    const PaginationParams& params,
    const std::optional<std::string>& direction,
    bool confirmedOnly) {

    REPO_SCOPED_LOG(COMPONENT_NAME, "getTransactionsByWallet");

    // Build the base query
    std::string sql = R"(
        SELECT id, wallet_id, txid, block_height, block_hash, amount_satoshis, fee_satoshis,
               direction, from_address, to_address, confirmation_count, is_confirmed, created_at, confirmed_at, memo
        FROM transactions
        WHERE wallet_id = ?
    )";

    std::vector<std::string> bindParams;
    bindParams.push_back(std::to_string(walletId));

    if (direction.has_value()) {
        sql += " AND direction = ?";
        bindParams.push_back(*direction);
    }

    if (confirmedOnly) {
        sql += " AND is_confirmed = 1";
    }

    // Add ordering
    sql += " ORDER BY " + params.sortField;
    if (!params.ascending) {
        sql += " DESC";
    }

    // Add pagination
    sql += " LIMIT ? OFFSET ?";
    bindParams.push_back(std::to_string(params.limit));
    bindParams.push_back(std::to_string(params.offset));

    // Get total count
    std::string countSql = "SELECT COUNT(*) FROM transactions WHERE wallet_id = ?";
    if (direction.has_value()) {
        countSql += " AND direction = ?";
    }
    if (confirmedOnly) {
        countSql += " AND is_confirmed = 1";
    }

    sqlite3_stmt* countStmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), countSql.c_str(), -1, &countStmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<PaginatedResult<Transaction>>("Failed to prepare count query", 500);
    }

    sqlite3_bind_int(countStmt, 1, walletId);
    if (direction.has_value()) {
        sqlite3_bind_text(countStmt, 2, direction->c_str(), -1, SQLITE_STATIC);
    }

    int totalCount = 0;
    rc = sqlite3_step(countStmt);
    if (rc == SQLITE_ROW) {
        totalCount = sqlite3_column_int(countStmt, 0);
    }
    sqlite3_finalize(countStmt);

    // Execute main query
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<PaginatedResult<Transaction>>("Failed to prepare transactions query", 500);
    }

    // Bind parameters
    for (size_t i = 0; i < bindParams.size(); ++i) {
        if (i == 0 || (direction.has_value() && i == 1)) {
            // Integer or string binding based on parameter
            if (i == 0) {
                sqlite3_bind_int(stmt, static_cast<int>(i + 1), std::stoi(bindParams[i]));
            } else {
                sqlite3_bind_text(stmt, static_cast<int>(i + 1), bindParams[i].c_str(), -1, SQLITE_STATIC);
            }
        } else {
            // Limit and offset are always integers
            sqlite3_bind_int(stmt, static_cast<int>(i + 1), std::stoi(bindParams[i]));
        }
    }

    std::vector<Transaction> transactions;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        transactions.push_back(mapRowToTransaction(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        PaginatedResult<Transaction> result(transactions, totalCount, params.offset, params.limit);
        return Result<PaginatedResult<Transaction>>(result);
    } else {
        return Result<PaginatedResult<Transaction>>("Database error while retrieving transactions", 500);
    }
}

Result<bool> TransactionRepository::updateTransactionConfirmation(const std::string& txid,
                                                                 int blockHeight,
                                                                 const std::string& blockHash,
                                                                 int confirmationCount) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "updateTransactionConfirmation");

    const std::string sql = R"(
        UPDATE transactions
        SET block_height = ?, block_hash = ?, confirmation_count = ?, is_confirmed = ?
        WHERE txid = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare confirmation update statement", 500);
    }

    sqlite3_bind_int(stmt, 1, blockHeight);
    sqlite3_bind_text(stmt, 2, blockHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, confirmationCount);
    sqlite3_bind_int(stmt, 4, confirmationCount >= MIN_CONFIRMATIONS_FOR_CONFIRMED ? 1 : 0);
    sqlite3_bind_text(stmt, 5, txid.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        int changes = sqlite3_changes(m_dbManager.getHandle());
        if (changes > 0) {
            REPO_LOG_INFO(COMPONENT_NAME, "Transaction confirmation updated",
                         "TXID: " + txid + ", Confirmations: " + std::to_string(confirmationCount));
            return Result<bool>(true);
        } else {
            return Result<bool>("Transaction not found", 404);
        }
    } else {
        return Result<bool>("Database error during confirmation update", 500);
    }
}

Result<TransactionStats> TransactionRepository::getTransactionStats(int walletId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getTransactionStats");

    const std::string sql = R"(
        SELECT
            COUNT(*) as total_transactions,
            COUNT(CASE WHEN is_confirmed = 1 THEN 1 END) as confirmed_transactions,
            COUNT(CASE WHEN is_confirmed = 0 THEN 1 END) as pending_transactions,
            COALESCE(SUM(CASE WHEN direction = 'incoming' THEN amount_satoshis ELSE 0 END), 0) as total_received,
            COALESCE(SUM(CASE WHEN direction = 'outgoing' THEN amount_satoshis ELSE 0 END), 0) as total_sent,
            COALESCE(SUM(fee_satoshis), 0) as total_fees,
            MIN(created_at) as first_transaction,
            MAX(created_at) as last_transaction
        FROM transactions
        WHERE wallet_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<TransactionStats>("Failed to prepare transaction stats query", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        TransactionStats stats;
        stats.totalTransactions = sqlite3_column_int(stmt, 0);
        stats.confirmedTransactions = sqlite3_column_int(stmt, 1);
        stats.pendingTransactions = sqlite3_column_int(stmt, 2);
        stats.totalReceived = sqlite3_column_int64(stmt, 3);
        stats.totalSent = sqlite3_column_int64(stmt, 4);
        stats.totalFees = sqlite3_column_int64(stmt, 5);

        const char* firstTxStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (firstTxStr && strlen(firstTxStr) > 0) {
            stats.firstTransaction = std::chrono::system_clock::now(); // Simplified
        }

        const char* lastTxStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        if (lastTxStr && strlen(lastTxStr) > 0) {
            stats.lastTransaction = std::chrono::system_clock::now(); // Simplified
        }

        sqlite3_finalize(stmt);
        return Result<TransactionStats>(stats);
    } else {
        sqlite3_finalize(stmt);
        return Result<TransactionStats>("Database error while retrieving transaction stats", 500);
    }
}

Result<WalletBalance> TransactionRepository::calculateWalletBalance(int walletId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "calculateWalletBalance");

    const std::string sql = R"(
        SELECT
            COALESCE(SUM(CASE WHEN is_confirmed = 1 THEN
                CASE WHEN direction = 'incoming' THEN amount_satoshis
                     WHEN direction = 'outgoing' THEN -(amount_satoshis + fee_satoshis)
                     ELSE 0 END
            ELSE 0 END), 0) as confirmed_balance,
            COALESCE(SUM(CASE WHEN is_confirmed = 0 THEN
                CASE WHEN direction = 'incoming' THEN amount_satoshis
                     WHEN direction = 'outgoing' THEN -(amount_satoshis + fee_satoshis)
                     ELSE 0 END
            ELSE 0 END), 0) as unconfirmed_balance,
            COUNT(DISTINCT CASE WHEN is_confirmed = 1 AND direction = 'incoming' THEN txid END) as utxo_count
        FROM transactions
        WHERE wallet_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<WalletBalance>("Failed to prepare balance calculation query", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        WalletBalance balance;
        balance.confirmedBalance = sqlite3_column_int64(stmt, 0);
        balance.unconfirmedBalance = sqlite3_column_int64(stmt, 1);
        balance.totalBalance = balance.confirmedBalance + balance.unconfirmedBalance;
        balance.utxoCount = sqlite3_column_int(stmt, 2);

        sqlite3_finalize(stmt);
        return Result<WalletBalance>(balance);
    } else {
        sqlite3_finalize(stmt);
        return Result<WalletBalance>("Database error while calculating balance", 500);
    }
}

Transaction TransactionRepository::mapRowToTransaction(sqlite3_stmt* stmt) {
    Transaction transaction;
    transaction.id = sqlite3_column_int(stmt, 0);
    transaction.walletId = sqlite3_column_int(stmt, 1);
    transaction.txid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

    if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
        transaction.blockHeight = sqlite3_column_int(stmt, 3);
    }

    const char* blockHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (blockHash && strlen(blockHash) > 0) {
        transaction.blockHash = blockHash;
    }

    transaction.amountSatoshis = sqlite3_column_int64(stmt, 5);
    transaction.feeSatoshis = sqlite3_column_int64(stmt, 6);
    transaction.direction = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));

    const char* fromAddress = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    if (fromAddress && strlen(fromAddress) > 0) {
        transaction.fromAddress = fromAddress;
    }

    const char* toAddress = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
    if (toAddress && strlen(toAddress) > 0) {
        transaction.toAddress = toAddress;
    }

    transaction.confirmationCount = sqlite3_column_int(stmt, 10);
    transaction.isConfirmed = sqlite3_column_int(stmt, 11) != 0;

    // Parse timestamps (simplified)
    transaction.createdAt = std::chrono::system_clock::now();

    const char* confirmedAtStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
    if (confirmedAtStr && strlen(confirmedAtStr) > 0) {
        transaction.confirmedAt = std::chrono::system_clock::now();
    }

    const char* memo = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
    if (memo && strlen(memo) > 0) {
        transaction.memo = memo;
    }

    return transaction;
}

Result<bool> TransactionRepository::validateTransaction(const Transaction& transaction) {
    if (transaction.walletId <= 0) {
        return Result<bool>("Invalid wallet ID", 400);
    }

    if (transaction.txid.empty()) {
        return Result<bool>("Transaction ID cannot be empty", 400);
    }

    if (transaction.direction != "incoming" && transaction.direction != "outgoing" && transaction.direction != "internal") {
        return Result<bool>("Invalid transaction direction", 400);
    }

    if (transaction.amountSatoshis < 0) {
        return Result<bool>("Transaction amount cannot be negative", 400);
    }

    if (transaction.feeSatoshis < 0) {
        return Result<bool>("Transaction fee cannot be negative", 400);
    }

    return Result<bool>(true);
}

Result<bool> TransactionRepository::updateAddressBalances(const Transaction& transaction) {
    // Update receiving address balance
    if (transaction.direction == "incoming" && transaction.toAddress.has_value()) {
        const std::string sql = R"(
            UPDATE addresses
            SET balance_satoshis = balance_satoshis + ?
            WHERE address = ?
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return Result<bool>("Failed to prepare address balance update", 500);
        }

        sqlite3_bind_int64(stmt, 1, transaction.amountSatoshis);
        sqlite3_bind_text(stmt, 2, transaction.toAddress->c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            return Result<bool>("Failed to update receiving address balance", 500);
        }
    }

    // Update sending address balance
    if (transaction.direction == "outgoing" && transaction.fromAddress.has_value()) {
        const std::string sql = R"(
            UPDATE addresses
            SET balance_satoshis = balance_satoshis - ?
            WHERE address = ?
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return Result<bool>("Failed to prepare address balance update", 500);
        }

        sqlite3_bind_int64(stmt, 1, transaction.amountSatoshis + transaction.feeSatoshis);
        sqlite3_bind_text(stmt, 2, transaction.fromAddress->c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            return Result<bool>("Failed to update sending address balance", 500);
        }
    }

    return Result<bool>(true);
}

} // namespace Repository

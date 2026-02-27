// TransactionRepository Extension Methods
// This file contains additional methods for TransactionRepository
// Include this content at the end of TransactionRepository.cpp

#include "../include/Repository/TransactionRepository.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

namespace Repository {

// Additional TransactionRepository methods to be merged into TransactionRepository.cpp:

/*
Result<PaginatedResult<Transaction>> TransactionRepository::getTransactionsByAddress(
    const std::string& address,
    const PaginationParams& params) {

    REPO_SCOPED_LOG(COMPONENT_NAME, "getTransactionsByAddress");

    std::string sql = R"(
        SELECT id, wallet_id, txid, block_height, block_hash, amount_satoshis, fee_satoshis,
               direction, from_address, to_address, confirmation_count, is_confirmed, created_at,
confirmed_at, memo FROM transactions WHERE from_address = ? OR to_address = ?
    )";

    sql += " ORDER BY " + params.sortField;
    if (!params.ascending) {
        sql += " DESC";
    }
    sql += " LIMIT ? OFFSET ?";

    // Get total count
    std::string countSql = "SELECT COUNT(*) FROM transactions WHERE from_address = ? OR to_address =
?"; sqlite3_stmt* countStmt = nullptr; int rc = sqlite3_prepare_v2(m_dbManager.getHandle(),
countSql.c_str(), -1, &countStmt, nullptr); if (rc != SQLITE_OK) { return
Result<PaginatedResult<Transaction>>("Failed to prepare count query", 500);
    }

    sqlite3_bind_text(countStmt, 1, address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(countStmt, 2, address.c_str(), -1, SQLITE_STATIC);

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

    sqlite3_bind_text(stmt, 1, address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, params.limit);
    sqlite3_bind_int(stmt, 4, params.offset);

    std::vector<Transaction> transactions;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        transactions.push_back(mapRowToTransaction(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        PaginatedResult<Transaction> result(transactions, totalCount, params.offset, params.limit);
        return Result<PaginatedResult<Transaction>>(result);
    } else {
        return Result<PaginatedResult<Transaction>>("Database error while retrieving transactions",
500);
    }
}

Result<bool> TransactionRepository::confirmTransaction(const std::string& txid,
                                                       const
std::optional<std::chrono::system_clock::time_point>& confirmedAt) { REPO_SCOPED_LOG(COMPONENT_NAME,
"confirmTransaction");

    const std::string sql = R"(
        UPDATE transactions
        SET is_confirmed = 1, confirmed_at = CURRENT_TIMESTAMP
        WHERE txid = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare confirmation update", 500);
    }

    sqlite3_bind_text(stmt, 1, txid.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        REPO_LOG_INFO(COMPONENT_NAME, "Transaction confirmed", "TXID: " + txid);
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during transaction confirmation", 500);
    }
}

Result<bool> TransactionRepository::updateTransactionMemo(int transactionId, const std::string&
memo) { REPO_SCOPED_LOG(COMPONENT_NAME, "updateTransactionMemo");

    const std::string sql = "UPDATE transactions SET memo = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare memo update", 500);
    }

    sqlite3_bind_text(stmt, 1, memo.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, transactionId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during memo update", 500);
    }
}

Result<std::vector<Transaction>> TransactionRepository::getRecentTransactionsForUser(int userId, int
limit) { REPO_SCOPED_LOG(COMPONENT_NAME, "getRecentTransactionsForUser");

    const std::string sql = R"(
        SELECT t.id, t.wallet_id, t.txid, t.block_height, t.block_hash, t.amount_satoshis,
t.fee_satoshis, t.direction, t.from_address, t.to_address, t.confirmation_count, t.is_confirmed,
               t.created_at, t.confirmed_at, t.memo
        FROM transactions t
        JOIN wallets w ON t.wallet_id = w.id
        WHERE w.user_id = ?
        ORDER BY t.created_at DESC
        LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<Transaction>>("Failed to prepare recent transactions query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_int(stmt, 2, limit);

    std::vector<Transaction> transactions;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        transactions.push_back(mapRowToTransaction(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<std::vector<Transaction>>(transactions);
    } else {
        return Result<std::vector<Transaction>>("Database error while retrieving transactions",
500);
    }
}

Result<std::vector<Transaction>> TransactionRepository::getPendingTransactions(int walletId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getPendingTransactions");

    const std::string sql = R"(
        SELECT id, wallet_id, txid, block_height, block_hash, amount_satoshis, fee_satoshis,
               direction, from_address, to_address, confirmation_count, is_confirmed, created_at,
confirmed_at, memo FROM transactions WHERE wallet_id = ? AND is_confirmed = 0 ORDER BY created_at
DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<Transaction>>("Failed to prepare pending transactions query",
500);
    }

    sqlite3_bind_int(stmt, 1, walletId);

    std::vector<Transaction> transactions;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        transactions.push_back(mapRowToTransaction(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<std::vector<Transaction>>(transactions);
    } else {
        return Result<std::vector<Transaction>>("Database error while retrieving pending
transactions", 500);
    }
}

// Note: Additional methods like addTransactionInputs, addTransactionOutputs, getTransactionInputs,
// getTransactionOutputs, markOutputAsSpent, getUTXOs, deleteOldTransactions, searchTransactions,
// mapRowToTransactionInput, mapRowToTransactionOutput, and buildSearchWhereClause
// should be added similarly to complete the TransactionRepository implementation.
*/

}  // namespace Repository

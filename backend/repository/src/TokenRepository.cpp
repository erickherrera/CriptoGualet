#include "Repository/TokenRepository.h"
#include <sqlite3.h>

namespace Repository {

TokenRepository::TokenRepository(Database::DatabaseManager& dbManager)
    : m_dbManager(dbManager) {}

Result<Token> TokenRepository::createToken(int wallet_id, const std::string& contract_address, const std::string& symbol, const std::string& name, int decimals) {
    std::string sql = "INSERT INTO erc20_tokens (wallet_id, contract_address, symbol, name, decimals) VALUES (?, ?, ?, ?, ?);";
    
    auto result = m_dbManager.executeQuery(sql, {std::to_string(wallet_id), contract_address, symbol, name, std::to_string(decimals)});

    if (!result.success) {
        return Result<Token>("Failed to create token in database.");
    }

    // Get the last inserted token
    return getToken(wallet_id, contract_address);
}

Result<Token> TokenRepository::getToken(int wallet_id, const std::string& contract_address) {
    std::string sql = "SELECT id, wallet_id, contract_address, symbol, name, decimals, created_at FROM erc20_tokens WHERE wallet_id = ? AND contract_address = ?;";
    
    Result<Token> repoResult;
    repoResult.success = false;

    auto dbResult = m_dbManager.executeQuery(sql, {std::to_string(wallet_id), contract_address}, [&](sqlite3* db) {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, wallet_id);
            sqlite3_bind_text(stmt, 2, contract_address.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                Token token;
                token.id = sqlite3_column_int(stmt, 0);
                token.wallet_id = sqlite3_column_int(stmt, 1);
                token.contract_address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                token.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                token.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                token.decimals = sqlite3_column_int(stmt, 5);
                token.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
                
                repoResult.success = true;
                repoResult.data = token;
            }
            sqlite3_finalize(stmt);
        }
    });

    if (dbResult.success && !repoResult.success) {
        repoResult.errorMessage = "Token not found.";
    } else if (!dbResult.success) {
        repoResult.errorMessage = dbResult.message;
    }

    return repoResult;
}

Result<std::vector<Token>> TokenRepository::getTokensForWallet(int wallet_id) {
    std::string sql = "SELECT id, wallet_id, contract_address, symbol, name, decimals, created_at FROM erc20_tokens WHERE wallet_id = ?;";
    
    Result<std::vector<Token>> repoResult;
    std::vector<Token> tokens;
    repoResult.success = false;

    auto dbResult = m_dbManager.executeQuery(sql, {std::to_string(wallet_id)}, [&](sqlite3* db) {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, wallet_id);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Token token;
                token.id = sqlite3_column_int(stmt, 0);
                token.wallet_id = sqlite3_column_int(stmt, 1);
                token.contract_address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                token.symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                token.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                token.decimals = sqlite3_column_int(stmt, 5);
                token.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
                tokens.push_back(token);
            }
            repoResult.success = true;
            repoResult.data = tokens;
            sqlite3_finalize(stmt);
        }
    });

    if (!dbResult.success) {
        repoResult.errorMessage = dbResult.message;
    }

    return repoResult;
}

Result<bool> TokenRepository::deleteToken(int wallet_id, const std::string& contract_address) {
    std::string sql = "DELETE FROM erc20_tokens WHERE wallet_id = ? AND contract_address = ?;";
    
    auto result = m_dbManager.executeQuery(sql, {std::to_string(wallet_id), contract_address});

    if (!result.success) {
        return Result<bool>("Failed to delete token from database.");
    }
    return Result<bool>(true);
}

} // namespace Repository

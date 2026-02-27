#include "Repository/SessionRepository.h"
#include "Database/DatabaseManager.h"
#include <vector>

#ifdef SQLCIPHER_AVAILABLE
#define SQLITE_HAS_CODEC 1
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif

SessionRepository::SessionRepository(Database::DatabaseManager& dbManager) 
    : m_dbManager(dbManager) {
    // Queries removed from constructor to prevent issues with uninitialized DatabaseManager
}

bool SessionRepository::ensureTableExists() {
    std::string createTableQuery = R"(
        CREATE TABLE IF NOT EXISTS sessions (
            sessionId TEXT PRIMARY KEY,
            userId INTEGER NOT NULL,
            username TEXT NOT NULL,
            createdAt TEXT NOT NULL,
            expiresAt TEXT NOT NULL,
            lastActivity TEXT NOT NULL,
            ipAddress TEXT,
            userAgent TEXT,
            totpAuthenticated INTEGER,
            isActive INTEGER
        );
    )";
    return m_dbManager.executeQuery(createTableQuery).success;
}

bool SessionRepository::storeSession(const SessionRecord& session) {
    ensureTableExists();
    std::string insertQuery = R"(
        INSERT INTO sessions (sessionId, userId, username, createdAt, expiresAt, lastActivity, ipAddress, userAgent, totpAuthenticated, isActive)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    std::vector<std::string> params;
    params.push_back(session.sessionId);
    params.push_back(std::to_string(session.userId));
    params.push_back(session.username);
    params.push_back(std::to_string(std::chrono::system_clock::to_time_t(session.createdAt)));
    params.push_back(std::to_string(std::chrono::system_clock::to_time_t(session.expiresAt)));
    params.push_back(std::to_string(std::chrono::system_clock::to_time_t(session.lastActivity)));
    params.push_back(session.ipAddress);
    params.push_back(session.userAgent);
    params.push_back(std::to_string(session.totpAuthenticated));
    params.push_back(std::to_string(session.isActive));

    Database::DatabaseResult result = m_dbManager.executeQuery(insertQuery, params);
    return result.success;
}

std::optional<SessionRecord> SessionRepository::getSession(const std::string& sessionId) const {
    const_cast<SessionRepository*>(this)->ensureTableExists();
    std::string selectQuery = "SELECT * FROM sessions WHERE sessionId = ?;";
    std::optional<SessionRecord> sessionRecord;

    m_dbManager.executeQuery(selectQuery, {sessionId}, [&](sqlite3* db) {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, selectQuery.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                SessionRecord record;
                record.sessionId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                record.userId = sqlite3_column_int(stmt, 1);
                record.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                record.createdAt = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 3));
                record.expiresAt = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 4));
                record.lastActivity = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 5));
                record.ipAddress = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
                record.userAgent = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
                record.totpAuthenticated = sqlite3_column_int(stmt, 8);
                record.isActive = sqlite3_column_int(stmt, 9);
                sessionRecord = record;
            }
            sqlite3_finalize(stmt);
        }
    });

    return sessionRecord;
}

bool SessionRepository::updateSessionActivity(const std::string& sessionId) {
    ensureTableExists();
    std::string updateQuery = "UPDATE sessions SET lastActivity = ?, expiresAt = ? WHERE sessionId = ?;";
    
    auto now = std::chrono::system_clock::now();
    auto newExpiresAt = now + std::chrono::minutes(15);

    std::vector<std::string> params;
    params.push_back(std::to_string(std::chrono::system_clock::to_time_t(now)));
    params.push_back(std::to_string(std::chrono::system_clock::to_time_t(newExpiresAt)));
    params.push_back(sessionId);

    Database::DatabaseResult result = m_dbManager.executeQuery(updateQuery, params);
    return result.success;
}

bool SessionRepository::invalidateSession(const std::string& sessionId) {
    ensureTableExists();
    std::string updateQuery = "UPDATE sessions SET isActive = 0 WHERE sessionId = ?;";
    Database::DatabaseResult result = m_dbManager.executeQuery(updateQuery, {sessionId});
    return result.success;
}

std::vector<SessionRecord> SessionRepository::getActiveSessions(int userId) const {
    const_cast<SessionRepository*>(this)->ensureTableExists();
    std::string selectQuery = "SELECT * FROM sessions WHERE userId = ? AND isActive = 1;";
    std::vector<SessionRecord> activeSessions;

    m_dbManager.executeQuery(selectQuery, {std::to_string(userId)}, [&](sqlite3* db) {
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, selectQuery.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, userId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                SessionRecord record;
                record.sessionId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                record.userId = sqlite3_column_int(stmt, 1);
                record.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                record.createdAt = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 3));
                record.expiresAt = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 4));
                record.lastActivity = std::chrono::system_clock::from_time_t(sqlite3_column_int64(stmt, 5));
                record.ipAddress = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
                record.userAgent = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
                record.totpAuthenticated = sqlite3_column_int(stmt, 8);
                record.isActive = sqlite3_column_int(stmt, 9);
                activeSessions.push_back(record);
            }
            sqlite3_finalize(stmt);
        }
    });

    return activeSessions;
}

void SessionRepository::cleanupExpiredSessions() {
    ensureTableExists();
    std::string deleteQuery = "DELETE FROM sessions WHERE expiresAt < ?;";
    std::string now = std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    m_dbManager.executeQuery(deleteQuery, {now});
}

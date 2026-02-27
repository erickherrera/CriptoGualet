#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace Database {
class DatabaseManager;
}

struct SessionRecord {
    std::string sessionId;
    int userId;
    std::string username;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point expiresAt;
    std::chrono::system_clock::time_point lastActivity;
    std::string ipAddress;
    std::string userAgent;
    bool totpAuthenticated;
    bool isActive;
};

class SessionRepository {
  public:
    explicit SessionRepository(Database::DatabaseManager& dbManager);
    bool storeSession(const SessionRecord& session);
    std::optional<SessionRecord> getSession(const std::string& sessionId) const;
    bool updateSessionActivity(const std::string& sessionId);
    bool invalidateSession(const std::string& sessionId);
    std::vector<SessionRecord> getActiveSessions(int userId) const;
    void cleanupExpiredSessions();

  private:
    Database::DatabaseManager& m_dbManager;
    bool ensureTableExists();
};

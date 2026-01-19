#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>

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
    SessionRepository();
    bool storeSession(const SessionRecord& session);
    std::optional<SessionRecord> getSession(const std::string& sessionId) const;
    bool updateSessionActivity(const std::string& sessionId);
    bool invalidateSession(const std::string& sessionId);
    std::vector<SessionRecord> getActiveSessions(int userId) const;
    void cleanupExpiredSessions();
};

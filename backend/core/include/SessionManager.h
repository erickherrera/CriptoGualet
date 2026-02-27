#pragma once

#include "Repository/SessionRepository.h"
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct UserSession {
    int userId;
    std::string username;
    std::string sessionId;
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point lastActivity;
    std::chrono::steady_clock::time_point expiresAt;
    bool totpAuthenticated;

    struct WalletData {
        std::string btcAddress;
        std::string ltcAddress;
        std::string ethAddress;
        double btcBalance;
        double ltcBalance;
        double ethBalance;
    } walletData;

    bool isActive;

    bool isExpired() const;
    bool isFullyAuthenticated() const;
    bool canPerformSensitiveOperation() const;
    void clearSensitiveData();
};

class SessionManager {
  public:
    explicit SessionManager(Database::DatabaseManager& dbManager);
    std::string createSession(int userId, const std::string& username,
                              bool totpAuthenticated = false);
    bool validateSession(const std::string& sessionId);
    void refreshSession(const std::string& sessionId);
    void invalidateSession(const std::string& sessionId);
    UserSession* getCurrentSession();
    UserSession* getSession(const std::string& sessionId);
    void cleanup();

  private:
    std::map<std::string, UserSession> activeSessions_;
    std::string currentSessionId_;
    SessionRepository sessionRepository_;
};

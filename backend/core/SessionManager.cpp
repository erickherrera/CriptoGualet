#include "SessionManager.h"
#include "Crypto.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

SessionManager::SessionManager() {
    //
}

std::string SessionManager::createSession(int userId, const std::string& username, bool totpAuthenticated) {
    UserSession session;
    session.userId = userId;
    session.username = username;
    
    session.sessionId = Crypto::GenerateSecureRandomString(32);

    session.createdAt = std::chrono::steady_clock::now();
    session.lastActivity = session.createdAt;
    session.expiresAt = session.createdAt + std::chrono::minutes(15);
    session.totpAuthenticated = totpAuthenticated;
    session.isActive = true;

    activeSessions_[session.sessionId] = session;
    currentSessionId_ = session.sessionId;

    SessionRecord record;
    record.sessionId = session.sessionId;
    record.userId = session.userId;
    record.username = session.username;
    record.createdAt = std::chrono::system_clock::now();
    record.expiresAt = record.createdAt + std::chrono::minutes(15);
    record.lastActivity = record.createdAt;
    record.ipAddress = ""; // Should be filled with real IP
    record.userAgent = ""; // Should be filled with real user agent
    record.totpAuthenticated = totpAuthenticated;
    record.isActive = true;
    sessionRepository_.storeSession(record);

    return session.sessionId;
}

bool SessionManager::validateSession(const std::string& sessionId) {
    auto it = activeSessions_.find(sessionId);
    if (it == activeSessions_.end()) {
        return false;
    }
    
    if (!it->second.isExpired() && it->second.isActive) {
        refreshSession(sessionId);
        return true;
    }

    return false;
}


void SessionManager::refreshSession(const std::string& sessionId) {
    auto it = activeSessions_.find(sessionId);
    if (it != activeSessions_.end()) {
        it->second.lastActivity = std::chrono::steady_clock::now();
        it->second.expiresAt = it->second.lastActivity + std::chrono::minutes(15);
        sessionRepository_.updateSessionActivity(sessionId);
    }
}

void SessionManager::invalidateSession(const std::string& sessionId) {
    auto it = activeSessions_.find(sessionId);
    if (it != activeSessions_.end()) {
        it->second.clearSensitiveData();
        it->second.isActive = false;
        sessionRepository_.invalidateSession(sessionId);
    }
}

UserSession* SessionManager::getCurrentSession() {
    auto it = activeSessions_.find(currentSessionId_);
    if (it != activeSessions_.end()) {
        return &it->second;
    }
    return nullptr;
}

UserSession* SessionManager::getSession(const std::string& sessionId) {
    auto it = activeSessions_.find(sessionId);
    if (it != activeSessions_.end()) {
        return &it->second;
    }
    return nullptr;
}

void SessionManager::cleanup() {
    activeSessions_.clear();
    currentSessionId_.clear();
    sessionRepository_.cleanupExpiredSessions();
}

bool UserSession::isExpired() const {
    return std::chrono::steady_clock::now() > expiresAt;
}

bool UserSession::isFullyAuthenticated() const {
    return totpAuthenticated && isActive;
}

bool UserSession::canPerformSensitiveOperation() const {
    return isFullyAuthenticated();
}

void UserSession::clearSensitiveData() {
    walletData.btcAddress.clear();
    walletData.ltcAddress.clear();
    walletData.ethAddress.clear();
    walletData.btcBalance = 0.0;
    walletData.ltcBalance = 0.0;
    walletData.ethBalance = 0.0;
}
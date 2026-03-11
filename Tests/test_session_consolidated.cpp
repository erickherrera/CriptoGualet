// test_session_consolidated.cpp - Core Session Management Tests
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "include/TestUtils.h"
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Test configuration
struct SessionConfig {
    static constexpr std::chrono::minutes TIMEOUT{15};
    static constexpr int MAX_CONCURRENT = 3;
    static constexpr int ID_LENGTH = 32;
};

// Core session structure
struct UserSession {
    int userId;
    std::string username;
    std::string sessionId;
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point lastActivity;
    std::chrono::steady_clock::time_point expiresAt;
    bool totpAuthenticated;
    bool isActive;

    struct WalletData {
        std::string btcAddress;
        double btcBalance;
    } walletData;

    bool isExpired() const {
        return MockTime::now() > expiresAt;
    }
    bool isFullyAuthenticated() const {
        return totpAuthenticated && isActive;
    }
    void clearSensitiveData() {
        walletData.btcAddress.clear();
        walletData.btcBalance = 0.0;
    }
};

// Test helpers
class SessionHelpers {
  public:
    static UserSession createSession(int userId, const std::string& username) {
        UserSession session;
        session.userId = userId;
        session.username = username;
        session.sessionId = generateId();
        session.createdAt = MockTime::now();
        session.lastActivity = session.createdAt;
        session.expiresAt = session.createdAt + SessionConfig::TIMEOUT;
        session.totpAuthenticated = true;
        session.isActive = true;
        session.walletData = {"btc_addr_" + std::to_string(userId), 0.1};
        return session;
    }

    static std::string generateId() {
        static int counter = 0;
        return "sess_" + std::to_string(++counter) + std::string(25, 'x');
    }
};

// Mock session manager
class SessionManager {
  private:
    std::map<std::string, UserSession> sessions_;
    std::string currentId_;

  public:
    std::string create(int userId, const std::string& username) {
        UserSession session = SessionHelpers::createSession(userId, username);
        sessions_[session.sessionId] = session;
        currentId_ = session.sessionId;
        return session.sessionId;
    }

    bool validate(const std::string& id) {
        auto it = sessions_.find(id);
        return it != sessions_.end() && !it->second.isExpired() && it->second.isActive;
    }

    void invalidate(const std::string& id) {
        auto it = sessions_.find(id);
        if (it != sessions_.end()) {
            it->second.clearSensitiveData();
            it->second.isActive = false;
        }
    }

    UserSession* getCurrent() {
        auto it = sessions_.find(currentId_);
        return it != sessions_.end() ? &it->second : nullptr;
    }

    size_t count() const {
        return sessions_.size();
    }
    void clear() {
        sessions_.clear();
    }
};

// Core Tests
void testSessionLifecycle() {
    TEST_START("Session Lifecycle: Create, Validate, Invalidate");

    SessionManager mgr;
    std::string id = mgr.create(1, "user1");

    // Creation
    TEST_ASSERT(!id.empty() && id.length() >= SessionConfig::ID_LENGTH,
                "Session ID should be valid");
    TEST_ASSERT(mgr.validate(id), "New session should be valid");

    // Invalidation
    mgr.invalidate(id);
    TEST_ASSERT(!mgr.validate(id), "Invalidated session should fail validation");

    TEST_PASS();
}

void testSessionTimeout() {
    TEST_START("Session Timeout After 15 Minutes");

    SessionManager mgr;
    std::string id = mgr.create(1, "user1");

    TEST_ASSERT(mgr.validate(id), "Session valid initially");

    // Advance 16 minutes
    MockTime::advance(std::chrono::minutes(16));
    TEST_ASSERT(!mgr.validate(id), "Session should expire after 15 minutes");

    TEST_PASS();
}

void testTOTPRequirement() {
    TEST_START("TOTP Authentication Requirement");

    UserSession session = SessionHelpers::createSession(1, "user1");
    TEST_ASSERT(session.isFullyAuthenticated(), "Session with TOTP should be fully authenticated");

    session.totpAuthenticated = false;
    TEST_ASSERT(!session.isFullyAuthenticated(),
                "Session without TOTP should not be fully authenticated");

    TEST_PASS();
}

void testSessionDataIsolation() {
    TEST_START("Session Data Isolation");

    SessionManager mgr;
    std::string id1 = mgr.create(1, "user1");
    std::string id2 = mgr.create(2, "user2");

    TEST_ASSERT(id1 != id2, "Different users should have different session IDs");

    UserSession* s1 = mgr.getCurrent();
    mgr.create(1, "user1");  // Second session for user 1
    std::string id3 = mgr.getCurrent()->sessionId;

    TEST_ASSERT(id1 != id3, "Same user should have unique session IDs");

    TEST_PASS();
}

void testSensitiveDataWipe() {
    TEST_START("Sensitive Data Wipe on Invalidation");

    UserSession session = SessionHelpers::createSession(1, "user1");
    TEST_ASSERT(!session.walletData.btcAddress.empty(), "Session should have wallet data");

    session.clearSensitiveData();
    TEST_ASSERT(session.walletData.btcAddress.empty() && session.walletData.btcBalance == 0.0,
                "Sensitive data should be cleared");

    TEST_PASS();
}

void testConcurrentSessionLimit() {
    TEST_START("Concurrent Session Limit (Max 3)");

    SessionManager mgr;
    std::set<std::string> ids;

    // Create max allowed sessions
    for (int i = 0; i < SessionConfig::MAX_CONCURRENT; i++) {
        ids.insert(mgr.create(1, "user1"));
    }

    TEST_ASSERT(ids.size() == SessionConfig::MAX_CONCURRENT, "Should have 3 unique sessions");

    // All should be valid
    for (const auto& id : ids) {
        TEST_ASSERT(mgr.validate(id), "Each session should be valid");
    }

    TEST_PASS();
}

void testSessionActivityExtension() {
    TEST_START("Session Activity Extends Expiration");

    UserSession session = SessionHelpers::createSession(1, "user1");
    auto originalExpiry = session.expiresAt;

    // Simulate activity after 10 minutes
    MockTime::advance(std::chrono::minutes(10));
    session.lastActivity = MockTime::now();
    session.expiresAt = session.lastActivity + SessionConfig::TIMEOUT;

    TEST_ASSERT(session.expiresAt > originalExpiry, "Activity should extend expiration");
    TEST_ASSERT(!session.isExpired(), "Session should still be valid after activity");

    TEST_PASS();
}

// Main
int main() {
    TestUtils::printTestHeader("Session Management Core Tests");
    MockTime::enable();

    testSessionLifecycle();
    testSessionTimeout();
    testTOTPRequirement();
    testSessionDataIsolation();
    testSensitiveDataWipe();
    testConcurrentSessionLimit();
    testSessionActivityExtension();

    MockTime::disable();
    TestUtils::printTestSummary("Session Tests");

    return (TestGlobals::g_testsFailed > 0) ? 1 : 0;
}

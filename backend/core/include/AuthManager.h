#pragma once

#include "Auth.h"
#include "SessionManager.h"

namespace Auth {

class AuthManager {
public:
    static AuthManager& getInstance();

    AuthResponse RegisterUser(const std::string& username, const std::string& password);
    AuthResponse LoginUser(const std::string& username, const std::string& password);
    void LogoutUser(const std::string& sessionId);
    UserSession* getSession(const std::string& sessionId);

private:
    AuthManager();
    ~AuthManager();

    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    SessionManager sessionManager_;
};

} // namespace Auth

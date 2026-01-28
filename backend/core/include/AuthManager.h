#pragma once

#include "Auth.h"
#include "SessionManager.h"
#include <optional>

// Forward declaration for Qt dialog
class QtSeedDisplayDialog;

namespace Auth {

class AuthManager {
public:
    static AuthManager& getInstance();

    AuthResponse RegisterUser(const std::string& username, const std::string& password, std::vector<std::string>& outMnemonic);
    AuthResponse LoginUser(const std::string& username, const std::string& password);
    AuthResponse RevealSeed(const std::string& username, const std::string& password,
                            std::string& outSeedHex, std::optional<std::string>& outMnemonic);
    AuthResponse RestoreFromSeed(const std::string& username, const std::string& mnemonicText,
                                 const std::string& passphrase,
                                 const std::string& passwordForReauth);
    void LogoutUser(const std::string& sessionId);
    void cleanupSessions();
    UserSession* getSession(const std::string& sessionId);

private:
    AuthManager();
    ~AuthManager();

    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    SessionManager sessionManager_;
};

} // namespace Auth

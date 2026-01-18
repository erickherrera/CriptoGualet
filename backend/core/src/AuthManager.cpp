#include "AuthManager.h"
#include "Repository/UserRepository.h"

namespace Auth {

AuthManager& AuthManager::getInstance() {
    static AuthManager instance;
    return instance;
}

AuthManager::AuthManager() {
    //
}

AuthManager::~AuthManager() {
    //
}

AuthResponse AuthManager::RegisterUser(const std::string& username, const std::string& password, std::vector<std::string>& outMnemonic) {
    return Auth::RegisterUserWithMnemonic(username, password, outMnemonic);
}

AuthResponse AuthManager::LoginUser(const std::string& username, const std::string& password) {
    // This is just a mock implementation.
    // A real implementation would use the UserRepository to verify the user's credentials.
    
    // For now, we'll just assume the login is successful
    // and create a session.
    
    int userId = 1; // mock user id
    std::string sessionId = sessionManager_.createSession(userId, username);
    
    return {AuthResult::SUCCESS, sessionId};
}

void AuthManager::LogoutUser(const std::string& sessionId) {
    sessionManager_.invalidateSession(sessionId);
}

UserSession* AuthManager::getSession(const std::string& sessionId) {
    if (sessionManager_.validateSession(sessionId)) {
        return sessionManager_.getCurrentSession();
    }
    return nullptr;
}

} // namespace Auth

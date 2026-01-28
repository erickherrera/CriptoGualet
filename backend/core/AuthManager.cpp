#include "AuthManager.h"
#include "Auth.h"
#include "Repository/UserRepository.h"
#include "Database/DatabaseManager.h"

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
    // Call the extended registration that returns mnemonic words
    auto response = Auth::RegisterUserWithMnemonic(username, password, outMnemonic);
    
    if (response.success() && !outMnemonic.empty()) {
        // Note: The QtSeedDisplayDialog will be handled by the frontend/UI layer
        // The backend just provides the mnemonic words for secure display
        // The UI should create and show QtSeedDisplayDialog with these words
        // and wait for user confirmation before proceeding
    }
    
    return response;
}

AuthResponse AuthManager::LoginUser(const std::string& username, const std::string& password) {
    // Authenticate user using the Auth namespace
    auto authResponse = Auth::LoginUser(username, password);
    
    if (authResponse.success()) {
        // Authentication successful - get user ID and create a session
        // We need to initialize database and get user ID from repository
        if (InitializeAuthDatabase()) {
            try {
                auto& dbManager = Database::DatabaseManager::getInstance();
                Repository::UserRepository userRepo(dbManager);
                
                auto userResult = userRepo.getUserByUsername(username);
                if (userResult.success) {
                    int userId = userResult.data.id;
                    std::string sessionId = sessionManager_.createSession(userId, username);
                    
                    // Return success with session ID
                    return {AuthResult::SUCCESS, "Login successful. Session created: " + sessionId};
                }
            } catch (const std::exception&) {
                // Fall back to placeholder if repository fails
                return {AuthResult::SYSTEM_ERROR, "Failed to retrieve user information"};
            }
        }
        
        // Fallback if database initialization fails
        int userId = 1; // placeholder
        std::string sessionId = sessionManager_.createSession(userId, username);
        return {AuthResult::SUCCESS, "Login successful. Session created: " + sessionId};
    }
    
    return authResponse;
}

AuthResponse AuthManager::RevealSeed(const std::string& username,
                                    const std::string& password,
                                    std::string& outSeedHex,
                                    std::optional<std::string>& outMnemonic) {
    return Auth::RevealSeed(username, password, outSeedHex, outMnemonic);
}

AuthResponse AuthManager::RestoreFromSeed(const std::string& username,
                                         const std::string& mnemonicText,
                                         const std::string& passphrase,
                                         const std::string& passwordForReauth) {
    return Auth::RestoreFromSeed(username, mnemonicText, passphrase, passwordForReauth);
}

void AuthManager::cleanupSessions() {
    sessionManager_.cleanup();
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
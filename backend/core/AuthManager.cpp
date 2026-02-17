#include "AuthManager.h"
#include "Auth.h"
#include "Database/DatabaseManager.h"
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

AuthResponse AuthManager::RegisterUser(const std::string& username, const std::string& password,
                                       std::vector<std::string>& outMnemonic) {
    // SECURITY: Ensure database is initialized before registration
    if (!InitializeAuthDatabase()) {
        return {AuthResult::SYSTEM_ERROR, "Failed to initialize authentication database."};
    }

    // Call the extended registration that returns mnemonic words
    auto response = Auth::RegisterUserWithMnemonic(username, password, outMnemonic);

    if (response.success() && !outMnemonic.empty()) {
        // Registration successful and mnemonic generated
        // The UI will handle displaying the QtSeedDisplayDialog
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

                    // SECURITY: Do not include session ID in user-visible message
                    AuthResponse response = {AuthResult::SUCCESS, "Login successful. Welcome to CriptoGualet!"};
                    response.sessionId = sessionId;
                    return response;
                }
            } catch (const std::exception&) {
                // Fall back to placeholder if repository fails
                return {AuthResult::SYSTEM_ERROR, "Failed to retrieve user information"};
            }
        }

        // Return error if database initialization or user retrieval failed
        return {AuthResult::SYSTEM_ERROR, "Failed to initialize session"};
    }

    return authResponse;
}

AuthResponse AuthManager::RevealSeed(const std::string& username, const std::string& password,
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

AuthResponse AuthManager::VerifyTwoFactorCode(const std::string& username, const std::string& totpCode) {
    auto authResponse = Auth::VerifyTwoFactorCode(username, totpCode);

    if (authResponse.success()) {
        if (InitializeAuthDatabase()) {
            try {
                auto& dbManager = Database::DatabaseManager::getInstance();
                Repository::UserRepository userRepo(dbManager);

                auto userResult = userRepo.getUserByUsername(username);
                if (userResult.success) {
                    int userId = userResult.data.id;
                    std::string sessionId = sessionManager_.createSession(userId, username);

                    AuthResponse response = {AuthResult::SUCCESS, "Verification successful."};
                    response.sessionId = sessionId;
                    return response;
                }
            } catch (const std::exception&) {
                return {AuthResult::SYSTEM_ERROR, "Failed to retrieve user information"};
            }
        }
        return {AuthResult::SYSTEM_ERROR, "Failed to initialize session"};
    }

    return authResponse;
}

void AuthManager::cleanupSessions() {
    sessionManager_.cleanup();
}

void AuthManager::LogoutUser(const std::string& sessionId) {
    sessionManager_.invalidateSession(sessionId);
}

UserSession* AuthManager::getSession(const std::string& sessionId) {
    if (sessionManager_.validateSession(sessionId)) {
        return sessionManager_.getSession(sessionId);
    }
    return nullptr;
}

}  // namespace Auth
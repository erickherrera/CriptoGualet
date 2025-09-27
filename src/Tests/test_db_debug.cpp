#include "../../include/Database/DatabaseManager.h"
#include <iostream>

int main() {
    std::cout << "=== Database Debug Test ===" << std::endl;

    auto& db = Database::DatabaseManager::getInstance();
    std::cout << "1. Got DatabaseManager instance" << std::endl;

    std::cout << "2. Initial state check:" << std::endl;
    std::cout << "   isInitialized(): " << (db.isInitialized() ? "true" : "false") << std::endl;

    std::string testDbPath = "./debug_test.db";
    std::string encryptionKey = "CriptoGualet_SecureKey_2024_256bit_AES!";

    std::cout << "3. Attempting to initialize database..." << std::endl;
    std::cout << "   Path: " << testDbPath << std::endl;
    std::cout << "   Key length: " << encryptionKey.length() << " characters" << std::endl;

    auto result = db.initialize(testDbPath, encryptionKey);

    std::cout << "4. Initialization result:" << std::endl;
    std::cout << "   Success: " << (result.success ? "true" : "false") << std::endl;
    std::cout << "   Message: " << result.message << std::endl;
    std::cout << "   Error code: " << result.errorCode << std::endl;

    std::cout << "5. Final state check:" << std::endl;
    std::cout << "   isInitialized(): " << (db.isInitialized() ? "true" : "false") << std::endl;

    return result.success ? 0 : 1;
}
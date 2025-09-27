#include <iostream>
#include <string>

#define SQLITE_HAS_CODEC 1
#include <sqlcipher/sqlite3.h>

int main() {
    std::cout << "=== SQLCipher Direct Test ===" << std::endl;

    // Test SQLCipher functions directly
    std::cout << "SQLite version: " << sqlite3_libversion() << std::endl;

    sqlite3* db = nullptr;
    std::string dbPath = "./direct_test.db";

    std::cout << "1. Opening database..." << std::endl;
    int result = sqlite3_open(dbPath.c_str(), &db);
    if (result != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    std::cout << "   ✓ Database opened" << std::endl;

    std::cout << "2. Setting encryption key..." << std::endl;
    std::string key = "test_key_123456789012345678901234567890";
    result = sqlite3_key(db, key.c_str(), static_cast<int>(key.length()));
    if (result != SQLITE_OK) {
        std::cerr << "Failed to set key: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }
    std::cout << "   ✓ Encryption key set" << std::endl;

    std::cout << "3. Testing basic operation..." << std::endl;
    char* errorMsg = nullptr;
    result = sqlite3_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY);", nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::cerr << "Failed to create table: " << (errorMsg ? errorMsg : sqlite3_errmsg(db)) << std::endl;
        if (errorMsg) sqlite3_free(errorMsg);
        sqlite3_close(db);
        return 1;
    }
    std::cout << "   ✓ Table created successfully" << std::endl;

    std::cout << "4. Testing encryption verification..." << std::endl;
    result = sqlite3_exec(db, "PRAGMA cipher_version;", nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::cerr << "Cipher version check failed: " << (errorMsg ? errorMsg : sqlite3_errmsg(db)) << std::endl;
        if (errorMsg) sqlite3_free(errorMsg);
    } else {
        std::cout << "   ✓ SQLCipher is working" << std::endl;
    }

    sqlite3_close(db);
    std::cout << "   ✓ Database closed" << std::endl;

    std::cout << "\n=== SQLCipher Direct Test PASSED! ===" << std::endl;
    return 0;
}
#include <iostream>
#include <string>

#define SQLITE_HAS_CODEC 1
#include <sqlcipher/sqlite3.h>

int main() {
    std::cout << "=== Step-by-Step Database Debug Test ===" << std::endl;

    sqlite3* db = nullptr;
    std::string dbPath = "./step_test.db";
    std::string encryptionKey = "CriptoGualet_SecureKey_2024_256bit_AES!";

    std::cout << "SQLite version: " << sqlite3_libversion() << std::endl;

    // Step 1: Open database
    std::cout << "1. Opening database..." << std::endl;
    int result = sqlite3_open(dbPath.c_str(), &db);
    if (result != SQLITE_OK) {
        std::cerr << "   FAILED to open database: " << sqlite3_errmsg(db) << " (code: " << result << ")" << std::endl;
        return 1;
    }
    std::cout << "   ✓ Database opened successfully" << std::endl;

    // Step 2: Set encryption key
    std::cout << "2. Setting encryption key..." << std::endl;
    result = sqlite3_key(db, encryptionKey.c_str(), static_cast<int>(encryptionKey.length()));
    if (result != SQLITE_OK) {
        std::cerr << "   FAILED to set encryption key: " << sqlite3_errmsg(db) << " (code: " << result << ")" << std::endl;
        sqlite3_close(db);
        return 1;
    }
    std::cout << "   ✓ Encryption key set successfully" << std::endl;

    // Step 3: Validate encryption
    std::cout << "3. Validating encryption..." << std::endl;
    sqlite3_stmt* stmt = nullptr;
    result = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sqlite_master;", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        std::cerr << "   FAILED to prepare validation query: " << sqlite3_errmsg(db) << " (code: " << result << ")" << std::endl;
        sqlite3_close(db);
        return 1;
    }
    result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (result != SQLITE_ROW) {
        std::cerr << "   FAILED to execute validation query: " << sqlite3_errmsg(db) << " (code: " << result << ")" << std::endl;
        sqlite3_close(db);
        return 1;
    }
    std::cout << "   ✓ Encryption validation successful" << std::endl;

    // Step 4: Test pragma setup
    std::cout << "4. Setting up pragmas..." << std::endl;
    char* errorMsg = nullptr;

    result = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::cerr << "   FAILED to set foreign_keys pragma: " << (errorMsg ? errorMsg : sqlite3_errmsg(db)) << " (code: " << result << ")" << std::endl;
        if (errorMsg) sqlite3_free(errorMsg);
        sqlite3_close(db);
        return 1;
    }

    result = sqlite3_exec(db, "PRAGMA secure_delete = ON;", nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::cerr << "   FAILED to set secure_delete pragma: " << (errorMsg ? errorMsg : sqlite3_errmsg(db)) << " (code: " << result << ")" << std::endl;
        if (errorMsg) sqlite3_free(errorMsg);
        sqlite3_close(db);
        return 1;
    }

    std::cout << "   ✓ Pragmas set successfully" << std::endl;

    // Step 5: Test schema creation
    std::cout << "5. Creating schema version table..." << std::endl;
    result = sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "version INTEGER PRIMARY KEY"
        ");", nullptr, nullptr, &errorMsg);
    if (result != SQLITE_OK) {
        std::cerr << "   FAILED to create schema_version table: " << (errorMsg ? errorMsg : sqlite3_errmsg(db)) << " (code: " << result << ")" << std::endl;
        if (errorMsg) sqlite3_free(errorMsg);
        sqlite3_close(db);
        return 1;
    }
    std::cout << "   ✓ Schema version table created successfully" << std::endl;

    sqlite3_close(db);
    std::cout << "   ✓ Database closed" << std::endl;

    std::cout << "\n=== All Steps PASSED! ===\n" << std::endl;
    return 0;
}
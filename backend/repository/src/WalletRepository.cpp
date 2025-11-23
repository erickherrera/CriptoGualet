#include "Repository/WalletRepository.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

namespace Repository {

WalletRepository::WalletRepository(Database::DatabaseManager& dbManager) : m_dbManager(dbManager) {
    REPO_LOG_INFO(COMPONENT_NAME, "WalletRepository initialized");
}

Result<Wallet> WalletRepository::createWallet(int userId, const std::string& walletName,
                                             const std::string& walletType,
                                             const std::optional<std::string>& derivationPath,
                                             const std::optional<std::string>& extendedPublicKey) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "createWallet");

    // Validate input parameters
    auto nameValidation = validateWalletName(walletName);
    if (!nameValidation) {
        return Result<Wallet>(nameValidation.error(), 400);
    }

    auto typeValidation = validateWalletType(walletType);
    if (!typeValidation) {
        return Result<Wallet>(typeValidation.error(), 400);
    }

    // Check if wallet name already exists for this user
    auto existingWallet = getWalletByName(userId, walletName);
    if (existingWallet) {
        return Result<Wallet>("Wallet name already exists for this user", 409);
    }

    // Begin transaction
    auto transaction = m_dbManager.beginTransaction();
    if (!transaction) {
        REPO_LOG_ERROR(COMPONENT_NAME, "Failed to begin transaction", transaction.message);
        return Result<Wallet>("Database transaction error", 500);
    }

    try {
        // Insert wallet into database
        const std::string sql = R"(
            INSERT INTO wallets (user_id, wallet_name, wallet_type, derivation_path, extended_public_key, created_at, is_active)
            VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP, 1)
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            m_dbManager.rollbackTransaction();
            return Result<Wallet>("Failed to prepare wallet insertion statement", 500);
        }

        // Bind parameters
        sqlite3_bind_int(stmt, 1, userId);
        sqlite3_bind_text(stmt, 2, walletName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, walletType.c_str(), -1, SQLITE_STATIC);

        if (derivationPath.has_value()) {
            sqlite3_bind_text(stmt, 4, derivationPath->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 4);
        }

        if (extendedPublicKey.has_value()) {
            sqlite3_bind_text(stmt, 5, extendedPublicKey->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 5);
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            m_dbManager.rollbackTransaction();
            return Result<Wallet>("Failed to create wallet: database insertion failed", 500);
        }

        int walletId = static_cast<int>(sqlite3_last_insert_rowid(m_dbManager.getHandle()));
        sqlite3_finalize(stmt);

        // Generate initial receiving address
        auto addressResult = generateAddress(walletId, false, "Default receiving address");
        if (!addressResult) {
            m_dbManager.rollbackTransaction();
            REPO_LOG_ERROR(COMPONENT_NAME, "Failed to generate initial address", addressResult.error());
            return Result<Wallet>("Failed to generate initial wallet address", 500);
        }

        // Commit transaction
        auto commitResult = m_dbManager.commitTransaction();
        if (!commitResult) {
            REPO_LOG_ERROR(COMPONENT_NAME, "Failed to commit wallet creation", commitResult.message);
            return Result<Wallet>("Failed to commit wallet creation", 500);
        }

        // Retrieve and return created wallet
        auto walletResult = getWalletById(walletId);
        if (walletResult) {
            REPO_LOG_INFO(COMPONENT_NAME, "Wallet created successfully",
                         "Name: " + walletName + ", ID: " + std::to_string(walletId) + ", Type: " + walletType);
        }

        return walletResult;

    } catch (const std::exception& e) {
        m_dbManager.rollbackTransaction();
        REPO_LOG_ERROR(COMPONENT_NAME, "Exception during wallet creation", e.what());
        return Result<Wallet>("Internal error during wallet creation", 500);
    }
}

Result<Wallet> WalletRepository::getWalletById(int walletId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getWalletById");

    const std::string sql = R"(
        SELECT id, user_id, wallet_name, wallet_type, derivation_path, extended_public_key, created_at, is_active
        FROM wallets
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Wallet>("Failed to prepare wallet query", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        Wallet wallet = mapRowToWallet(stmt);
        sqlite3_finalize(stmt);
        return Result<Wallet>(wallet);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<Wallet>("Wallet not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<Wallet>("Database error while retrieving wallet", 500);
    }
}

Result<std::vector<Wallet>> WalletRepository::getWalletsByUserId(int userId, bool includeInactive) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getWalletsByUserId");

    std::string sql = R"(
        SELECT id, user_id, wallet_name, wallet_type, derivation_path, extended_public_key, created_at, is_active
        FROM wallets
        WHERE user_id = ?
    )";

    if (!includeInactive) {
        sql += " AND is_active = 1";
    }

    sql += " ORDER BY created_at ASC";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<Wallet>>("Failed to prepare wallets query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);

    std::vector<Wallet> wallets;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        wallets.push_back(mapRowToWallet(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<std::vector<Wallet>>(wallets);
    } else {
        return Result<std::vector<Wallet>>("Database error while retrieving wallets", 500);
    }
}

// PHASE 3: Get wallets by type for a user
Result<std::vector<Wallet>> WalletRepository::getWalletsByType(int userId, const std::string& walletType,
                                                                bool includeInactive) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getWalletsByType");

    // Validate wallet type
    auto validation = validateWalletType(walletType);
    if (!validation.success) {
        return Result<std::vector<Wallet>>(validation.errorMessage, validation.errorCode);
    }

    // Build SQL query with composite index (user_id, wallet_type)
    std::string sql = R"(
        SELECT id, user_id, wallet_name, wallet_type, derivation_path, extended_public_key, created_at, is_active
        FROM wallets
        WHERE user_id = ? AND wallet_type = ?
    )";

    if (!includeInactive) {
        sql += " AND is_active = 1";
    }

    sql += " ORDER BY created_at ASC";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<Wallet>>("Failed to prepare wallets by type query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_text(stmt, 2, walletType.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<Wallet> wallets;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        wallets.push_back(mapRowToWallet(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<std::vector<Wallet>>(wallets);
    } else {
        return Result<std::vector<Wallet>>("Database error while retrieving wallets by type", 500);
    }
}

Result<Wallet> WalletRepository::getWalletByName(int userId, const std::string& walletName) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getWalletByName");

    const std::string sql = R"(
        SELECT id, user_id, wallet_name, wallet_type, derivation_path, extended_public_key, created_at, is_active
        FROM wallets
        WHERE user_id = ? AND wallet_name = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Wallet>("Failed to prepare wallet query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_text(stmt, 2, walletName.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        Wallet wallet = mapRowToWallet(stmt);
        sqlite3_finalize(stmt);
        return Result<Wallet>(wallet);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<Wallet>("Wallet not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<Wallet>("Database error while retrieving wallet", 500);
    }
}

Result<Address> WalletRepository::generateAddress(int walletId, bool isChange,
                                                 const std::optional<std::string>& label) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "generateAddress");

    // Get next address index
    auto indexResult = getNextAddressIndex(walletId, isChange);
    if (!indexResult) {
        return Result<Address>(indexResult.error(), indexResult.errorCode);
    }

    int addressIndex = *indexResult;

    // Generate the actual address string
    std::string addressStr = generateBitcoinAddress(walletId, addressIndex, isChange);

    // Insert address into database
    const std::string sql = R"(
        INSERT INTO addresses (wallet_id, address, address_index, is_change, created_at, label, balance_satoshis)
        VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP, ?, 0)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Address>("Failed to prepare address insertion statement", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);
    sqlite3_bind_text(stmt, 2, addressStr.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, addressIndex);
    sqlite3_bind_int(stmt, 4, isChange ? 1 : 0);

    if (label.has_value()) {
        sqlite3_bind_text(stmt, 5, label->c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 5);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<Address>("Failed to insert address", 500);
    }

    int addressId = static_cast<int>(sqlite3_last_insert_rowid(m_dbManager.getHandle()));
    sqlite3_finalize(stmt);

    // Retrieve and return the created address
    const std::string retrieveSql = R"(
        SELECT id, wallet_id, address, address_index, is_change, public_key, created_at, label, balance_satoshis
        FROM addresses
        WHERE id = ?
    )";

    rc = sqlite3_prepare_v2(m_dbManager.getHandle(), retrieveSql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Address>("Failed to prepare address retrieval statement", 500);
    }

    sqlite3_bind_int(stmt, 1, addressId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        Address address = mapRowToAddress(stmt);
        sqlite3_finalize(stmt);

        REPO_LOG_INFO(COMPONENT_NAME, "Address generated successfully",
                     "Address: " + addressStr + ", Index: " + std::to_string(addressIndex) +
                     ", IsChange: " + (isChange ? "true" : "false"));

        return Result<Address>(address);
    } else {
        sqlite3_finalize(stmt);
        return Result<Address>("Failed to retrieve created address", 500);
    }
}

Result<std::vector<Address>> WalletRepository::getAddressesByWallet(int walletId,
                                                                   const std::optional<bool>& isChange) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getAddressesByWallet");

    std::string sql = R"(
        SELECT id, wallet_id, address, address_index, is_change, public_key, created_at, label, balance_satoshis
        FROM addresses
        WHERE wallet_id = ?
    )";

    if (isChange.has_value()) {
        sql += " AND is_change = ?";
    }

    sql += " ORDER BY address_index ASC";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<Address>>("Failed to prepare addresses query", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);

    if (isChange.has_value()) {
        sqlite3_bind_int(stmt, 2, *isChange ? 1 : 0);
    }

    std::vector<Address> addresses;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        addresses.push_back(mapRowToAddress(stmt));
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<std::vector<Address>>(addresses);
    } else {
        return Result<std::vector<Address>>("Database error while retrieving addresses", 500);
    }
}

Result<WalletSummary> WalletRepository::getWalletSummary(int walletId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getWalletSummary");

    // Get wallet information
    auto walletResult = getWalletById(walletId);
    if (!walletResult) {
        return Result<WalletSummary>(walletResult.error(), walletResult.errorCode);
    }

    WalletSummary summary;
    summary.wallet = *walletResult;

    // Get wallet statistics
    const std::string sql = R"(
        SELECT
            COUNT(DISTINCT a.id) as address_count,
            COUNT(CASE WHEN a.balance_satoshis > 0 THEN 1 END) as used_address_count,
            COALESCE(SUM(a.balance_satoshis), 0) as total_balance,
            COUNT(DISTINCT t.id) as transaction_count,
            MAX(t.created_at) as last_transaction_date
        FROM addresses a
        LEFT JOIN transactions t ON a.wallet_id = t.wallet_id
        WHERE a.wallet_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<WalletSummary>("Failed to prepare wallet summary query", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        summary.addressCount = sqlite3_column_int(stmt, 0);
        summary.usedAddressCount = sqlite3_column_int(stmt, 1);
        summary.totalBalanceSatoshis = sqlite3_column_int64(stmt, 2);
        summary.transactionCount = sqlite3_column_int(stmt, 3);

        const char* lastTxDateStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (lastTxDateStr && strlen(lastTxDateStr) > 0) {
            summary.lastTransactionDate = std::chrono::system_clock::now(); // Simplified
        }

        sqlite3_finalize(stmt);
        return Result<WalletSummary>(summary);
    } else {
        sqlite3_finalize(stmt);
        return Result<WalletSummary>("Database error while retrieving wallet summary", 500);
    }
}

Result<bool> WalletRepository::storeEncryptedSeed(int userId, const std::string& password,
                                                 const std::vector<std::string>& mnemonic) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "storeEncryptedSeed");

    // Encrypt the seed using our crypto functions
    Crypto::EncryptedSeed encryptedSeed;
    if (!Crypto::EncryptSeedPhrase(password, mnemonic, encryptedSeed)) {
        REPO_LOG_ERROR(COMPONENT_NAME, "Failed to encrypt seed phrase");
        return Result<bool>("Failed to encrypt seed phrase", 500);
    }

    // Store in database
    const std::string sql = R"(
        INSERT OR REPLACE INTO encrypted_seeds
        (user_id, encrypted_seed, encryption_salt, verification_hash, key_derivation_iterations, created_at, backup_confirmed)
        VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP, 0)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare encrypted seed insertion statement", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);
    sqlite3_bind_blob(stmt, 2, encryptedSeed.encrypted_data.data(),
                     static_cast<int>(encryptedSeed.encrypted_data.size()), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, encryptedSeed.salt.data(),
                     static_cast<int>(encryptedSeed.salt.size()), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 4, encryptedSeed.verification_hash.data(),
                     static_cast<int>(encryptedSeed.verification_hash.size()), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, 600000); // PBKDF2 iterations

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        REPO_LOG_INFO(COMPONENT_NAME, "Encrypted seed stored successfully", "UserID: " + std::to_string(userId));
        return Result<bool>(true);
    } else {
        return Result<bool>("Failed to store encrypted seed", 500);
    }
}

Result<std::vector<std::string>> WalletRepository::retrieveDecryptedSeed(int userId, const std::string& password) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "retrieveDecryptedSeed");

    // Retrieve encrypted seed from database
    const std::string sql = R"(
        SELECT encrypted_seed, encryption_salt, verification_hash
        FROM encrypted_seeds
        WHERE user_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<std::string>>("Failed to prepare encrypted seed query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) {
            return Result<std::vector<std::string>>("No encrypted seed found for user", 404);
        } else {
            return Result<std::vector<std::string>>("Database error while retrieving seed", 500);
        }
    }

    // Extract encrypted data
    Crypto::EncryptedSeed encryptedSeed;

    const void* encryptedData = sqlite3_column_blob(stmt, 0);
    int encryptedSize = sqlite3_column_bytes(stmt, 0);
    if (encryptedData && encryptedSize > 0) {
        encryptedSeed.encrypted_data.assign(static_cast<const uint8_t*>(encryptedData),
                                           static_cast<const uint8_t*>(encryptedData) + encryptedSize);
    }

    const void* saltData = sqlite3_column_blob(stmt, 1);
    int saltSize = sqlite3_column_bytes(stmt, 1);
    if (saltData && saltSize > 0) {
        encryptedSeed.salt.assign(static_cast<const uint8_t*>(saltData),
                                 static_cast<const uint8_t*>(saltData) + saltSize);
    }

    const void* hashData = sqlite3_column_blob(stmt, 2);
    int hashSize = sqlite3_column_bytes(stmt, 2);
    if (hashData && hashSize > 0) {
        encryptedSeed.verification_hash.assign(static_cast<const uint8_t*>(hashData),
                                              static_cast<const uint8_t*>(hashData) + hashSize);
    }

    sqlite3_finalize(stmt);

    // Decrypt the seed
    std::vector<std::string> mnemonic;
    if (!Crypto::DecryptSeedPhrase(password, encryptedSeed, mnemonic)) {
        REPO_LOG_WARNING(COMPONENT_NAME, "Failed to decrypt seed phrase", "UserID: " + std::to_string(userId));
        return Result<std::vector<std::string>>("Failed to decrypt seed phrase - invalid password", 401);
    }

    REPO_LOG_INFO(COMPONENT_NAME, "Seed phrase decrypted successfully", "UserID: " + std::to_string(userId));
    return Result<std::vector<std::string>>(mnemonic);
}

Wallet WalletRepository::mapRowToWallet(sqlite3_stmt* stmt) {
    Wallet wallet;
    wallet.id = sqlite3_column_int(stmt, 0);
    wallet.userId = sqlite3_column_int(stmt, 1);
    wallet.walletName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    wallet.walletType = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

    const char* derivationPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    if (derivationPath && strlen(derivationPath) > 0) {
        wallet.derivationPath = derivationPath;
    }

    const char* extendedPublicKey = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (extendedPublicKey && strlen(extendedPublicKey) > 0) {
        wallet.extendedPublicKey = extendedPublicKey;
    }

    // Parse timestamp (simplified)
    wallet.createdAt = std::chrono::system_clock::now();
    wallet.isActive = sqlite3_column_int(stmt, 7) != 0;

    return wallet;
}

Address WalletRepository::mapRowToAddress(sqlite3_stmt* stmt) {
    Address address;
    address.id = sqlite3_column_int(stmt, 0);
    address.walletId = sqlite3_column_int(stmt, 1);
    address.address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    address.addressIndex = sqlite3_column_int(stmt, 3);
    address.isChange = sqlite3_column_int(stmt, 4) != 0;

    const char* publicKey = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    if (publicKey && strlen(publicKey) > 0) {
        address.publicKey = publicKey;
    }

    address.createdAt = std::chrono::system_clock::now(); // Simplified

    const char* label = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    if (label && strlen(label) > 0) {
        address.label = label;
    }

    address.balanceSatoshis = sqlite3_column_int64(stmt, 8);

    return address;
}

Result<bool> WalletRepository::validateWalletName(const std::string& walletName) {
    if (walletName.length() < MIN_WALLET_NAME_LENGTH) {
        return Result<bool>("Wallet name cannot be empty", 400);
    }

    if (walletName.length() > MAX_WALLET_NAME_LENGTH) {
        return Result<bool>("Wallet name too long", 400);
    }

    return Result<bool>(true);
}

Result<bool> WalletRepository::validateWalletType(const std::string& walletType) {
    static const std::set<std::string> validTypes = {
        "bitcoin", "bitcoin_testnet", "litecoin", "ethereum"
    };

    if (validTypes.find(walletType) == validTypes.end()) {
        return Result<bool>("Invalid wallet type", 400);
    }

    return Result<bool>(true);
}

Result<int> WalletRepository::getNextAddressIndex(int walletId, bool isChange) {
    const std::string sql = R"(
        SELECT COALESCE(MAX(address_index), -1) + 1
        FROM addresses
        WHERE wallet_id = ? AND is_change = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<int>("Failed to prepare address index query", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);
    sqlite3_bind_int(stmt, 2, isChange ? 1 : 0);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int nextIndex = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return Result<int>(nextIndex);
    } else {
        sqlite3_finalize(stmt);
        return Result<int>("Database error while getting next address index", 500);
    }
}

std::string WalletRepository::generateBitcoinAddress(int walletId, int addressIndex, bool isChange) {
    // Simplified address generation - in production would use proper BIP44 derivation
    std::ostringstream oss;
    oss << "bc1q" << std::hex << walletId << addressIndex << (isChange ? "c" : "r");
    return oss.str();
}

int64_t WalletRepository::calculateWalletBalance(int walletId) {
    const std::string sql = R"(
        SELECT COALESCE(SUM(balance_satoshis), 0)
        FROM addresses
        WHERE wallet_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_int(stmt, 1, walletId);

    int64_t balance = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        balance = sqlite3_column_int64(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return balance;
}

// === Additional Wallet Management Methods ===

Result<bool> WalletRepository::updateWallet(int walletId,
                                           const std::optional<std::string>& walletName,
                                           const std::optional<std::string>& derivationPath,
                                           const std::optional<std::string>& extendedPublicKey) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "updateWallet");

    std::string sql = "UPDATE wallets SET ";
    std::vector<std::string> updates;

    if (walletName.has_value()) {
        auto nameValidation = validateWalletName(*walletName);
        if (!nameValidation) {
            return nameValidation;
        }
        updates.push_back("wallet_name = ?");
    }

    if (derivationPath.has_value()) {
        updates.push_back("derivation_path = ?");
    }

    if (extendedPublicKey.has_value()) {
        updates.push_back("extended_public_key = ?");
    }

    if (updates.empty()) {
        return Result<bool>("No fields to update", 400);
    }

    for (size_t i = 0; i < updates.size(); ++i) {
        if (i > 0) sql += ", ";
        sql += updates[i];
    }
    sql += " WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare wallet update statement", 500);
    }

    int paramIndex = 1;
    if (walletName.has_value()) {
        sqlite3_bind_text(stmt, paramIndex++, walletName->c_str(), -1, SQLITE_STATIC);
    }
    if (derivationPath.has_value()) {
        sqlite3_bind_text(stmt, paramIndex++, derivationPath->c_str(), -1, SQLITE_STATIC);
    }
    if (extendedPublicKey.has_value()) {
        sqlite3_bind_text(stmt, paramIndex++, extendedPublicKey->c_str(), -1, SQLITE_STATIC);
    }
    sqlite3_bind_int(stmt, paramIndex, walletId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        REPO_LOG_INFO(COMPONENT_NAME, "Wallet updated successfully", "WalletID: " + std::to_string(walletId));
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during wallet update", 500);
    }
}

Result<bool> WalletRepository::setWalletActive(int walletId, bool isActive) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "setWalletActive");

    const std::string sql = "UPDATE wallets SET is_active = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare wallet status update", 500);
    }

    sqlite3_bind_int(stmt, 1, isActive ? 1 : 0);
    sqlite3_bind_int(stmt, 2, walletId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during wallet status update", 500);
    }
}

Result<bool> WalletRepository::deleteWallet(int walletId) {
    return setWalletActive(walletId, false);
}

Result<Address> WalletRepository::getAddressByString(const std::string& addressStr) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getAddressByString");

    const std::string sql = R"(
        SELECT id, wallet_id, address, address_index, is_change, public_key, created_at, label, balance_satoshis
        FROM addresses
        WHERE address = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<Address>("Failed to prepare address query", 500);
    }

    sqlite3_bind_text(stmt, 1, addressStr.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        Address address = mapRowToAddress(stmt);
        sqlite3_finalize(stmt);
        return Result<Address>(address);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<Address>("Address not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<Address>("Database error while retrieving address", 500);
    }
}

Result<bool> WalletRepository::updateAddressLabel(int addressId, const std::string& label) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "updateAddressLabel");

    const std::string sql = "UPDATE addresses SET label = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare address label update", 500);
    }

    sqlite3_bind_text(stmt, 1, label.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, addressId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during address label update", 500);
    }
}

Result<bool> WalletRepository::updateAddressBalance(int addressId, int64_t balanceSatoshis) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "updateAddressBalance");

    const std::string sql = "UPDATE addresses SET balance_satoshis = ? WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare address balance update", 500);
    }

    sqlite3_bind_int64(stmt, 1, balanceSatoshis);
    sqlite3_bind_int(stmt, 2, addressId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during address balance update", 500);
    }
}

Result<bool> WalletRepository::confirmSeedBackup(int userId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "confirmSeedBackup");

    const std::string sql = "UPDATE encrypted_seeds SET backup_confirmed = 1 WHERE user_id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare seed backup confirmation", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        REPO_LOG_INFO(COMPONENT_NAME, "Seed backup confirmed", "UserID: " + std::to_string(userId));
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during seed backup confirmation", 500);
    }
}

Result<bool> WalletRepository::hasSeedStored(int userId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "hasSeedStored");

    const std::string sql = "SELECT COUNT(*) FROM encrypted_seeds WHERE user_id = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare seed check query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return Result<bool>(count > 0);
    } else {
        sqlite3_finalize(stmt);
        return Result<bool>("Database error while checking for seed", 500);
    }
}

Result<WalletStats> WalletRepository::getWalletStats(int userId) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getWalletStats");

    WalletStats stats;

    // Get wallet counts and balances
    const std::string sql = R"(
        SELECT
            COUNT(*) as total_wallets,
            COUNT(CASE WHEN is_active = 1 THEN 1 END) as active_wallets
        FROM wallets
        WHERE user_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<WalletStats>("Failed to prepare wallet stats query", 500);
    }

    sqlite3_bind_int(stmt, 1, userId);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        stats.totalWallets = sqlite3_column_int(stmt, 0);
        stats.activeWallets = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    // Check if seed backup is confirmed
    const std::string seedSql = "SELECT backup_confirmed FROM encrypted_seeds WHERE user_id = ?";
    rc = sqlite3_prepare_v2(m_dbManager.getHandle(), seedSql.c_str(), -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, userId);
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            stats.hasSeedBackup = sqlite3_column_int(stmt, 0) != 0;
        }
        sqlite3_finalize(stmt);
    }

    return Result<WalletStats>(stats);
}

// === UTXO Management Methods ===

Result<UTXO> WalletRepository::addUTXO(int walletId, int addressId, const std::string& txid,
                                       uint32_t vout, int64_t amountSatoshis,
                                       const std::string& scriptPubKey, uint32_t confirmations,
                                       const std::optional<int>& blockHeight) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "addUTXO");

    // First check if UTXO already exists
    auto existsResult = utxoExists(txid, vout);
    if (existsResult && *existsResult) {
        return Result<UTXO>("UTXO already exists", 409);
    }

    // Get address string for the UTXO
    auto addressResult = getAddressByString("");  // We need address by ID, simplified here
    std::string address = "";

    const std::string sql = R"(
        INSERT INTO transaction_outputs
        (transaction_id, output_index, script_pubkey, address, amount_satoshis, is_spent, spent_in_txid)
        VALUES (0, ?, ?, ?, ?, 0, NULL)
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<UTXO>("Failed to prepare UTXO insertion", 500);
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(vout));
    sqlite3_bind_text(stmt, 2, scriptPubKey.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, amountSatoshis);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        UTXO utxo;
        utxo.walletId = walletId;
        utxo.addressId = addressId;
        utxo.txid = txid;
        utxo.vout = vout;
        utxo.amountSatoshis = amountSatoshis;
        utxo.scriptPubKey = scriptPubKey;
        utxo.confirmations = confirmations;
        utxo.blockHeight = blockHeight;
        utxo.isSpent = false;
        utxo.createdAt = std::chrono::system_clock::now();

        REPO_LOG_INFO(COMPONENT_NAME, "UTXO added successfully",
                     "TXID: " + txid + ", Vout: " + std::to_string(vout));
        return Result<UTXO>(utxo);
    } else {
        return Result<UTXO>("Failed to insert UTXO", 500);
    }
}

Result<std::vector<UTXO>> WalletRepository::getUnspentUTXOs(int walletId, uint32_t minConfirmations) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getUnspentUTXOs");

    const std::string sql = R"(
        SELECT txout.id, txout.transaction_id, txout.output_index, t.txid, txout.address,
               txout.amount_satoshis, t.confirmation_count, t.is_confirmed
        FROM transaction_outputs txout
        JOIN transactions t ON txout.transaction_id = t.id
        WHERE t.wallet_id = ?
          AND txout.is_spent = 0
          AND t.confirmation_count >= ?
        ORDER BY t.created_at DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<UTXO>>("Failed to prepare UTXO query", 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);
    sqlite3_bind_int(stmt, 2, static_cast<int>(minConfirmations));

    std::vector<UTXO> utxos;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        UTXO utxo = mapRowToUTXO(stmt);
        utxos.push_back(utxo);
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<std::vector<UTXO>>(utxos);
    } else {
        return Result<std::vector<UTXO>>("Database error while retrieving UTXOs", 500);
    }
}

Result<std::vector<UTXO>> WalletRepository::getUnspentUTXOsByAddress(int addressId, uint32_t minConfirmations) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getUnspentUTXOsByAddress");

    // Get address string first
    const std::string addressSql = "SELECT address FROM addresses WHERE id = ?";
    sqlite3_stmt* addrStmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), addressSql.c_str(), -1, &addrStmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<UTXO>>("Failed to query address", 500);
    }

    sqlite3_bind_int(addrStmt, 1, addressId);
    rc = sqlite3_step(addrStmt);

    std::string addressStr;
    if (rc == SQLITE_ROW) {
        addressStr = reinterpret_cast<const char*>(sqlite3_column_text(addrStmt, 0));
    }
    sqlite3_finalize(addrStmt);

    if (addressStr.empty()) {
        return Result<std::vector<UTXO>>("Address not found", 404);
    }

    // Now get UTXOs for this address
    const std::string sql = R"(
        SELECT txout.id, txout.transaction_id, txout.output_index, t.txid, txout.address,
               txout.amount_satoshis, t.confirmation_count, t.is_confirmed
        FROM transaction_outputs txout
        JOIN transactions t ON txout.transaction_id = t.id
        WHERE txout.address = ?
          AND txout.is_spent = 0
          AND t.confirmation_count >= ?
        ORDER BY t.created_at DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<std::vector<UTXO>>("Failed to prepare UTXO query", 500);
    }

    sqlite3_bind_text(stmt, 1, addressStr.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, static_cast<int>(minConfirmations));

    std::vector<UTXO> utxos;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        UTXO utxo = mapRowToUTXO(stmt);
        utxos.push_back(utxo);
    }

    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<std::vector<UTXO>>(utxos);
    } else {
        return Result<std::vector<UTXO>>("Database error while retrieving UTXOs", 500);
    }
}

Result<bool> WalletRepository::markUTXOAsSpent(int utxoId, const std::string& spentInTxid) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "markUTXOAsSpent");

    const std::string sql = R"(
        UPDATE transaction_outputs
        SET is_spent = 1, spent_in_txid = ?
        WHERE id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare UTXO spent update", 500);
    }

    sqlite3_bind_text(stmt, 1, spentInTxid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, utxoId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        REPO_LOG_INFO(COMPONENT_NAME, "UTXO marked as spent", "UtxoID: " + std::to_string(utxoId));
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during UTXO spent update", 500);
    }
}

Result<bool> WalletRepository::markUTXOsAsSpent(const std::vector<int>& utxoIds, const std::string& spentInTxid) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "markUTXOsAsSpent");

    if (utxoIds.empty()) {
        return Result<bool>("No UTXOs provided", 400);
    }

    auto transaction = m_dbManager.beginTransaction();
    if (!transaction) {
        return Result<bool>("Failed to begin transaction", 500);
    }

    try {
        for (int utxoId : utxoIds) {
            auto result = markUTXOAsSpent(utxoId, spentInTxid);
            if (!result) {
                m_dbManager.rollbackTransaction();
                return result;
            }
        }

        auto commitResult = m_dbManager.commitTransaction();
        if (!commitResult) {
            return Result<bool>("Failed to commit transaction", 500);
        }

        REPO_LOG_INFO(COMPONENT_NAME, "Multiple UTXOs marked as spent",
                     "Count: " + std::to_string(utxoIds.size()));
        return Result<bool>(true);

    } catch (const std::exception& e) {
        m_dbManager.rollbackTransaction();
        return Result<bool>("Exception during bulk UTXO update", 500);
    }
}

Result<bool> WalletRepository::updateUTXOConfirmations(const std::string& txid, uint32_t confirmations) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "updateUTXOConfirmations");

    const std::string sql = "UPDATE transactions SET confirmation_count = ? WHERE txid = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare confirmation update", 500);
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(confirmations));
    sqlite3_bind_text(stmt, 2, txid.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return Result<bool>(true);
    } else {
        return Result<bool>("Database error during confirmation update", 500);
    }
}

Result<int64_t> WalletRepository::getSpendableBalance(int walletId, uint32_t minConfirmations) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getSpendableBalance");

    const std::string sql = R"(
        SELECT COALESCE(SUM(txout.amount_satoshis), 0)
        FROM transaction_outputs txout
        JOIN transactions t ON txout.transaction_id = t.id
        WHERE t.wallet_id = ?
          AND txout.is_spent = 0
          AND t.confirmation_count >= ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::string error = "Failed to prepare balance query: ";
        if (m_dbManager.getHandle()) {
            error += sqlite3_errmsg(m_dbManager.getHandle());
        } else {
            error += "Database handle is null";
        }
        return Result<int64_t>(error, 500);
    }

    sqlite3_bind_int(stmt, 1, walletId);
    sqlite3_bind_int(stmt, 2, static_cast<int>(minConfirmations));

    int64_t balance = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        balance = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return Result<int64_t>(balance);
}

Result<bool> WalletRepository::utxoExists(const std::string& txid, uint32_t vout) {
    const std::string sql = R"(
        SELECT COUNT(*)
        FROM transaction_outputs txout
        JOIN transactions t ON txout.transaction_id = t.id
        WHERE t.txid = ? AND txout.output_index = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<bool>("Failed to prepare UTXO existence check", 500);
    }

    sqlite3_bind_text(stmt, 1, txid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, static_cast<int>(vout));

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return Result<bool>(count > 0);
    } else {
        sqlite3_finalize(stmt);
        return Result<bool>("Database error during UTXO existence check", 500);
    }
}

Result<UTXO> WalletRepository::getUTXOByTxidVout(const std::string& txid, uint32_t vout) {
    REPO_SCOPED_LOG(COMPONENT_NAME, "getUTXOByTxidVout");

    const std::string sql = R"(
        SELECT txout.id, txout.transaction_id, txout.output_index, t.txid, txout.address,
               txout.amount_satoshis, t.confirmation_count, t.is_confirmed
        FROM transaction_outputs txout
        JOIN transactions t ON txout.transaction_id = t.id
        WHERE t.txid = ? AND txout.output_index = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return Result<UTXO>("Failed to prepare UTXO query", 500);
    }

    sqlite3_bind_text(stmt, 1, txid.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, static_cast<int>(vout));

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        UTXO utxo = mapRowToUTXO(stmt);
        sqlite3_finalize(stmt);
        return Result<UTXO>(utxo);
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Result<UTXO>("UTXO not found", 404);
    } else {
        sqlite3_finalize(stmt);
        return Result<UTXO>("Database error while retrieving UTXO", 500);
    }
}

UTXO WalletRepository::mapRowToUTXO(sqlite3_stmt* stmt) {
    UTXO utxo;
    utxo.id = sqlite3_column_int(stmt, 0);
    // Column 1 is transaction_id from DB, we don't store it directly in UTXO
    utxo.vout = static_cast<uint32_t>(sqlite3_column_int(stmt, 2));
    utxo.txid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    utxo.address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    utxo.amountSatoshis = sqlite3_column_int64(stmt, 5);
    utxo.confirmations = static_cast<uint32_t>(sqlite3_column_int(stmt, 6));
    // Column 7 is is_confirmed boolean - we use it to derive isSpent
    bool isConfirmed = sqlite3_column_int(stmt, 7) != 0;
    utxo.isSpent = false;  // We only query unspent UTXOs, so this is always false
    utxo.createdAt = std::chrono::system_clock::now(); // Simplified
    return utxo;
}

} // namespace Repository

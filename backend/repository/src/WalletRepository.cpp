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

} // namespace Repository

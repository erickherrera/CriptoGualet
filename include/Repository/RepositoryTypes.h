#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <cstdint>

namespace Repository {

/**
 * @brief Result wrapper for repository operations
 */
template<typename T>
struct Result {
    bool success;
    std::string errorMessage;
    T data;
    int errorCode;

    Result() : success(false), errorCode(0) {}
    Result(const T& value) : success(true), data(value), errorCode(0) {}
    Result(const std::string& error, int code = 0)
        : success(false), errorMessage(error), errorCode(code) {}

    operator bool() const { return success; }
    const T& operator*() const { return data; }
    T& operator*() { return data; }
    const T* operator->() const { return &data; }
    T* operator->() { return &data; }

    bool hasValue() const { return success; }
    const std::string& error() const { return errorMessage; }
};

/**
 * @brief Log levels for repository operations
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

/**
 * @brief Log entry structure
 */
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string component;
    std::string message;
    std::string details;

    LogEntry(LogLevel lvl, const std::string& comp, const std::string& msg, const std::string& det = "")
        : timestamp(std::chrono::system_clock::now()), level(lvl), component(comp), message(msg), details(det) {}
};

/**
 * @brief User entity structure
 */
struct User {
    int id;
    std::string username;
    std::string email;
    std::string passwordHash;
    std::vector<uint8_t> salt;
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> lastLogin;
    int walletVersion;
    bool isActive;

    User() : id(0), walletVersion(1), isActive(true) {}
};

/**
 * @brief Wallet entity structure
 */
struct Wallet {
    int id;
    int userId;
    std::string walletName;
    std::string walletType;
    std::optional<std::string> derivationPath;
    std::optional<std::string> extendedPublicKey;
    std::chrono::system_clock::time_point createdAt;
    bool isActive;

    Wallet() : id(0), userId(0), walletType("bitcoin"), isActive(true) {}
};

/**
 * @brief Address entity structure
 */
struct Address {
    int id;
    int walletId;
    std::string address;
    int addressIndex;
    bool isChange;
    std::optional<std::string> publicKey;
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::string> label;
    int64_t balanceSatoshis;

    Address() : id(0), walletId(0), addressIndex(0), isChange(false), balanceSatoshis(0) {}
};

/**
 * @brief Transaction entity structure
 */
struct Transaction {
    int id;
    int walletId;
    std::string txid;
    std::optional<int> blockHeight;
    std::optional<std::string> blockHash;
    int64_t amountSatoshis;
    int64_t feeSatoshis;
    std::string direction; // 'incoming', 'outgoing', 'internal'
    std::optional<std::string> fromAddress;
    std::optional<std::string> toAddress;
    int confirmationCount;
    bool isConfirmed;
    std::chrono::system_clock::time_point createdAt;
    std::optional<std::chrono::system_clock::time_point> confirmedAt;
    std::optional<std::string> memo;

    Transaction() : id(0), walletId(0), amountSatoshis(0), feeSatoshis(0),
                   direction("incoming"), confirmationCount(0), isConfirmed(false) {}
};

/**
 * @brief Encrypted seed entity structure
 */
struct EncryptedSeed {
    int id;
    int userId;
    std::vector<uint8_t> encryptedSeed;
    std::vector<uint8_t> encryptionSalt;
    int keyDerivationIterations;
    std::chrono::system_clock::time_point createdAt;
    bool backupConfirmed;

    EncryptedSeed() : id(0), userId(0), keyDerivationIterations(100000), backupConfirmed(false) {}
};

/**
 * @brief Pagination parameters
 */
struct PaginationParams {
    int offset;
    int limit;
    std::string sortField;
    bool ascending;

    PaginationParams(int off = 0, int lim = 50, const std::string& sort = "id", bool asc = true)
        : offset(off), limit(lim), sortField(sort), ascending(asc) {}
};

/**
 * @brief Paginated result wrapper
 */
template<typename T>
struct PaginatedResult {
    std::vector<T> items;
    int totalCount;
    int offset;
    int limit;
    bool hasMore;

    PaginatedResult() : totalCount(0), offset(0), limit(0), hasMore(false) {}
    PaginatedResult(const std::vector<T>& data, int total, int off, int lim)
        : items(data), totalCount(total), offset(off), limit(lim), hasMore(off + lim < total) {}
};

/**
 * @brief User statistics
 */
struct UserStats {
    int totalLogins;
    std::optional<std::chrono::system_clock::time_point> lastLogin;
    std::optional<std::chrono::system_clock::time_point> accountCreated;
    bool isActive;

    UserStats() : totalLogins(0), isActive(true) {}
};

/**
 * @brief Password validation result
 */
struct PasswordValidation {
    bool isValid;
    std::string errorMessage;
    std::vector<std::string> requirements;
    std::vector<std::string> violations;
    int strengthScore; // 0-100

    PasswordValidation() : isValid(false), strengthScore(0) {}
    PasswordValidation(bool valid, const std::string& message = "")
        : isValid(valid), errorMessage(message), strengthScore(0) {}
};

/**
 * @brief Password hash result
 */
struct PasswordHashResult {
    std::string hash;
    std::vector<uint8_t> salt;

    PasswordHashResult() = default;
    PasswordHashResult(const std::string& h, const std::vector<uint8_t>& s)
        : hash(h), salt(s) {}
};

} // namespace Repository

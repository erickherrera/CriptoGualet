#include "../include/Repository/SettingsRepository.h"

extern "C" {
#ifdef SQLCIPHER_AVAILABLE
#include <sqlcipher/sqlite3.h>
#else
#include <sqlite3.h>
#endif
}

namespace Repository {

SettingsRepository::SettingsRepository(Database::DatabaseManager &dbManager)
    : m_dbManager(dbManager) {
  REPO_LOG_INFO(COMPONENT_NAME, "SettingsRepository initialized");
}

Result<std::optional<std::string>>
SettingsRepository::getUserSetting(int userId, const std::string &key) {
  REPO_SCOPED_LOG(COMPONENT_NAME, "getUserSetting");

  const std::string sql = R"(
        SELECT setting_value
        FROM user_settings
        WHERE user_id = ? AND setting_key = ?
    )";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt,
                              nullptr);
  if (rc != SQLITE_OK) {
    return Result<std::optional<std::string>>(
        "Failed to prepare settings query", 500);
  }

  sqlite3_bind_int(stmt, 1, userId);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    const unsigned char *value = sqlite3_column_text(stmt, 0);
    std::optional<std::string> result;
    if (value) {
      result = std::string(reinterpret_cast<const char *>(value));
    }
    sqlite3_finalize(stmt);
    return Result<std::optional<std::string>>(result);
  }

  sqlite3_finalize(stmt);
  if (rc == SQLITE_DONE) {
    return Result<std::optional<std::string>>(std::optional<std::string>());
  }

  return Result<std::optional<std::string>>(
      "Database error while retrieving setting", 500);
}

Result<std::map<std::string, std::string>> SettingsRepository::getUserSettings(
    int userId, const std::vector<std::string> &keys) {
  REPO_SCOPED_LOG(COMPONENT_NAME, "getUserSettings");

  std::map<std::string, std::string> results;
  if (keys.empty()) {
    return Result<std::map<std::string, std::string>>(results);
  }

  std::string placeholders;
  placeholders.reserve(keys.size() * 2);
  for (size_t i = 0; i < keys.size(); ++i) {
    if (i > 0) {
      placeholders += ",";
    }
    placeholders += "?";
  }

  std::string sql =
      "SELECT setting_key, setting_value FROM user_settings WHERE user_id = ? "
      "AND setting_key IN (" +
      placeholders + ")";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt,
                              nullptr);
  if (rc != SQLITE_OK) {
    return Result<std::map<std::string, std::string>>(
        "Failed to prepare settings query", 500);
  }

  sqlite3_bind_int(stmt, 1, userId);
  int paramIndex = 2;
  for (const auto &key : keys) {
    sqlite3_bind_text(stmt, paramIndex++, key.c_str(), -1, SQLITE_STATIC);
  }

  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
    const unsigned char *keyText = sqlite3_column_text(stmt, 0);
    const unsigned char *valueText = sqlite3_column_text(stmt, 1);
    if (keyText && valueText) {
      results[reinterpret_cast<const char *>(keyText)] =
          reinterpret_cast<const char *>(valueText);
    }
  }

  sqlite3_finalize(stmt);

  if (rc == SQLITE_DONE) {
    return Result<std::map<std::string, std::string>>(results);
  }

  return Result<std::map<std::string, std::string>>(
      "Database error while retrieving settings", 500);
}

Result<bool> SettingsRepository::setUserSetting(int userId,
                                                const std::string &key,
                                                const std::string &value) {
  REPO_SCOPED_LOG(COMPONENT_NAME, "setUserSetting");

  const std::string sql = R"(
        INSERT INTO user_settings (user_id, setting_key, setting_value, updated_at)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        ON CONFLICT(user_id, setting_key)
        DO UPDATE SET setting_value = excluded.setting_value,
                      updated_at = CURRENT_TIMESTAMP
    )";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt,
                              nullptr);
  if (rc != SQLITE_OK) {
    return Result<bool>("Failed to prepare settings upsert", 500);
  }

  sqlite3_bind_int(stmt, 1, userId);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, value.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc == SQLITE_DONE) {
    return Result<bool>(true);
  }

  return Result<bool>("Database error while updating setting", 500);
}

Result<bool> SettingsRepository::deleteUserSetting(int userId,
                                                   const std::string &key) {
  REPO_SCOPED_LOG(COMPONENT_NAME, "deleteUserSetting");

  const std::string sql =
      "DELETE FROM user_settings WHERE user_id = ? AND setting_key = ?";

  sqlite3_stmt *stmt = nullptr;
  int rc = sqlite3_prepare_v2(m_dbManager.getHandle(), sql.c_str(), -1, &stmt,
                              nullptr);
  if (rc != SQLITE_OK) {
    return Result<bool>("Failed to prepare settings delete", 500);
  }

  sqlite3_bind_int(stmt, 1, userId);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_STATIC);

  rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  if (rc == SQLITE_DONE) {
    return Result<bool>(true);
  }

  return Result<bool>("Database error while deleting setting", 500);
}

} // namespace Repository

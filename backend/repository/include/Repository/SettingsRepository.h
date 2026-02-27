#pragma once

#include "../../database/include/Database/DatabaseManager.h"
#include "Logger.h"
#include "RepositoryTypes.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Repository {

class SettingsRepository {
  public:
    explicit SettingsRepository(Database::DatabaseManager& dbManager);

    Result<std::optional<std::string>> getUserSetting(int userId, const std::string& key);
    Result<std::map<std::string, std::string>> getUserSettings(
        int userId, const std::vector<std::string>& keys);
    Result<bool> setUserSetting(int userId, const std::string& key, const std::string& value);
    Result<bool> deleteUserSetting(int userId, const std::string& key);

  private:
    Database::DatabaseManager& m_dbManager;
    static constexpr const char* COMPONENT_NAME = "SettingsRepository";
};

}  // namespace Repository

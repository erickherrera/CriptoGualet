#pragma once

#include "../../../database/include/Database/DatabaseManager.h"
#include "../../include/Repository/RepositoryTypes.h"
#include <optional>
#include <string>
#include <vector>

namespace Repository {

struct Token {
    int id;
    int wallet_id;
    std::string contract_address;
    std::string symbol;
    std::string name;
    int decimals;
    std::string created_at;
};

class TokenRepository {
  public:
    explicit TokenRepository(Database::DatabaseManager& dbManager);

    Result<Token> createToken(int wallet_id, const std::string& contract_address,
                              const std::string& symbol, const std::string& name, int decimals);
    Result<Token> getToken(int wallet_id, const std::string& contract_address);
    Result<std::vector<Token>> getTokensForWallet(int wallet_id);
    Result<bool> deleteToken(int wallet_id, const std::string& contract_address);

  private:
    Database::DatabaseManager& m_dbManager;
};

}  // namespace Repository

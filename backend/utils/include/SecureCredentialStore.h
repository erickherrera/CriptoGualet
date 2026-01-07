#pragma once

#include <string>
#include <optional>

namespace SecureStorage {

/**
 * Secure credential storage using Windows Credential Manager
 * 
 * Stores credentials in Windows Vault (encrypted by Windows)
 * - User-specific access control
 * - Encrypted storage
 * - No plaintext exposure
 */
class SecureCredentialStore {
public:
  /**
   * Store SMTP credentials securely
   * @param username SMTP username (email address)
   * @param password SMTP password (app password)
   * @return true if successful, false otherwise
   */
  static bool StoreSMTPCredentials(const std::string& username, 
                                   const std::string& password);

  /**
   * Retrieve SMTP password from secure storage
   * @param username SMTP username to look up
   * @return password if found, empty optional otherwise
   */
  static std::optional<std::string> RetrieveSMTPPassword(const std::string& username);

  /**
   * Delete SMTP credentials from secure storage
   * @param username SMTP username to delete
   * @return true if successful, false otherwise
   */
  static bool DeleteSMTPCredentials(const std::string& username);

  /**
   * Check if SMTP credentials exist for given username
   * @param username SMTP username to check
   * @return true if credentials exist, false otherwise
   */
  static bool HasSMTPCredentials(const std::string& username);

private:
  // Internal helper to build credential target name
  static std::wstring BuildCredentialTarget(const std::string& username);
};

} // namespace SecureStorage


#pragma once

#include <optional>
#include <string>

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
    // SMTP functionality removed
};

}  // namespace SecureStorage

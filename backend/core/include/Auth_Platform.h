#pragma once
// Platform-specific authentication helpers
// Windows: Windows API (GetComputerName, GetUserName, GetVolumeInformation)
// macOS: POSIX + IOKit (gethostname, getpwuid, IORegistryEntry)

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Auth {
namespace Platform {

// === Machine Entropy ===
// Gathers machine-specific entropy for database key derivation
// This binds encrypted data to the specific machine and user account
// Windows: Computer name, username, volume serial number
// macOS: Hostname, username, hardware UUID
bool GetMachineEntropy(std::vector<uint8_t>& entropy);

// === User Information ===
// Get current logged-in username
// Windows: GetUserNameA
// macOS: getpwuid(getuid())
bool GetCurrentUsername(std::string& username);

// Get maximum username length for the platform
// Windows: UNLEN (256)
// macOS: LOGIN_NAME_MAX (256)
size_t GetMaxUsernameLength();

// === Secure Storage Path ===
// Get platform-appropriate path for secure storage
// Windows: %APPDATA%\CriptoGualet
// macOS: ~/Library/Application Support/CriptoGualet
bool GetSecureStoragePath(std::string& path);

}  // namespace Platform
}  // namespace Auth

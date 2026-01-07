// Auth_Windows.cpp - Windows-specific authentication helpers
// Uses Windows API for machine entropy and user information

#ifdef _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <lmcons.h>  // For UNLEN constant
#include <shlobj.h>  // For SHGetFolderPathA

#include "include/Auth_Platform.h"

#include <cstring>

namespace Auth {
namespace Platform {

bool GetMachineEntropy(std::vector<uint8_t>& entropy) {
    entropy.clear();

    // 1. Get Windows computer name
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        entropy.insert(entropy.end(), computerName, computerName + size);
    }

    // 2. Get Windows username
    char username[UNLEN + 1];
    size = sizeof(username);
    if (GetUserNameA(username, &size)) {
        entropy.insert(entropy.end(), username, username + size);
    }

    // 3. Get volume serial number (disk-specific)
    DWORD volumeSerial = 0;
    if (GetVolumeInformationA("C:\\", nullptr, 0, &volumeSerial, nullptr, nullptr, nullptr, 0)) {
        uint8_t* serialBytes = reinterpret_cast<uint8_t*>(&volumeSerial);
        entropy.insert(entropy.end(), serialBytes, serialBytes + sizeof(volumeSerial));
    }

    // Ensure we have minimum entropy
    return entropy.size() >= 16;
}

bool GetCurrentUsername(std::string& username) {
    char buf[UNLEN + 1];
    DWORD size = sizeof(buf);
    if (GetUserNameA(buf, &size)) {
        username = buf;
        return true;
    }
    return false;
}

size_t GetMaxUsernameLength() { return UNLEN; }

bool GetSecureStoragePath(std::string& path) {
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        path = std::string(appDataPath) + "\\CriptoGualet";
        return true;
    }
    return false;
}

}  // namespace Platform
}  // namespace Auth

#endif  // _WIN32

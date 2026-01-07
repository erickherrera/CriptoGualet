// Auth_macOS.cpp - macOS-specific authentication helpers
// Uses POSIX APIs and IOKit for machine entropy

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "include/Auth_Platform.h"

#include <cstring>

namespace Auth {
namespace Platform {

bool GetMachineEntropy(std::vector<uint8_t>& entropy) {
    entropy.clear();

    // 1. Get hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        size_t len = strlen(hostname);
        entropy.insert(entropy.end(), hostname, hostname + len);
    }

    // 2. Get username
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        const char* name = pw->pw_name;
        size_t len = strlen(name);
        entropy.insert(entropy.end(), name, name + len);
    }

    // 3. Get hardware UUID (macOS-specific, equivalent to Windows volume serial)
    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMainPortDefault, "IOService:/");
    if (ioRegistryRoot) {
        CFStringRef uuidCF = (CFStringRef)IORegistryEntryCreateCFProperty(
            ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
        if (uuidCF) {
            char uuid[128];
            if (CFStringGetCString(uuidCF, uuid, sizeof(uuid), kCFStringEncodingUTF8)) {
                size_t len = strlen(uuid);
                entropy.insert(entropy.end(), uuid, uuid + len);
            }
            CFRelease(uuidCF);
        }
        IOObjectRelease(ioRegistryRoot);
    }

    // Ensure we have minimum entropy
    return entropy.size() >= 16;
}

bool GetCurrentUsername(std::string& username) {
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        username = pw->pw_name;
        return true;
    }
    return false;
}

size_t GetMaxUsernameLength() {
    // POSIX LOGIN_NAME_MAX is typically 256
    return 256;
}

bool GetSecureStoragePath(std::string& path) {
    // macOS standard: ~/Library/Application Support/AppName
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        path = std::string(pw->pw_dir) + "/Library/Application Support/CriptoGualet";
        return true;
    }
    return false;
}

}  // namespace Platform
}  // namespace Auth

#endif  // __APPLE__

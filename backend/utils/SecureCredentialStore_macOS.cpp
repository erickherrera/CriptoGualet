// SecureCredentialStore_macOS.cpp - macOS Keychain implementation
// Provides secure credential storage using macOS Keychain Services

#ifdef __APPLE__

#include "include/SecureCredentialStore.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

namespace SecureStorage {

namespace {

// Service name for SMTP credentials in Keychain
static const char* KEYCHAIN_SERVICE = "CriptoGualet-SMTP";

CFStringRef CreateCFString(const std::string& str) {
    return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
}

CFDataRef CreateCFData(const std::string& str) {
    return CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(str.c_str()),
                        str.size());
}

}  // anonymous namespace

bool SecureCredentialStore::StoreSMTPCredentials(const std::string& username,
                                                 const std::string& password) {
    if (username.empty() || password.empty()) {
        return false;
    }

    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(username);
    CFDataRef passwordData = CreateCFData(password);

    if (!service || !account || !passwordData) {
        if (service)
            CFRelease(service);
        if (account)
            CFRelease(account);
        if (passwordData)
            CFRelease(passwordData);
        return false;
    }

    // First, try to delete any existing item
    CFMutableDictionaryRef deleteQuery = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(deleteQuery, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(deleteQuery, kSecAttrService, service);
    CFDictionarySetValue(deleteQuery, kSecAttrAccount, account);
    SecItemDelete(deleteQuery);
    CFRelease(deleteQuery);

    // Add new item to Keychain
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFDictionarySetValue(query, kSecValueData, passwordData);
    // Allow access when device is unlocked, survives app reinstalls
    CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked);
    // Add a comment for user visibility in Keychain Access
    CFStringRef comment = CFSTR("CriptoGualet SMTP credentials");
    CFDictionarySetValue(query, kSecAttrComment, comment);

    OSStatus status = SecItemAdd(query, nullptr);

    CFRelease(query);
    CFRelease(service);
    CFRelease(account);
    CFRelease(passwordData);

    return status == errSecSuccess;
}

std::optional<std::string> SecureCredentialStore::RetrieveSMTPPassword(
    const std::string& username) {
    if (username.empty()) {
        return std::nullopt;
    }

    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(username);

    if (!service || !account) {
        if (service)
            CFRelease(service);
        if (account)
            CFRelease(account);
        return std::nullopt;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFDataRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&result);

    CFRelease(query);
    CFRelease(service);
    CFRelease(account);

    if (status == errSecSuccess && result) {
        const char* bytes = reinterpret_cast<const char*>(CFDataGetBytePtr(result));
        CFIndex length = CFDataGetLength(result);
        std::string password(bytes, length);
        CFRelease(result);
        return password;
    }

    return std::nullopt;
}

bool SecureCredentialStore::DeleteSMTPCredentials(const std::string& username) {
    if (username.empty()) {
        return false;
    }

    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(username);

    if (!service || !account) {
        if (service)
            CFRelease(service);
        if (account)
            CFRelease(account);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);

    OSStatus status = SecItemDelete(query);

    CFRelease(query);
    CFRelease(service);
    CFRelease(account);

    // Consider success if item was deleted or didn't exist
    return status == errSecSuccess || status == errSecItemNotFound;
}

bool SecureCredentialStore::HasSMTPCredentials(const std::string& username) {
    if (username.empty()) {
        return false;
    }

    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(username);

    if (!service || !account) {
        if (service)
            CFRelease(service);
        if (account)
            CFRelease(account);
        return false;
    }

    CFMutableDictionaryRef query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, service);
    CFDictionarySetValue(query, kSecAttrAccount, account);
    // Don't return data, just check existence
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanFalse);

    OSStatus status = SecItemCopyMatching(query, nullptr);

    CFRelease(query);
    CFRelease(service);
    CFRelease(account);

    return status == errSecSuccess;
}

}  // namespace SecureStorage

#endif  // __APPLE__

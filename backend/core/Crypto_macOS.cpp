// Crypto_macOS.cpp - macOS-specific cryptographic implementations
// Uses CommonCrypto and Keychain Services

#ifdef __APPLE__

#include <CommonCrypto/CommonCrypto.h>
#include <CommonCrypto/CommonRandom.h>
#include <Security/Security.h>

#include "include/Crypto_Platform.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace Crypto {
namespace Platform {

// === Random Number Generation ===
bool RandBytes(void* buf, size_t len) { return CCRandomGenerateBytes(buf, len) == kCCSuccess; }

// === SHA256 Hash ===
bool SHA256(const uint8_t* data, size_t len, uint8_t* out) {
    CC_SHA256(data, static_cast<CC_LONG>(len), out);
    return true;
}

// === HMAC-SHA256 ===
bool HMAC_SHA256(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
                 uint8_t* out) {
    CCHmac(kCCHmacAlgSHA256, key, key_len, data, data_len, out);
    return true;
}

// === HMAC-SHA512 ===
bool HMAC_SHA512(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
                 uint8_t* out) {
    CCHmac(kCCHmacAlgSHA512, key, key_len, data, data_len, out);
    return true;
}

// === HMAC-SHA1 (for TOTP) ===
bool HMAC_SHA1(const uint8_t* key, size_t key_len, const uint8_t* data, size_t data_len,
               uint8_t* out) {
    CCHmac(kCCHmacAlgSHA1, key, key_len, data, data_len, out);
    return true;
}

// === AES-GCM Encryption ===
// Note: CommonCrypto added GCM support in macOS 10.13+
bool AES_GCM_Encrypt(const uint8_t* key, size_t key_len, const uint8_t* plaintext,
                     size_t plaintext_len, const uint8_t* aad, size_t aad_len, uint8_t* iv,
                     size_t iv_len, uint8_t* ciphertext, uint8_t* tag, size_t tag_len) {
    if (key_len != 32 || iv_len != 12 || tag_len != 16)
        return false;

    // Generate random IV
    if (!RandBytes(iv, iv_len))
        return false;

    // Use CCCryptorGCMOneshotEncrypt (available macOS 10.13+)
    CCCryptorStatus status =
        CCCryptorGCMOneshotEncrypt(kCCAlgorithmAES, key, key_len, iv, iv_len, aad, aad_len,
                                   plaintext, plaintext_len, ciphertext, tag, tag_len);

    return status == kCCSuccess;
}

// === AES-GCM Decryption ===
bool AES_GCM_Decrypt(const uint8_t* key, size_t key_len, const uint8_t* ciphertext,
                     size_t ciphertext_len, const uint8_t* aad, size_t aad_len, const uint8_t* iv,
                     size_t iv_len, const uint8_t* tag, size_t tag_len, uint8_t* plaintext) {
    if (key_len != 32 || iv_len != 12 || tag_len != 16)
        return false;

    // Use CCCryptorGCMOneshotDecrypt (available macOS 10.13+)
    CCCryptorStatus status =
        CCCryptorGCMOneshotDecrypt(kCCAlgorithmAES, key, key_len, iv, iv_len, aad, aad_len,
                                   ciphertext, ciphertext_len, plaintext, tag, tag_len);

    return status == kCCSuccess;
}

// === Keychain Helper Functions ===
namespace {

// Service name for all CriptoGualet keychain items
static const char* KEYCHAIN_SERVICE = "CriptoGualet";

CFStringRef CreateCFString(const std::string& str) {
    return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
}

CFDataRef CreateCFData(const std::vector<uint8_t>& data) {
    return CFDataCreate(kCFAllocatorDefault, data.data(), data.size());
}

}  // anonymous namespace

// === Keychain Secure Storage (DPAPI equivalent) ===
bool SecureProtect(const std::vector<uint8_t>& plaintext, const std::string& identifier,
                   std::vector<uint8_t>& ciphertext) {
    // On macOS, we store directly in Keychain which handles encryption
    // The "ciphertext" output is just the identifier for retrieval

    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(identifier);
    CFDataRef data = CreateCFData(plaintext);

    if (!service || !account || !data) {
        if (service)
            CFRelease(service);
        if (account)
            CFRelease(account);
        if (data)
            CFRelease(data);
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
    CFDictionarySetValue(query, kSecValueData, data);
    // Allow access when device is unlocked, survives app reinstalls
    CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked);

    OSStatus status = SecItemAdd(query, nullptr);

    CFRelease(query);
    CFRelease(service);
    CFRelease(account);
    CFRelease(data);

    if (status == errSecSuccess) {
        // Return identifier as "ciphertext" marker
        ciphertext.assign(identifier.begin(), identifier.end());
        return true;
    }

    return false;
}

bool SecureUnprotect(const std::vector<uint8_t>& ciphertext, const std::string& identifier,
                     std::vector<uint8_t>& plaintext) {
    (void)ciphertext;  // Not used on macOS - we look up by identifier

    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(identifier);

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
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFDataRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&result);

    CFRelease(query);
    CFRelease(service);
    CFRelease(account);

    if (status == errSecSuccess && result) {
        const uint8_t* bytes = CFDataGetBytePtr(result);
        CFIndex length = CFDataGetLength(result);
        plaintext.assign(bytes, bytes + length);
        CFRelease(result);
        return true;
    }

    return false;
}

bool SecureDelete(const std::string& identifier) {
    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(identifier);

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

    return status == errSecSuccess || status == errSecItemNotFound;
}

bool SecureExists(const std::string& identifier) {
    CFStringRef service = CreateCFString(KEYCHAIN_SERVICE);
    CFStringRef account = CreateCFString(identifier);

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
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanFalse);

    OSStatus status = SecItemCopyMatching(query, nullptr);

    CFRelease(query);
    CFRelease(service);
    CFRelease(account);

    return status == errSecSuccess;
}

}  // namespace Platform
}  // namespace Crypto

#endif  // __APPLE__

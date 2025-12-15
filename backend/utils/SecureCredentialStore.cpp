#include "include/SecureCredentialStore.h"

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <wincred.h>
#undef UNICODE
#undef _UNICODE

#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Credui.lib")

namespace SecureStorage {

std::wstring SecureCredentialStore::BuildCredentialTarget(const std::string& username) {
  // Format: "CriptoGualet:SMTP:username@domain.com"
  std::wstring target = L"CriptoGualet:SMTP:";
  target += std::wstring(username.begin(), username.end());
  return target;
}

bool SecureCredentialStore::StoreSMTPCredentials(const std::string& username,
                                                   const std::string& password) {
  if (username.empty() || password.empty()) {
    return false;
  }

  std::wstring targetName = BuildCredentialTarget(username);
  std::wstring wUsername(username.begin(), username.end());

  // Convert password to wide string for credential blob
  std::vector<BYTE> passwordBlob(password.begin(), password.end());

  CREDENTIALW cred = {0};
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = const_cast<LPWSTR>(targetName.c_str());
  cred.Comment = const_cast<LPWSTR>(L"CriptoGualet SMTP credentials");
  cred.UserName = const_cast<LPWSTR>(wUsername.c_str());
  cred.CredentialBlobSize = static_cast<DWORD>(passwordBlob.size());
  cred.CredentialBlob = passwordBlob.data();
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE; // Persist across reboots
  cred.AttributeCount = 0;
  cred.Attributes = nullptr;

  BOOL result = CredWriteW(&cred, 0);
  
  // Securely wipe password from memory
  std::fill(passwordBlob.begin(), passwordBlob.end(), 0);

  return result == TRUE;
}

std::optional<std::string> SecureCredentialStore::RetrieveSMTPPassword(
    const std::string& username) {
  if (username.empty()) {
    return std::nullopt;
  }

  std::wstring targetName = BuildCredentialTarget(username);
  PCREDENTIALW cred = nullptr;

  BOOL result = CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &cred);
  if (result == FALSE || cred == nullptr) {
    return std::nullopt;
  }

  // Extract password from credential blob
  std::string password;
  if (cred->CredentialBlobSize > 0 && cred->CredentialBlob != nullptr) {
    password.assign(reinterpret_cast<const char*>(cred->CredentialBlob),
                    cred->CredentialBlobSize);
  }

  // Free credential structure
  CredFree(cred);

  if (password.empty()) {
    return std::nullopt;
  }

  return password;
}

bool SecureCredentialStore::DeleteSMTPCredentials(const std::string& username) {
  if (username.empty()) {
    return false;
  }

  std::wstring targetName = BuildCredentialTarget(username);
  BOOL result = CredDeleteW(targetName.c_str(), CRED_TYPE_GENERIC, 0);

  return result == TRUE;
}

bool SecureCredentialStore::HasSMTPCredentials(const std::string& username) {
  if (username.empty()) {
    return false;
  }

  std::wstring targetName = BuildCredentialTarget(username);
  PCREDENTIALW cred = nullptr;

  BOOL result = CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &cred);
  if (cred != nullptr) {
    CredFree(cred);
  }

  return result == TRUE;
}

} // namespace SecureStorage


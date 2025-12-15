#include "include/EmailService.h"
#include "include/SecureCredentialStore.h"
#include "../core/include/Crypto.h" // For RandBytes

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

// libcurl for SMTP
#include <curl/curl.h>

// Windows-specific includes
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winreg.h>

#pragma comment(lib, "Advapi32.lib")

namespace Email {

// ===== Helper Functions =====

std::string GenerateVerificationCode() {
  // Generate a secure 6-digit code using cryptographic RNG
  std::array<uint8_t, 3> randomBytes{};
  if (!Crypto::RandBytes(randomBytes.data(), randomBytes.size())) {
    // Fallback to less secure method if crypto RNG fails
    srand(static_cast<unsigned>(time(nullptr)));
    int code = 100000 + (rand() % 900000);
    return std::to_string(code);
  }

  // Convert 3 random bytes to a number in range [100000, 999999]
  uint32_t randomNumber = (static_cast<uint32_t>(randomBytes[0]) << 16) |
                         (static_cast<uint32_t>(randomBytes[1]) << 8) |
                         static_cast<uint32_t>(randomBytes[2]);

  // Map to 6-digit range [100000, 999999]
  int code = 100000 + (randomNumber % 900000);
  return std::to_string(code);
}

bool IsValidEmailFormat(const std::string &email) {
  // Basic email validation using regex
  // Pattern: local@domain.tld
  static const std::regex emailRegex(
      R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
  return std::regex_match(email, emailRegex);
}

// ===== SMTPEmailService Implementation =====

SMTPEmailService::SMTPEmailService(const EmailConfig &config)
    : m_config(config) {
  // Validate configuration
  if (m_config.smtpServer.empty()) {
    // Try to load from environment if not provided
    m_config = loadConfigFromEnvironment();
  }
}

// Helper function to read environment variable from registry if not in process environment
static std::string GetEnvVarFromRegistry(const char* varName) {
#ifdef _WIN32
  // First try process environment
#pragma warning(push)
#pragma warning(disable : 4996) // Suppress getenv warning
  const char* envValue = std::getenv(varName);
#pragma warning(pop)
  if (envValue) {
    return std::string(envValue);
  }
  
  // If not found, try reading from registry
  HKEY hKey;
  LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_READ, &hKey);
  if (result == ERROR_SUCCESS) {
    char buffer[1024];
    DWORD bufferSize = sizeof(buffer);
    DWORD type = REG_SZ;
    
    result = RegQueryValueExA(hKey, varName, nullptr, &type, (LPBYTE)buffer, &bufferSize);
    RegCloseKey(hKey);
    
    if (result == ERROR_SUCCESS && type == REG_SZ) {
      return std::string(buffer);
    }
  }
#endif
  return std::string();
}

EmailConfig SMTPEmailService::loadConfigFromEnvironment() {
  EmailConfig config;

  // Read from environment or registry
  std::string server = GetEnvVarFromRegistry("WALLET_SMTP_SERVER");
  std::string port = GetEnvVarFromRegistry("WALLET_SMTP_PORT");
  std::string username = GetEnvVarFromRegistry("WALLET_SMTP_USERNAME");
  std::string fromEmail = GetEnvVarFromRegistry("WALLET_FROM_EMAIL");
  std::string fromName = GetEnvVarFromRegistry("WALLET_FROM_NAME");

  if (!server.empty())
    config.smtpServer = server;
  if (!port.empty())
    config.smtpPort = std::stoi(port);
  if (!username.empty()) {
    config.username = username;
    // Try to retrieve password from secure credential store first
    auto securePassword = SecureStorage::SecureCredentialStore::RetrieveSMTPPassword(username);
    if (securePassword.has_value()) {
      config.password = securePassword.value();
    } else {
      // Fallback to environment variable or registry (for backward compatibility during migration)
      std::string password = GetEnvVarFromRegistry("WALLET_SMTP_PASSWORD");
      if (!password.empty()) {
        config.password = password;
        // Store in secure credential store for future use
        SecureStorage::SecureCredentialStore::StoreSMTPCredentials(username, password);
      }
    }
  }
  if (!fromEmail.empty())
    config.fromEmail = fromEmail;
  if (!fromName.empty())
    config.fromName = fromName;
  else
    config.fromName = "CriptoGualet Wallet";

  return config;
}

bool SMTPEmailService::StoreSMTPPassword(const std::string& username, const std::string& password) {
  if (username.empty() || password.empty()) {
    return false;
  }
  return SecureStorage::SecureCredentialStore::StoreSMTPCredentials(username, password);
}

std::string SMTPEmailService::generateVerificationEmailBody(
    const std::string &toName, const std::string &code) {
  std::ostringstream body;

  body << "Hello " << toName << ",\n\n";
  body << "Welcome to CriptoGualet!\n\n";
  body << "Your email verification code is:\n\n";
  body << "    " << code << "\n\n";
  body << "This code will expire in 10 minutes.\n\n";
  body << "If you did not create an account with CriptoGualet, please ignore this email.\n\n";
  body << "Best regards,\n";
  body << "The CriptoGualet Team\n\n";
  body << "---\n";
  body << "This is an automated message. Please do not reply to this email.";

  return body.str();
}

EmailResult SMTPEmailService::sendVerificationCode(const std::string &toEmail,
                                                   const std::string &toName,
                                                   const std::string &code) {
  EmailMessage message;
  message.toEmail = toEmail;
  message.toName = toName;
  message.subject = "CriptoGualet - Email Verification Code";
  message.body = generateVerificationEmailBody(toName, code);
  message.isHTML = false;

  return sendEmail(message);
}

EmailResult SMTPEmailService::sendEmail(const EmailMessage &message) {
  // Trim and validate recipient email
  std::string toEmail = message.toEmail;
  toEmail.erase(0, toEmail.find_first_not_of(" \t\n\r"));
  toEmail.erase(toEmail.find_last_not_of(" \t\n\r") + 1);
  
  if (!IsValidEmailFormat(toEmail)) {
    return {false, "Invalid recipient email address format: " + message.toEmail};
  }

  // Trim and validate sender email
  std::string fromEmail = m_config.fromEmail;
  fromEmail.erase(0, fromEmail.find_first_not_of(" \t\n\r"));
  fromEmail.erase(fromEmail.find_last_not_of(" \t\n\r") + 1);
  
  if (fromEmail.empty()) {
    return {false,
            "Email service not configured. Please set SMTP environment variable:\n"
            "WALLET_FROM_EMAIL (the email address to send from)"};
  }
  
  if (!IsValidEmailFormat(fromEmail)) {
    return {false, "Invalid sender email address format in configuration: " + m_config.fromEmail};
  }

  // Validate configuration
  if (m_config.smtpServer.empty() || m_config.username.empty()) {
    return {false,
            "Email service not configured. Please set SMTP environment variables:\n"
            "WALLET_SMTP_SERVER, WALLET_SMTP_USERNAME, WALLET_FROM_EMAIL\n"
            "Password should be stored securely using Windows Credential Manager."};
  }

  // Try to retrieve password from secure storage if not already loaded
  if (m_config.password.empty()) {
    auto securePassword = SecureStorage::SecureCredentialStore::RetrieveSMTPPassword(m_config.username);
    if (securePassword.has_value()) {
      m_config.password = securePassword.value();
    } else {
      return {false,
              "SMTP password not found. Please configure SMTP credentials securely."};
    }
  }

  // Update config and message with trimmed emails
  m_config.fromEmail = fromEmail;
  EmailMessage trimmedMessage = message;
  trimmedMessage.toEmail = toEmail;

  return connectAndSend(trimmedMessage);
}

// Callback function for libcurl to read email payload
struct EmailPayload {
  const std::string* emailContent;
  size_t position;
};

static size_t ReadEmailPayload(void* ptr, size_t size, size_t nmemb, void* userp) {
  EmailPayload* payload = static_cast<EmailPayload*>(userp);
  size_t totalSize = size * nmemb;
  
  if (payload->position >= payload->emailContent->size()) {
    return 0; // No more data
  }
  
  size_t remaining = payload->emailContent->size() - payload->position;
  size_t toCopy = (totalSize < remaining) ? totalSize : remaining;
  
  std::memcpy(ptr, payload->emailContent->c_str() + payload->position, toCopy);
  payload->position += toCopy;
  
  return toCopy;
}

EmailResult SMTPEmailService::connectAndSend(const EmailMessage &message) {
  // Production implementation using libcurl for secure SMTP communication
  // This replaces the insecure PowerShell script approach
  
  CURL* curl = curl_easy_init();
  if (!curl) {
    return {false, "Failed to initialize libcurl"};
  }

  EmailResult result = {false, "Unknown error"};
  
  // Build email content in RFC 5322 format
  std::ostringstream emailContentStream;
  emailContentStream << "From: " << m_config.fromEmail << "\r\n";
  emailContentStream << "To: " << message.toEmail << "\r\n";
  emailContentStream << "Subject: " << message.subject << "\r\n";
  emailContentStream << "Content-Type: text/plain; charset=UTF-8\r\n";
  emailContentStream << "\r\n";
  emailContentStream << message.body << "\r\n";
  
  std::string emailStr = emailContentStream.str();
  
  // Set up payload for reading
  EmailPayload payload;
  payload.emailContent = &emailStr;
  payload.position = 0;
  
  // Set up libcurl for SMTP
  struct curl_slist* recipients_list = nullptr;
  recipients_list = curl_slist_append(recipients_list, message.toEmail.c_str());
  
  // Build SMTP URL
  std::string smtpUrl = "smtp://" + m_config.smtpServer + ":" + std::to_string(m_config.smtpPort);
  curl_easy_setopt(curl, CURLOPT_URL, smtpUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_MAIL_FROM, m_config.fromEmail.c_str());
  curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients_list);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadEmailPayload);
  curl_easy_setopt(curl, CURLOPT_READDATA, &payload);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)emailStr.size());
  curl_easy_setopt(curl, CURLOPT_USERNAME, m_config.username.c_str());
  curl_easy_setopt(curl, CURLOPT_PASSWORD, m_config.password.c_str());
  
  // Enable TLS/SSL
  if (m_config.useTLS) {
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  }
  
  // Set timeout
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  
  // Perform the email send
  CURLcode res = curl_easy_perform(curl);
  
  if (res == CURLE_OK) {
    long responseCode;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    result = {true, "Email sent successfully"};
  } else {
    result = {false, std::string("Failed to send email: ") + curl_easy_strerror(res)};
  }
  
  // Cleanup
  curl_slist_free_all(recipients_list);
  curl_easy_cleanup(curl);
  
  // Securely wipe password from memory (if it was loaded)
  // Note: libcurl may have copied it, but we clear our copy
  if (!m_config.password.empty()) {
    std::fill(m_config.password.begin(), m_config.password.end(), '\0');
  }
  
  return result;
}

} // namespace Email

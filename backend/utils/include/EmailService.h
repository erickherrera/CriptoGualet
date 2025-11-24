#pragma once

#include <string>
#include <vector>

namespace Email {

// Email configuration structure
struct EmailConfig {
  std::string smtpServer;      // e.g., "smtp.gmail.com"
  int smtpPort = 587;          // 587 for TLS, 465 for SSL
  std::string username;        // SMTP auth username
  std::string password;        // SMTP auth password (app password for Gmail)
  std::string fromEmail;       // Sender email address
  std::string fromName;        // Sender display name
  bool useTLS = true;          // Use STARTTLS
};

// Email message structure
struct EmailMessage {
  std::string toEmail;
  std::string toName;
  std::string subject;
  std::string body;
  bool isHTML = false;
};

// Email sending result
struct EmailResult {
  bool success = false;
  std::string errorMessage;
};

// Abstract EmailService interface
class IEmailService {
public:
  virtual ~IEmailService() = default;

  // Send email with given message
  virtual EmailResult sendEmail(const EmailMessage &message) = 0;

  // Send verification code email (convenience method)
  virtual EmailResult sendVerificationCode(const std::string &toEmail,
                                          const std::string &toName,
                                          const std::string &code) = 0;
};

// SMTP Email Service Implementation
class SMTPEmailService : public IEmailService {
public:
  explicit SMTPEmailService(const EmailConfig &config);
  ~SMTPEmailService() override = default;

  EmailResult sendEmail(const EmailMessage &message) override;
  EmailResult sendVerificationCode(const std::string &toEmail,
                                   const std::string &toName,
                                   const std::string &code) override;

  // Load configuration from environment variables
  // Expected variables:
  //   WALLET_SMTP_SERVER (e.g., smtp.gmail.com)
  //   WALLET_SMTP_PORT (default: 587)
  //   WALLET_SMTP_USERNAME (your email)
  //   WALLET_SMTP_PASSWORD (app password) - will be stored securely
  //   WALLET_FROM_EMAIL (sender email)
  //   WALLET_FROM_NAME (sender name, optional)
  static EmailConfig loadConfigFromEnvironment();

  // Store SMTP password securely in Windows Credential Manager
  // This should be called once to migrate from environment variables
  // to secure storage
  static bool StoreSMTPPassword(const std::string& username, const std::string& password);

private:
  EmailConfig m_config;

  // Internal SMTP communication methods
  EmailResult connectAndSend(const EmailMessage &message);
  std::string generateVerificationEmailBody(const std::string &toName,
                                           const std::string &code);
};

// Helper: Generate a 6-digit verification code
std::string GenerateVerificationCode();

// Helper: Check if email format is valid (basic validation)
bool IsValidEmailFormat(const std::string &email);

} // namespace Email

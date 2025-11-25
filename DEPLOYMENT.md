# Deployment Guide

## SMTP Configuration (System-Level)

Email verification for 2FA requires SMTP configuration. This must be configured at the **system/deployment level** by administrators, not by end users.

### Required Environment Variables

Set these environment variables at the system/user level:

- `WALLET_SMTP_SERVER` - SMTP server (e.g., `smtp.gmail.com`)
- `WALLET_SMTP_PORT` - SMTP port (usually `587` for TLS)
- `WALLET_SMTP_USERNAME` - SMTP username (email address)
- `WALLET_FROM_EMAIL` - Sender email address
- `WALLET_SMTP_PASSWORD` - SMTP password (stored securely in Windows Credential Manager)
- `WALLET_FROM_NAME` - Sender name (optional, defaults to "CriptoGualet Wallet")

### Windows Setup

**PowerShell (User-level, persistent):**
```powershell
[Environment]::SetEnvironmentVariable("WALLET_SMTP_SERVER", "smtp.gmail.com", "User")
[Environment]::SetEnvironmentVariable("WALLET_SMTP_PORT", "587", "User")
[Environment]::SetEnvironmentVariable("WALLET_SMTP_USERNAME", "your-email@gmail.com", "User")
[Environment]::SetEnvironmentVariable("WALLET_FROM_EMAIL", "your-email@gmail.com", "User")
[Environment]::SetEnvironmentVariable("WALLET_SMTP_PASSWORD", "your-app-password", "User")
[Environment]::SetEnvironmentVariable("WALLET_FROM_NAME", "CriptoGualet Wallet", "User")
```

**Note:** For Gmail, use an App Password from https://myaccount.google.com/apppasswords

The password will be automatically migrated to Windows Credential Manager on first use for secure storage.

### How It Works

1. User registers with email address
2. System sends verification code to user's email (using configured SMTP)
3. User enters verification code to complete registration
4. User can sign in after email is verified

Environment variables persist in the Windows registry (`HKEY_CURRENT_USER\Environment`) and work across all cloned repositories for the same user account.


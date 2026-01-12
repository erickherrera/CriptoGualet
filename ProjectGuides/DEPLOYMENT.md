# Deployment Guide

## Two-Factor Authentication (2FA)

CriptoGualet supports optional two-factor authentication using authenticator apps (TOTP - Time-based One-Time Password).

### Supported Authenticator Apps

- Google Authenticator
- Authy
- Microsoft Authenticator
- 1Password
- Any TOTP-compatible authenticator

### How It Works

1. **Registration**: Users create an account with username and password only (no email required)
2. **Optional 2FA**: Users can enable 2FA in Settings at any time
3. **Setup**: Scan QR code with authenticator app or enter secret manually
4. **Login**: If 2FA is enabled, user must enter 6-digit code from authenticator app

### Security Features

- **TOTP (RFC 6238)**: Industry-standard time-based one-time passwords
- **30-second codes**: Codes refresh every 30 seconds
- **Backup codes**: 8 one-time-use backup codes for recovery
- **No server required**: 2FA works entirely offline (no email service needed)

---

## Build Configuration

### Standard Build

```powershell
cmake --preset default
cmake --build build --config Release
```

### Debug Build

```powershell
cmake --preset default
cmake --build build --config Debug
```

---

## Database

The application uses SQLCipher for encrypted database storage.

- Database file: `wallet.db` (created in working directory)
- Encryption: Machine-specific key derived from Windows user context
- Tables: users, wallets, addresses, transactions, encrypted_seeds

---

## Seed Phrase Security

- BIP-39 12-word seed phrases
- Stored encrypted with Windows DPAPI
- Machine-bound (cannot be moved to different computer)
- Users should backup seed phrase on paper/metal

---

## Distribution

When distributing the application:

1. Build in Release mode
2. Include required DLLs (Qt, OpenSSL, SQLCipher)
3. Include `assets/bip39/english.txt` wordlist
4. No server configuration required (email-free 2FA)

---

## Troubleshooting

### "Failed to initialize database"
- Ensure write permissions in working directory
- Check disk space

### "Seed phrase generation failed"
- Ensure `assets/bip39/english.txt` exists
- Check file permissions

### "2FA code invalid"
- Ensure device time is accurate (±30 seconds)
- Try the next code if on the edge of a time period

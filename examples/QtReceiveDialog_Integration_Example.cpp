/**
 * QtReceiveDialog Integration Example
 *
 * This example demonstrates how to integrate QtReceiveDialog into CriptoGualet's
 * main application (CriptoGualetQt.cpp) to display receiving addresses with QR codes.
 *
 * The dialog replaces the simple QMessageBox approach with a rich, user-friendly
 * interface that includes:
 * - Large QR code for easy scanning
 * - One-click address copy to clipboard
 * - Optional payment request amounts
 * - Consistent theme integration
 */

// ============================================================================
// STEP 1: Add include at the top of CriptoGualetQt.cpp
// ============================================================================

#include "../frontend/qt/include/QtReceiveDialog.h"

// ============================================================================
// STEP 2: Replace Bitcoin receive handler
// ============================================================================

// OLD CODE (around line 355-396 in CriptoGualetQt.cpp):
/*
connect(m_walletUI, &QtWalletUI::receiveBitcoinRequested, [this]() {
  if (!m_wallet || g_currentUser.empty() ||
      g_users.find(g_currentUser) == g_users.end()) {
    QMessageBox::warning(this, "Error",
                         "Wallet not initialized or user not logged in");
    return;
  }

  const User &user = g_users[g_currentUser];

  // Get address info and recent transactions
  WalletAPI::ReceiveInfo info = m_wallet->GetAddressInfo(user.walletAddress);

  QString receiveText =
      QString("Your Bitcoin Address:\n%1\n\n"
              "Share this address to receive Bitcoin payments.\n\n"
              "Recent Transactions:\n")
          .arg(QString::fromStdString(info.address));

  if (info.recent_transactions.empty()) {
    receiveText += "No recent transactions found.";
  } else {
    int count = 0;
    for (const auto &txHash : info.recent_transactions) {
      if (count >= 3)
        break; // Show only first 3 for brevity
      receiveText += QString("- %1...\n")
                         .arg(QString::fromStdString(txHash.substr(0, 16)));
      count++;
    }
    if (info.recent_transactions.size() > 3) {
      receiveText +=
          QString("... and %1 more").arg(info.recent_transactions.size() - 3);
    }
  }

  // Copy address to clipboard
  QApplication::clipboard()->setText(QString::fromStdString(info.address));
  receiveText += "\n\nAddress copied to clipboard!";

  QMessageBox::information(this, "Receive Bitcoin", receiveText);
});
*/

// NEW CODE (replacement):
connect(m_walletUI, &QtWalletUI::receiveBitcoinRequested, [this]() {
  if (!m_wallet || g_currentUser.empty() ||
      g_users.find(g_currentUser) == g_users.end()) {
    QMessageBox::warning(this, "Error",
                         "Wallet not initialized or user not logged in");
    return;
  }

  const User &user = g_users[g_currentUser];

  // Get address info
  WalletAPI::ReceiveInfo info = m_wallet->GetAddressInfo(user.walletAddress);

  // Create and show the receive dialog with QR code
  QtReceiveDialog dialog(
    QtReceiveDialog::ChainType::BITCOIN,
    QString::fromStdString(info.address),
    this
  );

  dialog.exec();
});

// ============================================================================
// STEP 3: Replace Ethereum receive handler
// ============================================================================

// OLD CODE (around line 399-461 in CriptoGualetQt.cpp):
/*
connect(m_walletUI, &QtWalletUI::receiveEthereumRequested, [this]() {
  if (!m_ethereumWallet || g_currentUser.empty() ||
      g_users.find(g_currentUser) == g_users.end()) {
    QMessageBox::warning(
        this, "Error",
        "Ethereum wallet not initialized or user not logged in");
    return;
  }

  const User &legacyUser = g_users[g_currentUser];

  // Get Ethereum address (with EIP-55 checksum)
  QString ethAddress;
  if (m_walletRepository && m_userRepository) {
    // Get proper user from repository to access user ID
    auto userResult = m_userRepository->getUserByUsername(g_currentUser);
    if (!userResult.hasValue()) {
      QMessageBox::warning(this, "Error",
                           "Failed to retrieve user information");
      return;
    }

    // Note: Using passwordHash as password - this may need proper password handling
    auto seedResult = m_walletRepository->retrieveDecryptedSeed(
        userResult->id, legacyUser.passwordHash);
    if (seedResult.success && !seedResult.data.empty()) {
      std::array<uint8_t, 64> seed{};
      if (Crypto::BIP39_SeedFromMnemonic(seedResult.data, "", seed)) {
        Crypto::BIP32ExtendedKey masterKey;
        if (Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
          std::string ethAddr;
          if (Crypto::BIP44_GetEthereumAddress(masterKey, 0, false, 0,
                                               ethAddr)) {
            ethAddress = QString::fromStdString(ethAddr);
          }
        }
        seed.fill(uint8_t(0)); // Securely wipe
      }
    }
  }

  if (ethAddress.isEmpty()) {
    QMessageBox::warning(this, "Error",
                         "Failed to retrieve Ethereum address");
    return;
  }

  QString receiveText =
      QString("Your Ethereum Address:\n%1\n\n"
              "Share this address to receive Ethereum payments.\n\n"
              "Note: This address is in EIP-55 checksum format for extra "
              "safety.\n"
              "You can use this address on Ethereum mainnet.")
          .arg(ethAddress);

  // Copy address to clipboard
  QApplication::clipboard()->setText(ethAddress);
  receiveText += "\n\nAddress copied to clipboard!";

  QMessageBox::information(this, "Receive Ethereum", receiveText);
});
*/

// NEW CODE (replacement):
connect(m_walletUI, &QtWalletUI::receiveEthereumRequested, [this]() {
  if (!m_ethereumWallet || g_currentUser.empty() ||
      g_users.find(g_currentUser) == g_users.end()) {
    QMessageBox::warning(
        this, "Error",
        "Ethereum wallet not initialized or user not logged in");
    return;
  }

  const User &legacyUser = g_users[g_currentUser];

  // Get Ethereum address (with EIP-55 checksum)
  QString ethAddress;
  if (m_walletRepository && m_userRepository) {
    // Get proper user from repository to access user ID
    auto userResult = m_userRepository->getUserByUsername(g_currentUser);
    if (!userResult.hasValue()) {
      QMessageBox::warning(this, "Error",
                           "Failed to retrieve user information");
      return;
    }

    // Note: Using passwordHash as password - this may need proper password handling
    auto seedResult = m_walletRepository->retrieveDecryptedSeed(
        userResult->id, legacyUser.passwordHash);
    if (seedResult.success && !seedResult.data.empty()) {
      std::array<uint8_t, 64> seed{};
      if (Crypto::BIP39_SeedFromMnemonic(seedResult.data, "", seed)) {
        Crypto::BIP32ExtendedKey masterKey;
        if (Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
          std::string ethAddr;
          if (Crypto::BIP44_GetEthereumAddress(masterKey, 0, false, 0,
                                               ethAddr)) {
            ethAddress = QString::fromStdString(ethAddr);
          }
        }
        seed.fill(uint8_t(0)); // Securely wipe
      }
    }
  }

  if (ethAddress.isEmpty()) {
    QMessageBox::warning(this, "Error",
                         "Failed to retrieve Ethereum address");
    return;
  }

  // Create and show the receive dialog with QR code
  QtReceiveDialog dialog(
    QtReceiveDialog::ChainType::ETHEREUM,
    ethAddress,
    this
  );

  dialog.exec();
});

// ============================================================================
// ADVANCED USAGE: Creating dialog programmatically
// ============================================================================

void exampleCreateAndShowDialog() {
  // Example 1: Bitcoin receive dialog
  QString bitcoinAddress = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
  QtReceiveDialog bitcoinDialog(
    QtReceiveDialog::ChainType::BITCOIN,
    bitcoinAddress,
    nullptr  // or pass parent widget
  );
  bitcoinDialog.exec();

  // Example 2: Ethereum receive dialog
  QString ethereumAddress = "0x742d35Cc6634C0532925a3b844Bc9e7595f0bEb";
  QtReceiveDialog ethereumDialog(
    QtReceiveDialog::ChainType::ETHEREUM,
    ethereumAddress,
    nullptr  // or pass parent widget
  );
  ethereumDialog.exec();
}

// ============================================================================
// FEATURES OVERVIEW
// ============================================================================

/*
 * QtReceiveDialog Features:
 *
 * 1. Multi-Chain Support
 *    - Bitcoin (BTC) with bitcoin: URI scheme
 *    - Ethereum (ETH) with ethereum: URI scheme
 *
 * 2. QR Code Generation
 *    - High-quality QR codes using QRGenerator utility
 *    - Automatic scaling for optimal visibility
 *    - Theme-aware (dark/light mode support)
 *    - 300x300px display size with white border
 *
 * 3. Address Display
 *    - Read-only text field with monospace font
 *    - Address is pre-selected for easy copying
 *    - Dedicated "Copy" button with visual feedback
 *    - 2-second confirmation after copying
 *
 * 4. Payment Request (Optional)
 *    - Optional amount field for creating payment requests
 *    - Checkbox to enable/disable amount in QR code
 *    - When enabled, generates URI with amount (e.g., bitcoin:address?amount=0.001)
 *    - Helpful explanation text for users
 *
 * 5. Theme Integration
 *    - Full QtThemeManager integration
 *    - Supports DARK, LIGHT, CRYPTO_DARK, CRYPTO_LIGHT themes
 *    - QR codes adapt to theme colors (dark/light backgrounds)
 *    - Consistent with other dialogs (QtSendDialog, QtSeedDisplayDialog)
 *
 * 6. User Experience
 *    - Large, scannable QR code
 *    - Clear title and subtitle
 *    - Grouped sections for better organization
 *    - Copy confirmation with temporary button state change
 *    - Smooth animations and transitions
 *
 * 7. Security Considerations
 *    - Read-only address display (cannot be accidentally edited)
 *    - No sensitive data in window title or logs
 *    - QR code regenerates on theme change to prevent visual artifacts
 *    - Proper memory cleanup for QR data
 */

// ============================================================================
// TECHNICAL IMPLEMENTATION NOTES
// ============================================================================

/*
 * QR Code Generation Flow:
 *
 * 1. getPaymentURI() generates the appropriate URI:
 *    - Plain address if no amount specified
 *    - bitcoin:address?amount=X.XXXXXXXX for Bitcoin with amount
 *    - ethereum:address for Ethereum (amount support can be added)
 *
 * 2. generateQRCode() uses QRGenerator utility:
 *    - Calls QR::GenerateQRCode() from backend/utils/QRGenerator
 *    - Converts QRData to QImage
 *    - Applies theme colors (foreground/background)
 *    - Scales to display size (300x300px)
 *    - Adds white border for better scanning
 *
 * 3. updateQRCode() regenerates when:
 *    - Amount checkbox is toggled
 *    - Amount value changes (if checkbox is enabled)
 *    - Theme changes
 *
 * Layout Structure:
 *
 * QDialog
 * └── QVBoxLayout (main)
 *     ├── QLabel (title)
 *     ├── QLabel (subtitle)
 *     ├── QGroupBox (QR Code)
 *     │   └── QVBoxLayout
 *     │       ├── QLabel (QR image)
 *     │       └── QLabel (status)
 *     ├── QGroupBox (Address)
 *     │   └── QVBoxLayout
 *     │       ├── QLabel (address label)
 *     │       └── QHBoxLayout
 *     │           ├── QLineEdit (address - read-only)
 *     │           └── QPushButton (copy)
 *     ├── QGroupBox (Payment Request - Optional)
 *     │   └── QVBoxLayout
 *     │       ├── QCheckBox (include amount)
 *     │       ├── QLabel (amount label)
 *     │       ├── QDoubleSpinBox (amount input)
 *     │       └── QLabel (note)
 *     └── QHBoxLayout (buttons)
 *         └── QPushButton (close)
 *
 * Styling:
 * - Uses QtThemeManager for all colors and spacing
 * - Follows same patterns as QtSendDialog
 * - QGroupBox with rounded borders (8px radius)
 * - Accent color for primary button and focus states
 * - Surface color for input backgrounds
 * - Dimmed text color for secondary information
 */

// ============================================================================
// FUTURE ENHANCEMENTS
// ============================================================================

/*
 * Potential improvements:
 *
 * 1. Save QR Code Image
 *    - Add "Save QR Code" button
 *    - Export as PNG/SVG file
 *    - Include address in filename
 *
 * 2. Print QR Code
 *    - Add "Print" button
 *    - Print preview dialog
 *    - Include address text below QR code
 *
 * 3. Share Options
 *    - Email QR code
 *    - Share via messaging apps
 *    - Generate shareable link
 *
 * 4. Payment Memo/Label
 *    - Add optional memo field
 *    - Include in payment URI
 *    - Store with transaction history
 *
 * 5. Multiple Address Support
 *    - Dropdown to select from multiple addresses
 *    - Generate new address button
 *    - Address history
 *
 * 6. Transaction Monitoring
 *    - Show incoming transactions in real-time
 *    - Display confirmation status
 *    - Link to block explorer
 *
 * 7. Ethereum Enhancements
 *    - Support ERC-20 tokens
 *    - Token selection dropdown
 *    - Amount in Wei/Gwei/ETH conversion
 *
 * 8. Bitcoin Enhancements
 *    - BIP21 URI with multiple parameters
 *    - SegWit address formats
 *    - Lightning Network invoice support
 */

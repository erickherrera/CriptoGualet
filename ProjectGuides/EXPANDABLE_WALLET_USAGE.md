# Reusable Expandable Wallet Card Component

## Overview
The `QtExpandableWalletCard` is a reusable Qt widget component designed to display cryptocurrency wallet information in an expandable card format. It can be used for any cryptocurrency (Bitcoin, Ethereum, etc.).

## Features
- **Compact collapsed state** showing cryptocurrency logo, name, and balance
- **Expandable view** revealing Send and Receive action buttons
- **Transaction history** display in the expanded section
- **Theme-aware styling** that automatically adapts to the current theme
- **Customizable** for any cryptocurrency

## Usage

### 1. Include the Header
```cpp
#include "QtExpandableWalletCard.h"
#include "QtThemeManager.h"
```

### 2. Create and Configure a Wallet Card

#### Bitcoin Example (already implemented in QtWalletUI.cpp)
```cpp
// Create the Bitcoin wallet card
m_bitcoinWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);

// Configure for Bitcoin
m_bitcoinWalletCard->setCryptocurrency("Bitcoin", "BTC", "₿");
m_bitcoinWalletCard->setBalance("0.00000000 BTC");
m_bitcoinWalletCard->setTransactionHistory(
    "No transactions yet.<br><br>This is a demo wallet.");

// Connect signals
connect(m_bitcoinWalletCard, &QtExpandableWalletCard::sendRequested,
        this, &QtWalletUI::onSendBitcoinClicked);
connect(m_bitcoinWalletCard, &QtExpandableWalletCard::receiveRequested,
        this, &QtWalletUI::onReceiveBitcoinClicked);

// Add to layout
m_contentLayout->addWidget(m_bitcoinWalletCard);
```

#### Ethereum Example
```cpp
// Create an Ethereum wallet card
auto ethereumWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);

// Configure for Ethereum
ethereumWalletCard->setCryptocurrency("Ethereum", "ETH", "Ξ");
ethereumWalletCard->setBalance("0.0000 ETH");
ethereumWalletCard->setTransactionHistory("No transactions yet.");

// Connect signals
connect(ethereumWalletCard, &QtExpandableWalletCard::sendRequested,
        this, &QtWalletUI::onSendEthereumClicked);
connect(ethereumWalletCard, &QtExpandableWalletCard::receiveRequested,
        this, &QtWalletUI::onReceiveEthereumClicked);

// Add to layout
m_contentLayout->addWidget(ethereumWalletCard);
```

#### Litecoin Example
```cpp
auto litecoinWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);

litecoinWalletCard->setCryptocurrency("Litecoin", "LTC", "Ł");
litecoinWalletCard->setBalance("0.00000000 LTC");
litecoinWalletCard->setTransactionHistory("No transactions yet.");

connect(litecoinWalletCard, &QtExpandableWalletCard::sendRequested,
        this, &QtWalletUI::onSendLitecoinClicked);
connect(litecoinWalletCard, &QtExpandableWalletCard::receiveRequested,
        this, &QtWalletUI::onReceiveLitecoinClicked);

m_contentLayout->addWidget(litecoinWalletCard);
```

### 3. Update Balance and Transaction History

#### Update Balance
```cpp
// Update balance dynamically
m_bitcoinWalletCard->setBalance(QString("%1 BTC").arg(balance, 0, 'f', 8));
```

#### Update Transaction History
```cpp
// Update transaction history with HTML formatting
QString historyHtml;
for (const auto &tx : transactions) {
    historyHtml += QString("<b>%1:</b> %2 BTC<br>Time: %3<br>Status: %4<br><br>")
        .arg(tx.type)
        .arg(QString::number(tx.amount, 'f', 8))
        .arg(tx.timestamp)
        .arg(tx.status);
}
m_bitcoinWalletCard->setTransactionHistory(historyHtml);
```

### 4. Apply Theme Changes
```cpp
// When theme changes, update all wallet cards
void QtWalletUI::onThemeChanged() {
    applyTheme();

    if (m_bitcoinWalletCard) {
        m_bitcoinWalletCard->applyTheme();
    }
    if (m_ethereumWalletCard) {
        m_ethereumWalletCard->applyTheme();
    }
    // Add more wallet cards as needed
}
```

### 5. Access Send/Receive Buttons Directly (Optional)
```cpp
// You can also access the buttons directly if needed
QPushButton* sendBtn = m_bitcoinWalletCard->sendButton();
QPushButton* receiveBtn = m_bitcoinWalletCard->receiveButton();

// Customize button behavior
sendBtn->setEnabled(balance > 0);
```

## API Reference

### Constructor
```cpp
QtExpandableWalletCard(QtThemeManager *themeManager, QWidget *parent = nullptr)
```

### Configuration Methods
```cpp
// Set cryptocurrency details
void setCryptocurrency(const QString &name, const QString &symbol, const QString &logoText);

// Update balance display
void setBalance(const QString &balance);

// Update transaction history (supports HTML)
void setTransactionHistory(const QString &historyHtml);

// Apply current theme
void applyTheme();
```

### Access Methods
```cpp
// Get direct access to action buttons
QPushButton* sendButton() const;
QPushButton* receiveButton() const;
```

### Signals
```cpp
// Emitted when user clicks Send button
void sendRequested();

// Emitted when user clicks Receive button
void receiveRequested();
```

## Customization Tips

### 1. Custom Logo Text
You can use any Unicode symbol for the logo:
- Bitcoin: "₿"
- Ethereum: "Ξ" or "Ⓔ"
- Litecoin: "Ł"
- Dogecoin: "Ð"
- Monero: "ɱ"
- Or use custom text like "BTC", "ETH", etc.

### 2. Transaction History Formatting
The transaction history supports HTML, so you can customize it:
```cpp
QString html = R"(
<div style="margin-bottom: 10px;">
    <b style="color: #00FF00;">RECEIVED:</b> 0.5 BTC<br>
    From: 1A1zP1eP...<br>
    <span style="color: #888;">Confirmed</span>
</div>
)";
walletCard->setTransactionHistory(html);
```

### 3. Dynamic Balance Updates
```cpp
// Update balance when transactions occur
void updateWalletBalance(double newBalance) {
    QString balanceStr = QString("%1 BTC").arg(newBalance, 0, 'f', 8);
    m_bitcoinWalletCard->setBalance(balanceStr);
}
```

## File Locations
- **Header:** `include/QtExpandableWalletCard.h`
- **Implementation:** `src/QtExpandableWalletCard.cpp`
- **Example Usage:** `src/QtWalletUI.cpp` (see `createActionButtons()` function)

## Theme Integration
The component automatically uses colors from `QtThemeManager`:
- **Surface color** for card background
- **Accent color** for collapsed header background
- **Text color** for primary text
- **Subtitle color** for secondary text (crypto name)

The component responds to theme changes when `applyTheme()` is called.

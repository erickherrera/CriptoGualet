// QtWalletUI.cpp
#include "QtWalletUI.h"
#include "../../backend/blockchain/include/PriceService.h"
#include "../../backend/core/include/Crypto.h"
#include "QtExpandableWalletCard.h"
#include "QtReceiveDialog.h"
#include "QtSendDialog.h"
#include "QtThemeManager.h"
#include "QtTokenImportDialog.h"
#include "QtTokenListWidget.h"
#include "QtTokenCard.h"

#include <QTextEdit>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpacerItem>
#include <QSvgRenderer>
#include <QVBoxLayout>

QtWalletUI::QtWalletUI(QWidget* parent)
    : QWidget(parent),
      m_themeManager(nullptr),
      m_centeringLayout(nullptr),
      m_leftSpacer(nullptr),
      m_rightSpacer(nullptr),
      m_headerSection(nullptr),
      m_headerTitle(nullptr),
      m_balanceTitle(nullptr),
      m_balanceLabel(nullptr),
      m_toggleBalanceButton(nullptr),
      m_refreshButton(nullptr),
      m_bitcoinWalletCard(nullptr),
      m_litecoinWalletCard(nullptr),
      m_ethereumWalletCard(nullptr),
      m_currentMockUser(nullptr),
      m_wallet(nullptr),
      m_litecoinWallet(nullptr),
      m_ethereumWallet(nullptr),
      m_balanceUpdateTimer(nullptr),
      m_realBalanceBTC(0.0),
      m_realBalanceLTC(0.0),
      m_realBalanceETH(0.0),
      m_userRepository(nullptr),
      m_walletRepository(nullptr),
      m_tokenRepository(nullptr),
      m_currentUserId(-1),
       m_tokenImportDialog(nullptr),
       m_tokenListWidget(nullptr),
       m_stablecoinSectionHeader(nullptr),
       m_usdtWalletCard(nullptr),
       m_usdcWalletCard(nullptr),
       m_daiWalletCard(nullptr),
      m_priceFetcher(nullptr),
      m_priceUpdateTimer(nullptr),
      m_currentBTCPrice(43000.0),
      m_currentLTCPrice(70.0),
      m_currentETHPrice(2500.0),
      m_isLoadingBTC(false),
      m_isLoadingLTC(false),
      m_isLoadingETH(false),
      m_statusLabel(nullptr),
      m_balanceVisible(true),
      m_mockMode(false) {
    // Get theme manager SAFELY
    m_themeManager = &QtThemeManager::instance();

    // Initialize mock users first (doesn't touch UI)
    initializeMockUsers();

    // Create all UI widgets
    setupUI();

    // Defer all complex initialization to after event loop starts
    QTimer::singleShot(100, this, [this]() {
        // Initialize price fetcher - ensure proper constructor call
        m_priceFetcher.reset(new PriceService::PriceFetcher());

        // Setup price update timer
        m_priceUpdateTimer = new QTimer(this);
        connect(m_priceUpdateTimer, &QTimer::timeout, this, &QtWalletUI::onPriceUpdateTimer);
        m_priceUpdateTimer->start(60000);  // 60 seconds

        // Setup balance update timer (refresh every 30 seconds)
        m_balanceUpdateTimer = new QTimer(this);
        connect(m_balanceUpdateTimer, &QTimer::timeout, this, &QtWalletUI::onBalanceUpdateTimer);
        m_balanceUpdateTimer->start(30000);  // 30 seconds

        // Apply theme
        if (m_themeManager) {
            applyTheme();
        }

        // PHASE 2: Fetch initial prices for both BTC and ETH
        fetchAllPrices();
    });

    // Connect theme changed signal
    if (m_themeManager) {
        connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtWalletUI::onThemeChanged);
    }
}

void QtWalletUI::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    // Use more compact margins - reduced from standardMargin to optimized values
    // Initial margins will be overridden by updateResponsiveLayout() but these provide good
    // defaults
    int defaultMargin = 16;  // Reduced from standardMargin (~20-24px)
    m_mainLayout->setContentsMargins(defaultMargin, defaultMargin, defaultMargin, defaultMargin);
    m_mainLayout->setSpacing(16);  // Reduced from standardSpacing (~20px)

    // Create a horizontal layout to center content with max width
    m_centeringLayout = new QHBoxLayout();

    // Add left spacer (will be dynamically controlled for full-width on laptops)
    m_leftSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_centeringLayout->addItem(m_leftSpacer);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_scrollContent = new QWidget();
    m_contentLayout = new QVBoxLayout(m_scrollContent);
    // More compact content margins - optimized for better space utilization
    int contentMargin = 16;  // Reduced from 20 for tighter layout
    m_contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
    m_contentLayout->setSpacing(16);  // Reduced from 20 for more compact layout

    createHeaderSection();
    createActionButtons();

    // Optimized bottom spacer - reduced from 20,40 to 12,24 for better space utilization
    m_contentLayout->addItem(new QSpacerItem(12, 24, QSizePolicy::Minimum, QSizePolicy::Expanding));

    m_scrollArea->setWidget(m_scrollContent);

    // Add scroll area to centering layout
    m_centeringLayout->addWidget(m_scrollArea);

    // Add right spacer (will be dynamically controlled for full-width on laptops)
    m_rightSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_centeringLayout->addItem(m_rightSpacer);

    m_mainLayout->addLayout(m_centeringLayout);

    // Initialize responsive layout after UI setup - add safety delay and null checks
    QTimer::singleShot(100, this, [this]() {
        // Add safety check before updating layouts
        if (this && m_mainLayout && m_contentLayout) {
            updateResponsiveLayout();
            adjustButtonLayout();
            updateCardSizes();
        } else {
            qDebug() << "QtWalletUI: Skipping layout updates due to null pointers";
        }
    });
}

void QtWalletUI::createHeaderSection() {
    m_headerSection = new QWidget(m_scrollContent);

    QVBoxLayout* headerLayout = new QVBoxLayout(m_headerSection);
    // Increased header padding for better visual hierarchy
    headerLayout->setContentsMargins(0, 15, 0, 25);
    headerLayout->setSpacing(15);

    // Digital Wallets title
    m_headerTitle = new QLabel("Digital Wallets", m_headerSection);
    m_headerTitle->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(m_headerTitle);

    // Balance section
    QVBoxLayout* balanceVerticalLayout = new QVBoxLayout();
    balanceVerticalLayout->setSpacing(5);

    m_balanceTitle = new QLabel("Total Balance", m_headerSection);
    m_balanceTitle->setAlignment(Qt::AlignCenter);
    balanceVerticalLayout->addWidget(m_balanceTitle);

    // Horizontal layout for amount and toggle button - optimized spacing
    QHBoxLayout* balanceRowLayout = new QHBoxLayout();
    balanceRowLayout->setSpacing(8);  // Kept 8 for good visual separation
    balanceRowLayout->setAlignment(Qt::AlignCenter);

    // Balance amount
    m_balanceLabel = new QLabel("$0.00 USD", m_headerSection);
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    balanceRowLayout->addWidget(m_balanceLabel);

    // Toggle button - size optimized for better proportions with safety bounds
    m_toggleBalanceButton = new QPushButton(m_headerSection);
    m_toggleBalanceButton->setIconSize(QSize(18, 18));  // Reduced from 20,20
    m_toggleBalanceButton->setFixedSize(28, 28);        // Reduced from 32,32 for better scale
    m_toggleBalanceButton->setToolTip(
        "Hide/Show Balance (Ctrl+H)");  // PHASE 3: Accessibility tooltip
    m_toggleBalanceButton->setCursor(Qt::PointingHandCursor);
    m_toggleBalanceButton->setAccessibleName(
        "Toggle Balance Visibility");  // PHASE 3: Screen reader support
    m_toggleBalanceButton->setAccessibleDescription("Press to toggle balance visibility");
    balanceRowLayout->addWidget(m_toggleBalanceButton);

    // PHASE 3: Manual refresh button - size optimized
    m_refreshButton = new QPushButton(m_headerSection);
    m_refreshButton->setText("🔄");
    m_refreshButton->setFixedSize(28, 28);  // Reduced from 32,32 for consistency
    m_refreshButton->setToolTip(
        "Refresh balances and prices (F5 or Ctrl+R)");  // PHASE 3: Accessibility tooltip
    m_refreshButton->setCursor(Qt::PointingHandCursor);
    m_refreshButton->setAccessibleName("Refresh Balances");  // PHASE 3: Screen reader support
    m_refreshButton->setAccessibleDescription(
        "Press to refresh wallet balances and cryptocurrency prices");
    balanceRowLayout->addWidget(m_refreshButton);

    balanceVerticalLayout->addLayout(balanceRowLayout);
    headerLayout->addLayout(balanceVerticalLayout);

    // PHASE 2: Status label for loading/error messages - more compact
    m_statusLabel = new QLabel("", m_headerSection);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setVisible(false);  // Hidden by default
    headerLayout->addWidget(m_statusLabel);

    connect(m_toggleBalanceButton, &QPushButton::clicked, this,
            &QtWalletUI::onToggleBalanceClicked);
    connect(m_refreshButton, &QPushButton::clicked, this,
            &QtWalletUI::onRefreshClicked);  // PHASE 3: Connect refresh button

    m_contentLayout->addWidget(m_headerSection);
}

void QtWalletUI::createActionButtons() {
    // Create Bitcoin wallet card using the reusable component
    m_bitcoinWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);
    m_bitcoinWalletCard->setCryptocurrency("Bitcoin", "BTC", "₿");
    m_bitcoinWalletCard->setBalance("0.00000000 BTC");
    m_bitcoinWalletCard->setTransactionHistory(
        "No transactions yet.<br><br>This is a demo wallet. In a real "
        "implementation, transaction history would be displayed here.");

    // Connect signals
    connect(m_bitcoinWalletCard, &QtExpandableWalletCard::sendRequested, this,
            &QtWalletUI::onSendBitcoinClicked);
    connect(m_bitcoinWalletCard, &QtExpandableWalletCard::receiveRequested, this,
            &QtWalletUI::onReceiveBitcoinClicked);

    m_contentLayout->addWidget(m_bitcoinWalletCard);

    // Create Litecoin wallet card
    m_litecoinWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);
    m_litecoinWalletCard->setCryptocurrency("Litecoin", "LTC", "Ł");
    m_litecoinWalletCard->setBalance("0.00000000 LTC");
    m_litecoinWalletCard->setTransactionHistory(
        "No transactions yet.<br><br>This wallet supports Litecoin network. "
        "Transaction history will be displayed here.");

    // Connect Litecoin signals
    connect(m_litecoinWalletCard, &QtExpandableWalletCard::sendRequested, this,
            &QtWalletUI::onSendLitecoinClicked);
    connect(m_litecoinWalletCard, &QtExpandableWalletCard::receiveRequested, this,
            &QtWalletUI::onReceiveLitecoinClicked);

    m_contentLayout->addWidget(m_litecoinWalletCard);

    // Create Ethereum wallet card
    m_ethereumWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);
    m_ethereumWalletCard->setCryptocurrency("Ethereum", "ETH", "Ξ");
    m_ethereumWalletCard->setBalance("0.00000000 ETH");
    m_ethereumWalletCard->setTransactionHistory(
        "No transactions yet.<br><br>This wallet supports Ethereum network. "
        "Transaction history will be displayed here.");

    // PHASE 2: Connect Ethereum handlers
    connect(m_ethereumWalletCard, &QtExpandableWalletCard::sendRequested, this,
            &QtWalletUI::onSendEthereumClicked);
    connect(m_ethereumWalletCard, &QtExpandableWalletCard::receiveRequested, this,
            &QtWalletUI::onReceiveEthereumClicked);

    m_contentLayout->addWidget(m_ethereumWalletCard);

    // Import ERC20 Token button for Ethereum ecosystem
    QHBoxLayout* tokenActionsLayout = new QHBoxLayout();
    tokenActionsLayout->setSpacing(10);
    tokenActionsLayout->setAlignment(Qt::AlignCenter);

    m_importTokenButton = new QPushButton("Import ERC20 Token", this);
    m_importTokenButton->setToolTip("Import any ERC20 token by contract address to your Ethereum wallet.");
    m_importTokenButton->setCursor(Qt::PointingHandCursor);
    m_importTokenButton->setObjectName("importTokenButton");

    connect(m_importTokenButton, &QPushButton::clicked, this, &QtWalletUI::onImportTokenClicked);

    tokenActionsLayout->addStretch();
    tokenActionsLayout->addWidget(m_importTokenButton);
    tokenActionsLayout->addStretch();

    m_contentLayout->addLayout(tokenActionsLayout);

    // ============================================
    // STABLECOINS SECTION
    // ============================================
    
    // Section header for Stablecoins
    m_stablecoinSectionHeader = new QLabel("Stablecoins", m_scrollContent);
    m_stablecoinSectionHeader->setObjectName("sectionHeader");
    m_stablecoinSectionHeader->setAlignment(Qt::AlignLeft);
    m_contentLayout->addWidget(m_stablecoinSectionHeader);

    // Create USDT wallet card
    m_usdtWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);
    m_usdtWalletCard->setCryptocurrency("Tether USD", "USDT", "$");
    m_usdtWalletCard->setBalance("0.00 USDT");
    m_usdtWalletCard->setTransactionHistory(
        "No transactions yet.<br><br>USDT (Tether) is a stablecoin pegged to the US Dollar. "
        "It operates on the Ethereum network as an ERC20 token.");

    // Connect USDT signals
    connect(m_usdtWalletCard, &QtExpandableWalletCard::sendRequested, this,
            &QtWalletUI::onSendUSDTClicked);
    connect(m_usdtWalletCard, &QtExpandableWalletCard::receiveRequested, this,
            &QtWalletUI::onReceiveUSDTClicked);

    m_contentLayout->addWidget(m_usdtWalletCard);

    // Create USDC wallet card
    m_usdcWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);
    m_usdcWalletCard->setCryptocurrency("USD Coin", "USDC", "$");
    m_usdcWalletCard->setBalance("0.00 USDC");
    m_usdcWalletCard->setTransactionHistory(
        "No transactions yet.<br><br>USDC (USD Coin) is a stablecoin pegged to the US Dollar. "
        "It operates on the Ethereum network as an ERC20 token.");

    // Connect USDC signals
    connect(m_usdcWalletCard, &QtExpandableWalletCard::sendRequested, this,
            &QtWalletUI::onSendUSDCClicked);
    connect(m_usdcWalletCard, &QtExpandableWalletCard::receiveRequested, this,
            &QtWalletUI::onReceiveUSDCClicked);

    m_contentLayout->addWidget(m_usdcWalletCard);

    // Create DAI wallet card
    m_daiWalletCard = new QtExpandableWalletCard(m_themeManager, m_scrollContent);
    m_daiWalletCard->setCryptocurrency("Dai Stablecoin", "DAI", "◈");
    m_daiWalletCard->setBalance("0.00 DAI");
    m_daiWalletCard->setTransactionHistory(
        "No transactions yet.<br><br>DAI is a decentralized stablecoin pegged to the US Dollar. "
        "It operates on the Ethereum network as an ERC20 token.");

    // Connect DAI signals
    connect(m_daiWalletCard, &QtExpandableWalletCard::sendRequested, this,
            &QtWalletUI::onSendDAIClicked);
    connect(m_daiWalletCard, &QtExpandableWalletCard::receiveRequested, this,
            &QtWalletUI::onReceiveDAIClicked);

    m_contentLayout->addWidget(m_daiWalletCard);
}

void QtWalletUI::setUserInfo(const QString& username, const QString& address) {
    m_currentUsername = username;
    m_currentAddress = address;

    // Fetch real balance when address is set
    if (m_wallet && !address.isEmpty()) {
        fetchRealBalance();
    }
}

void QtWalletUI::onViewBalanceClicked() {
    if (!m_currentMockUser) {
        return;
    }

    double usdBalance = m_currentMockUser->balance * m_currentBTCPrice;

    QMessageBox::information(
        this, "Balance Updated",
        QString("Your current balance:\n%1 BTC\n$%L2 USD\n\nBTC Price: $%L3\n\nThis is "
                "mock data for testing purposes.")
            .arg(QString::number(m_currentMockUser->balance, 'f', 8))
            .arg(usdBalance, 0, 'f', 2)
            .arg(m_currentBTCPrice, 0, 'f', 2));

    emit viewBalanceRequested();
}

void QtWalletUI::onSendBitcoinClicked() {
    // Determine current balance to use
    double currentBalance =
        m_mockMode ? (m_currentMockUser ? m_currentMockUser->balance : 0.0) : m_realBalanceBTC;

    // Create and show send dialog
    QtSendDialog dialog(QtSendDialog::ChainType::BITCOIN, currentBalance, m_currentBTCPrice, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto txDataOpt = dialog.getTransactionData();
        if (!txDataOpt.has_value()) {
            return;
        }

        auto txData = txDataOpt.value();

        // Handle mock mode differently
        if (m_mockMode) {
            // Mock transaction - just update the UI
            QMessageBox::information(
                this, "Mock Transaction Sent",
                QString("Mock transaction of %1 BTC sent to:\n%2\n\n"
                        "This is a demo transaction. In real mode, actual Bitcoin would be sent.")
                    .arg(txData.amountBTC, 0, 'f', 8)
                    .arg(txData.recipientAddress));

            // Update mock balance
            if (m_currentMockUser) {
                double feeBTC = static_cast<double>(txData.estimatedFeeSatoshis) / 100000000.0;
                m_currentMockUser->balance -= (txData.amountBTC + feeBTC);
                updateUSDBalance();
                if (m_bitcoinWalletCard) {
                    m_bitcoinWalletCard->setBalance(
                        QString("%1 BTC").arg(m_currentMockUser->balance, 0, 'f', 8));
                }
            }
        } else {
            // Real transaction mode
            if (!m_wallet) {
                QMessageBox::critical(this, "Error", "Wallet not initialized");
                return;
            }

            try {
                // Show progress indicator
                QMessageBox* progressBox = new QMessageBox(this);
                progressBox->setWindowTitle("Sending Transaction");
                progressBox->setText("Broadcasting transaction to the network...\nPlease wait.");
                progressBox->setStandardButtons(QMessageBox::NoButton);
                progressBox->setModal(true);
                progressBox->show();
                QApplication::processEvents();

                // Send the real transaction
                sendRealTransaction(txData.recipientAddress, txData.amountSatoshis,
                                    txData.estimatedFeeSatoshis, txData.password);

                progressBox->close();
                delete progressBox;

                // Refresh balance after sending
                fetchRealBalance();
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Transaction Failed",
                                      QString("Failed to send transaction:\n%1").arg(e.what()));
            }
        }
    }

    emit sendBitcoinRequested();
}

void QtWalletUI::onReceiveBitcoinClicked() {
    // Show Bitcoin receive dialog with current address
    QtReceiveDialog dialog(QtReceiveDialog::ChainType::BITCOIN, m_currentAddress, this);
    dialog.exec();

    emit receiveBitcoinRequested();
}

void QtWalletUI::onSendLitecoinClicked() {
    // Check if Litecoin wallet is available
    if (!m_litecoinWallet) {
        QMessageBox::warning(this, "Litecoin Wallet Not Available",
                             "Litecoin wallet is not initialized. Please restart the application.");
        return;
    }

    // Determine current balance
    double currentBalance = m_realBalanceLTC;

    // Create and show send dialog for Litecoin
    QtSendDialog dialog(QtSendDialog::ChainType::LITECOIN, currentBalance, m_currentLTCPrice, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto txDataOpt = dialog.getTransactionData();
        if (!txDataOpt.has_value()) {
            return;
        }

        auto txData = txDataOpt.value();

        try {
            // Show progress indicator
            QMessageBox* progressBox = new QMessageBox(this);
            progressBox->setWindowTitle("Sending Litecoin Transaction");
            progressBox->setText("Broadcasting transaction to the Litecoin network...\nPlease wait.");
            progressBox->setStandardButtons(QMessageBox::NoButton);
            progressBox->setModal(true);
            progressBox->show();
            QApplication::processEvents();

            // Derive private key for the Litecoin address
            std::vector<uint8_t> privateKeyBytes =
                derivePrivateKeyForAddress(m_litecoinAddress, txData.password);

            // Create private keys map
            std::map<std::string, std::vector<uint8_t>> privateKeysMap;
            std::string fromAddress = m_litecoinAddress.toStdString();
            privateKeysMap[fromAddress] = privateKeyBytes;

            // Prepare addresses
            std::vector<std::string> fromAddresses = {fromAddress};
            std::string toAddress = txData.recipientAddress.toStdString();

            // Send the Litecoin transaction
            auto result = m_litecoinWallet->SendFunds(
                fromAddresses, toAddress, txData.amountLitoshis,
                privateKeysMap, txData.estimatedFeeLitoshis);

            progressBox->close();
            delete progressBox;

            if (result.success) {
                QMessageBox::information(
                    this, "Transaction Sent",
                    QString("Transaction sent successfully!\n\n"
                            "Transaction Hash:\n%1\n\n"
                            "Amount: %2 LTC\n"
                            "Fee: %3 LTC\n\n"
                            "You can track your transaction on a Litecoin block explorer.")
                        .arg(QString::fromStdString(result.transaction_hash))
                        .arg(m_litecoinWallet->ConvertLitoshisToLTC(txData.amountLitoshis), 0, 'f', 8)
                        .arg(m_litecoinWallet->ConvertLitoshisToLTC(result.total_fees), 0, 'f', 8));

                // Refresh balance after sending
                fetchRealBalance();
            } else {
                QMessageBox::critical(this, "Transaction Failed",
                                      QString("Failed to send transaction:\n%1")
                                          .arg(QString::fromStdString(result.error_message)));
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Transaction Failed",
                                  QString("Failed to send transaction:\n%1").arg(e.what()));
        }
    }

    emit sendLitecoinRequested();
}

void QtWalletUI::onReceiveLitecoinClicked() {
    // Show Litecoin receive dialog with Litecoin address
    if (m_litecoinAddress.isEmpty()) {
        QMessageBox::warning(
            this, "No Litecoin Address",
            "Litecoin address not available. Please ensure your wallet is set up correctly.");
        return;
    }

    QtReceiveDialog dialog(QtReceiveDialog::ChainType::LITECOIN, m_litecoinAddress, this);
    dialog.exec();

    emit receiveLitecoinRequested();
}

// PHASE 2: Separate Ethereum receive handler
void QtWalletUI::onReceiveEthereumClicked() {
    // Show Ethereum receive dialog with Ethereum address
    if (m_ethereumAddress.isEmpty()) {
        QMessageBox::warning(
            this, "No Ethereum Address",
            "Ethereum address not available. Please ensure your wallet is set up correctly.");
        return;
    }

    QtReceiveDialog dialog(QtReceiveDialog::ChainType::ETHEREUM, m_ethereumAddress, this);
    dialog.exec();

    emit receiveEthereumRequested();
}

void QtWalletUI::onSendEthereumClicked() {
    // Check if Ethereum wallet is available
    if (!m_ethereumWallet) {
        QMessageBox::warning(this, "Ethereum Wallet Not Available",
                             "Ethereum wallet is not initialized. Please restart the application.");
        return;
    }

    // Determine current balance
    double currentBalance = m_realBalanceETH;

    // Create and show send dialog for Ethereum
    QtSendDialog dialog(QtSendDialog::ChainType::ETHEREUM, currentBalance, m_currentETHPrice, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto txDataOpt = dialog.getTransactionData();
        if (!txDataOpt.has_value()) {
            return;
        }

        auto txData = txDataOpt.value();

        // Show confirmation
        QString confirmMsg = QString(
                                 "You are about to send %1 ETH to:\n%2\n\n"
                                 "Gas Price: %3 Gwei\n"
                                 "Gas Limit: %4\n"
                                 "Total Cost: %5 ETH\n\n"
                                 "This action cannot be undone. Continue?")
                                 .arg(QString::number(txData.amountETH, 'f', 8))
                                 .arg(txData.recipientAddress)
                                 .arg(txData.gasPriceGwei)
                                 .arg(txData.gasLimit)
                                 .arg(QString::number(txData.totalCostETH, 'f', 8));

        int response = QMessageBox::question(this, "Confirm Ethereum Transaction", confirmMsg,
                                             QMessageBox::Yes | QMessageBox::No);

        if (response == QMessageBox::Yes) {
            try {
                // Show progress indicator
                QMessageBox* progressBox = new QMessageBox(this);
                progressBox->setWindowTitle("Sending Ethereum Transaction");
                progressBox->setText(
                    "Broadcasting transaction to the Ethereum network...\nPlease wait.");
                progressBox->setStandardButtons(QMessageBox::NoButton);
                progressBox->setModal(true);
                progressBox->show();
                QApplication::processEvents();

                // Derive private key for the Ethereum address
                std::vector<uint8_t> privateKeyBytes =
                    derivePrivateKeyForAddress(m_ethereumAddress, txData.password);

                // Convert private key to hex string
                QString privateKeyHex;
                for (uint8_t byte : privateKeyBytes) {
                    privateKeyHex += QString("%1").arg(byte, 2, 16, QChar('0'));
                }

                // Send the Ethereum transaction
                auto result = m_ethereumWallet->SendFunds(
                    m_ethereumAddress.toStdString(), txData.recipientAddress.toStdString(),
                    txData.amountETH, privateKeyHex.toStdString(),
                    txData.gasPriceGwei.toStdString(), txData.gasLimit);

                progressBox->close();
                delete progressBox;

                if (result.success) {
                    QMessageBox::information(
                        this, "Transaction Sent",
                        QString("Transaction sent successfully!\n\n"
                                "Transaction Hash:\n%1\n\n"
                                "Total Cost: %2 ETH ($%3)\n\n"
                                "You can track your transaction on Etherscan.")
                            .arg(QString::fromStdString(result.transaction_hash))
                            .arg(QString::number(result.total_cost_eth, 'f', 8))
                            .arg(QString::number(result.total_cost_eth * m_currentETHPrice, 'f',
                                                 2)));

                    // Refresh balance after sending
                    fetchRealBalance();
                } else {
                    QMessageBox::critical(this, "Transaction Failed",
                                          QString("Failed to send transaction:\n%1")
                                              .arg(QString::fromStdString(result.error_message)));
                }
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Transaction Failed",
                                      QString("Failed to send transaction:\n%1").arg(e.what()));
            }
        }
    }
}

void QtWalletUI::onThemeChanged() {
    applyTheme();
    if (m_bitcoinWalletCard) {
        m_bitcoinWalletCard->applyTheme();
    }
    if (m_litecoinWalletCard) {
        m_litecoinWalletCard->applyTheme();
    }
    if (m_ethereumWalletCard) {
        m_ethereumWalletCard->applyTheme();
    }
    // Apply theme to stablecoin cards
    if (m_usdtWalletCard) {
        m_usdtWalletCard->applyTheme();
    }
    if (m_usdcWalletCard) {
        m_usdcWalletCard->applyTheme();
    }
    if (m_daiWalletCard) {
        m_daiWalletCard->applyTheme();
    }
}

// ============================================
// STABLECOIN HANDLERS
// ============================================

void QtWalletUI::onSendUSDTClicked() {
    // USDT uses the same Ethereum address - show info that this is an ERC20 token
    if (m_ethereumAddress.isEmpty()) {
        QMessageBox::warning(this, "No Ethereum Address",
                             "Ethereum address not available. USDT is an ERC20 token on Ethereum.\n\n"
                             "Please ensure your Ethereum wallet is set up correctly.");
        return;
    }
    
    QMessageBox::information(this, "Send USDT",
                             "To send USDT, use your Ethereum wallet.\n\n"
                             "USDT (Tether) is an ERC20 token that operates on the Ethereum network. "
                             "You will need ETH in your wallet to pay for gas fees.\n\n"
                             "Contract Address:\n0xdAC17F958D2ee523a2206206994597C13D831ec7");
}

void QtWalletUI::onReceiveUSDTClicked() {
    if (m_ethereumAddress.isEmpty()) {
        QMessageBox::warning(this, "No Ethereum Address",
                             "Ethereum address not available. USDT uses the same address as Ethereum.\n\n"
                             "Please ensure your Ethereum wallet is set up correctly.");
        return;
    }
    
    // USDT uses the same address as Ethereum
    QtReceiveDialog dialog(QtReceiveDialog::ChainType::ETHEREUM, m_ethereumAddress, this);
    dialog.setWindowTitle("Receive USDT");
    dialog.exec();
}

void QtWalletUI::onSendUSDCClicked() {
    if (m_ethereumAddress.isEmpty()) {
        QMessageBox::warning(this, "No Ethereum Address",
                             "Ethereum address not available. USDC is an ERC20 token on Ethereum.\n\n"
                             "Please ensure your Ethereum wallet is set up correctly.");
        return;
    }
    
    QMessageBox::information(this, "Send USDC",
                             "To send USDC, use your Ethereum wallet.\n\n"
                             "USDC (USD Coin) is an ERC20 token that operates on the Ethereum network. "
                             "You will need ETH in your wallet to pay for gas fees.\n\n"
                             "Contract Address:\n0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48");
}

void QtWalletUI::onReceiveUSDCClicked() {
    if (m_ethereumAddress.isEmpty()) {
        QMessageBox::warning(this, "No Ethereum Address",
                             "Ethereum address not available. USDC uses the same address as Ethereum.\n\n"
                             "Please ensure your Ethereum wallet is set up correctly.");
        return;
    }
    
    // USDC uses the same address as Ethereum
    QtReceiveDialog dialog(QtReceiveDialog::ChainType::ETHEREUM, m_ethereumAddress, this);
    dialog.setWindowTitle("Receive USDC");
    dialog.exec();
}

void QtWalletUI::onSendDAIClicked() {
    if (m_ethereumAddress.isEmpty()) {
        QMessageBox::warning(this, "No Ethereum Address",
                             "Ethereum address not available. DAI is an ERC20 token on Ethereum.\n\n"
                             "Please ensure your Ethereum wallet is set up correctly.");
        return;
    }
    
    QMessageBox::information(this, "Send DAI",
                             "To send DAI, use your Ethereum wallet.\n\n"
                             "DAI is an ERC20 token that operates on the Ethereum network. "
                             "You will need ETH in your wallet to pay for gas fees.\n\n"
                             "Contract Address:\n0x6B175474E89094C44Da98b954EedeAC495271d0F");
}

void QtWalletUI::onReceiveDAIClicked() {
    if (m_ethereumAddress.isEmpty()) {
        QMessageBox::warning(this, "No Ethereum Address",
                             "Ethereum address not available. DAI uses the same address as Ethereum.\n\n"
                             "Please ensure your Ethereum wallet is set up correctly.");
        return;
    }
    
    // DAI uses the same address as Ethereum
    QtReceiveDialog dialog(QtReceiveDialog::ChainType::ETHEREUM, m_ethereumAddress, this);
    dialog.setWindowTitle("Receive DAI");
    dialog.exec();
}

void QtWalletUI::onImportTokenClicked() {
    // Validate prerequisites with comprehensive checks
    if (!m_ethereumWallet) {
        QMessageBox::warning(this, "Wallet Unavailable",
                             "Your Ethereum wallet is not initialized.\n\n"
                             "Please restart the application and try again.");
        return;
    }

    if (!m_tokenRepository || !m_walletRepository) {
        QMessageBox::critical(this, "Configuration Error",
                              "Token storage is not configured properly.\n\n"
                              "Please restart the application.");
        return;
    }

    if (m_currentUserId < 0) {
        QMessageBox::warning(this, "Not Logged In",
                             "Please log in to import tokens.");
        return;
    }

    // Get Ethereum wallet ID upfront to fail fast
    int ethereumWalletId = getEthereumWalletId();
    if (ethereumWalletId == -1) {
        QMessageBox::warning(this, "No Ethereum Wallet",
                             "No Ethereum wallet found for your account.\n\n"
                             "Please create an Ethereum wallet first.");
        return;
    }

    // Create and show the import dialog
    QtTokenImportDialog* dialog = new QtTokenImportDialog(this);
    dialog->setThemeManager(m_themeManager);
    dialog->setEthereumWallet(m_ethereumWallet);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    if (dialog->exec() != QDialog::Accepted) {
        return;
    }

    auto importData = dialog->getImportData();
    if (!importData.has_value()) {
        return;
    }

    // Validate contract address format
    QString tokenAddress = importData->contractAddress.trimmed();
    if (tokenAddress.isEmpty() || tokenAddress.length() != 42 || !tokenAddress.startsWith("0x")) {
        QMessageBox::warning(this, "Invalid Address",
                             "The contract address format is invalid.\n\n"
                             "Please enter a valid Ethereum address (42 characters starting with 0x).");
        return;
    }

    // Show progress indicator with null check
    setLoadingState(true, "ETH");
    if (m_importTokenButton) {
        m_importTokenButton->setEnabled(false);
        m_importTokenButton->setText("Importing...");
    }
    QApplication::processEvents();

    // Perform the import with exception handling
    WalletAPI::ImportTokenResult importResult;
    try {
        importResult = m_ethereumWallet->importERC20Token(
            ethereumWalletId, tokenAddress.toStdString(), *m_tokenRepository);
    } catch (const std::exception& e) {
        // Reset loading state
        setLoadingState(false, "ETH");
        if (m_importTokenButton) {
            m_importTokenButton->setEnabled(true);
            m_importTokenButton->setText("Import ERC20 Token");
        }
        QMessageBox::critical(this, "Import Error",
                              QString("An unexpected error occurred while importing the token:\n\n%1")
                                  .arg(e.what()));
        return;
    }

    // Reset loading state
    setLoadingState(false, "ETH");
    if (m_importTokenButton) {
        m_importTokenButton->setEnabled(true);
        m_importTokenButton->setText("Import ERC20 Token");
    }

    if (!importResult.success) {
        QString errorMsg = QString::fromStdString(importResult.error_message);

        // Provide user-friendly error messages
        if (errorMsg.contains("already", Qt::CaseInsensitive)) {
            QMessageBox::information(this, "Token Already Imported",
                                     "This token is already in your wallet.");
        } else if (errorMsg.contains("invalid", Qt::CaseInsensitive) || 
                   errorMsg.contains("not found", Qt::CaseInsensitive) ||
                   errorMsg.contains("contract", Qt::CaseInsensitive)) {
            QMessageBox::warning(this, "Invalid Token",
                                 "The contract address does not appear to be a valid ERC20 token.\n\n"
                                 "Please verify the address and try again.");
        } else if (errorMsg.contains("network", Qt::CaseInsensitive) || 
                   errorMsg.contains("connection", Qt::CaseInsensitive)) {
            QMessageBox::warning(this, "Network Error",
                                 "Unable to connect to the Ethereum network.\n\n"
                                 "Please check your internet connection and try again.");
        } else {
            QMessageBox::critical(this, "Import Failed",
                                  QString("Failed to import token:\n\n%1").arg(errorMsg));
        }
        return;
    }

    // Validate token_info before accessing
    if (!importResult.token_info) {
        QMessageBox::critical(this, "Import Error",
                              "Token was imported but token information is unavailable.\n\n"
                              "Please try refreshing your token list.");
        return;
    }

    // Create token card data
    TokenCardData tokenData;
    tokenData.contractAddress = tokenAddress;
    tokenData.name = QString::fromStdString(importResult.token_info->name);
    tokenData.symbol = QString::fromStdString(importResult.token_info->symbol);
    tokenData.decimals = importResult.token_info->decimals;
    tokenData.balance = "Loading...";
    tokenData.balanceUSD = "";

    // Add to token list widget
    if (m_tokenListWidget) {
        m_tokenListWidget->addToken(tokenData);
    }

    // Show success notification
    QMessageBox::information(this, "Token Imported",
                             QString("%1 (%2) has been added to your wallet.\n\n"
                                     "Your token balance will be updated shortly.")
                                 .arg(tokenData.name)
                                 .arg(tokenData.symbol));

    // Fetch and update token balance asynchronously
    QTimer::singleShot(500, this, &QtWalletUI::updateTokenBalances);
}

int QtWalletUI::getEthereumWalletId() {
    if (!m_walletRepository || m_currentUserId < 0) {
        return -1;
    }

    auto userWalletsResult = m_walletRepository->getWalletsByUserId(m_currentUserId);
    if (!userWalletsResult.success) {
        return -1;
    }

    for (const auto& wallet : userWalletsResult.data) {
        if (wallet.walletType == "ethereum") {
            return wallet.id;
        }
    }

    return -1;
}

void QtWalletUI::setupTokenManagement() {
    // Create token list widget for Ethereum wallet
    m_tokenListWidget = new QtTokenListWidget(m_themeManager, this);
    m_tokenListWidget->setEmptyMessage("No custom tokens imported yet.");

    // Connect token list signals
    connect(m_tokenListWidget, &QtTokenListWidget::deleteTokenClicked, this, &QtWalletUI::onTokenDeleted);

    // Add token list to Ethereum wallet card
    if (m_ethereumWalletCard) {
        m_ethereumWalletCard->setTokenListWidget(m_tokenListWidget);
    }

    // Load any previously imported tokens
    loadImportedTokens();
}

void QtWalletUI::loadImportedTokens() {
    if (!m_tokenRepository || !m_tokenListWidget) {
        return;
    }

    int ethereumWalletId = getEthereumWalletId();
    if (ethereumWalletId == -1) {
        return;
    }

    // Get tokens for wallet
    auto tokensResult = m_tokenRepository->getTokensForWallet(ethereumWalletId);
    if (!tokensResult.success) {
        return;
    }

    // Clear existing tokens first
    m_tokenListWidget->clearTokens();

    // Add each token to the list widget
    for (const auto& token : tokensResult.data) {
        TokenCardData tokenData;
        tokenData.contractAddress = QString::fromStdString(token.contract_address);
        tokenData.name = QString::fromStdString(token.name);
        tokenData.symbol = QString::fromStdString(token.symbol);
        tokenData.decimals = token.decimals;
        tokenData.balance = "Loading...";
        tokenData.balanceUSD = "";

        m_tokenListWidget->addToken(tokenData);
    }

    // Update balances after loading
    if (!tokensResult.data.empty()) {
        QTimer::singleShot(100, this, &QtWalletUI::updateTokenBalances);
    }
}

void QtWalletUI::updateTokenBalances() {
    if (!m_ethereumWallet || m_ethereumAddress.isEmpty()) {
        return;
    }

    if (!m_tokenRepository || !m_tokenListWidget) {
        return;
    }

    int ethereumWalletId = getEthereumWalletId();
    if (ethereumWalletId == -1) {
        return;
    }

    // Get tokens for wallet
    auto tokensResult = m_tokenRepository->getTokensForWallet(ethereumWalletId);
    if (!tokensResult.success) {
        return;
    }

    // Update balance for each token
    for (const auto& token : tokensResult.data) {
        QString contractAddress = QString::fromStdString(token.contract_address);

        // Fetch token balance from blockchain
        auto balanceOpt = m_ethereumWallet->GetTokenBalance(
            m_ethereumAddress.toStdString(),
            token.contract_address
        );

        if (balanceOpt.has_value()) {
            // Format the balance based on token decimals
            QString rawBalance = QString::fromStdString(balanceOpt.value());
            double balanceValue = rawBalance.toDouble();

            // Apply decimals formatting
            double formattedBalance = balanceValue / std::pow(10.0, token.decimals);
            QString balanceStr = QString::number(formattedBalance, 'f', qMin(token.decimals, 8));

            // Update the token card
            m_tokenListWidget->updateTokenBalance(contractAddress, balanceStr);
        } else {
            // If balance fetch fails, show error state
            m_tokenListWidget->updateTokenBalance(contractAddress, "Error", "");
        }
    }
}

void QtWalletUI::onTokenImported(const TokenCardData& tokenData) {
    if (m_tokenListWidget) {
        m_tokenListWidget->addToken(tokenData);
    }
    updateTokenBalances();
}

void QtWalletUI::onTokenDeleted(const QString& contractAddress) {
    if (!m_tokenRepository || !m_tokenListWidget) {
        return;
    }

    int ethereumWalletId = getEthereumWalletId();
    if (ethereumWalletId == -1) {
        QMessageBox::warning(this, "Error", "Could not find your Ethereum wallet.");
        return;
    }

    // Get token info for confirmation message
    auto tokenResult = m_tokenRepository->getToken(ethereumWalletId, contractAddress.toStdString());
    QString tokenName = tokenResult.success ? QString::fromStdString(tokenResult.data.symbol) : "this token";

    // Confirm deletion with token name
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Remove Token",
        QString("Are you sure you want to remove %1 from your wallet?\n\n"
                "This only removes the token from your view. "
                "Your actual token balance on the blockchain is not affected.")
            .arg(tokenName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    // Delete from repository
    auto deleteResult = m_tokenRepository->deleteToken(ethereumWalletId, contractAddress.toStdString());
    if (deleteResult.success) {
        m_tokenListWidget->removeToken(contractAddress);
        // Brief visual feedback without blocking message box
        if (m_statusLabel) {
            m_statusLabel->setText(QString("%1 removed from wallet").arg(tokenName));
            m_statusLabel->setVisible(true);
            QTimer::singleShot(3000, this, [this]() {
                if (m_statusLabel) {
                    m_statusLabel->setVisible(false);
                }
            });
        }
    } else {
        QMessageBox::warning(this, "Error",
                             "Failed to remove token. Please try again.");
    }
}

void QtWalletUI::applyTheme() { updateStyles(); }

QIcon QtWalletUI::createColoredIcon(const QString& svgPath, const QColor& color) {
    QSvgRenderer renderer(svgPath);

    // Check if SVG is valid
    if (!renderer.isValid()) {
        return QIcon();  // Return empty icon if SVG can't be loaded
    }

    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();

    // Colorize the icon
    QPixmap coloredPixmap(24, 24);
    coloredPixmap.fill(Qt::transparent);

    QPainter colorPainter(&coloredPixmap);
    colorPainter.setCompositionMode(QPainter::CompositionMode_Source);
    colorPainter.drawPixmap(0, 0, pixmap);
    colorPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    colorPainter.fillRect(coloredPixmap.rect(), color);
    colorPainter.end();

    return QIcon(coloredPixmap);
}

void QtWalletUI::updateStyles() {
    // Early return if theme manager or widgets haven't been created yet
    if (!m_themeManager || !m_headerSection || !m_headerTitle || !m_balanceLabel) {
        return;
    }

    try {
        setStyleSheet(m_themeManager->getMainWindowStyleSheet());
    } catch (...) {
        // Ignore any stylesheet errors during initialization
        return;
    }

    const QString text = m_themeManager->textColor().name();
    const QString accent = m_themeManager->accentColor().name();
    const QString background = m_themeManager->backgroundColor().name();
    const QString surface = m_themeManager->surfaceColor().name();
    // Note: isDarkTheme could be used for conditional styling in the future
    (void)m_themeManager->surfaceColor().lightness();  // Suppress unused warning

    // Apply proper background colors to scroll area and content
    if (m_scrollArea) {
        m_scrollArea->setStyleSheet(QString(R"(
      QScrollArea {
        background-color: %1;
        border: none;
      }
      QScrollBar:vertical {
        background: %1;
        width: 10px;
        border-radius: 5px;
        margin: 2px;
      }
      QScrollBar::handle:vertical {
        background: %2;
        border-radius: 5px;
        min-height: 20px;
      }
      QScrollBar::handle:vertical:hover {
        background: %3;
      }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
      }
      QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
        background: none;
      }
    )")
                                        .arg(background)
                                        .arg(m_themeManager->secondaryColor().name())
                                        .arg(m_themeManager->accentColor().name()));
    }

    if (m_scrollContent) {
        m_scrollContent->setStyleSheet(QString(R"(
      QWidget {
        background-color: %1;
      }
    )")
                                           .arg(background));
    }

    if (m_headerSection) {
        m_headerSection->setStyleSheet(QString(R"(
      QWidget {
        background-color: transparent;
      }
    )"));
    }

    // Calculate responsive font sizes based on window width
    int windowWidth = this->width();
    int headerTitleSize, balanceTitleSize, balanceAmountSize, toggleButtonSize;

    if (windowWidth <= 480) {
        // Very small screens - compact sizing
        headerTitleSize = 20;
        balanceTitleSize = 11;
        balanceAmountSize = 18;
        toggleButtonSize = 20;
    } else if (windowWidth <= 768) {
        // Small screens - compact sizing
        headerTitleSize = 24;
        balanceTitleSize = 12;
        balanceAmountSize = 20;
        toggleButtonSize = 22;
    } else if (windowWidth <= 1024) {
        // Tablets and small laptops - balanced sizing
        headerTitleSize = 28;
        balanceTitleSize = 13;
        balanceAmountSize = 22;
        toggleButtonSize = 24;
    } else if (windowWidth <= 1366) {
        // Common laptop screens - balanced sizing
        headerTitleSize = 32;
        balanceTitleSize = 14;
        balanceAmountSize = 24;
        toggleButtonSize = 26;
    } else if (windowWidth <= 1600) {
        // Medium laptop/desktop screens - generous sizing
        headerTitleSize = 42;
        balanceTitleSize = 18;
        balanceAmountSize = 32;
        toggleButtonSize = 26;
    } else if (windowWidth <= 1920) {
        // Full HD screens - generous sizing
        headerTitleSize = 46;
        balanceTitleSize = 19;
        balanceAmountSize = 36;
        toggleButtonSize = 28;
    } else if (windowWidth <= 2560) {
        // QHD screens - spacious sizing
        headerTitleSize = 54;
        balanceTitleSize = 22;
        balanceAmountSize = 42;
        toggleButtonSize = 30;
    } else {
        // 4K and larger - spacious sizing
        headerTitleSize = 60;
        balanceTitleSize = 24;
        balanceAmountSize = 48;
        toggleButtonSize = 32;
    }

    // Header title styling - responsive and bold
    if (m_headerTitle) {
        QString headerTitleStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: %2px;
            font-weight: 700;
            background-color: transparent;
        }
    )")
                                       .arg(text)
                                       .arg(headerTitleSize);
        m_headerTitle->setStyleSheet(headerTitleStyle);

        QFont headerFont = m_themeManager->titleFont();
        headerFont.setPointSize(headerTitleSize);
        headerFont.setBold(true);
        m_headerTitle->setFont(headerFont);
    }

    // Balance title - subtle and smaller
    if (m_balanceTitle) {
        QString balanceTitleStyle =
            QString(R"(
        QLabel {
            color: %1;
            font-size: %2px;
            font-weight: 600;
            background-color: transparent;
        }
    )")
                .arg(m_themeManager->dimmedTextColor().name())  // Theme-aware dimmed text
                .arg(balanceTitleSize);
        m_balanceTitle->setStyleSheet(balanceTitleStyle);

        QFont balanceTitleFont = m_themeManager->textFont();
        balanceTitleFont.setPointSize(balanceTitleSize);
        balanceTitleFont.setBold(true);
        m_balanceTitle->setFont(balanceTitleFont);
    }

    // Balance amount - large and prominent with accent color
    if (m_balanceLabel) {
        QString balanceColor =
            m_themeManager->accentColor().name();  // Theme provides proper contrast

        QString balanceAmountStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: %2px;
            font-weight: 700;
            background-color: transparent;
        }
    )")
                                         .arg(balanceColor)
                                         .arg(balanceAmountSize);
        m_balanceLabel->setStyleSheet(balanceAmountStyle);

        QFont balanceAmountFont = m_themeManager->titleFont();
        balanceAmountFont.setPointSize(balanceAmountSize);
        balanceAmountFont.setBold(true);
        m_balanceLabel->setFont(balanceAmountFont);
    }

    // Toggle button - simple and clean with responsive icon size
    if (m_toggleBalanceButton) {
        QString toggleButtonStyle =
            QString(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            border-radius: 16px;
            font-size: %2px;
        }
        QPushButton:hover {
            background-color: %1;
        }
    )")
                .arg(m_themeManager->secondaryColor().name())  // Theme-aware hover background
                .arg(toggleButtonSize);
        m_toggleBalanceButton->setStyleSheet(toggleButtonStyle);

        // Use emoji icons (SVG loading disabled for stability)
        m_toggleBalanceButton->setText(m_balanceVisible ? "👁" : "🚫");
    }

    // Refresh button styling - match toggle button style
    if (m_refreshButton) {
        QString refreshButtonStyle = QString(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            border-radius: 16px;
            font-size: %2px;
        }
        QPushButton:hover {
            background-color: %1;
        }
        QPushButton:disabled {
            opacity: 0.5;
        }
    )")
                                         .arg(m_themeManager->secondaryColor().name())
                                                                                   .arg(toggleButtonSize);
                                                 m_refreshButton->setStyleSheet(refreshButtonStyle);
                                             }
                                         
    if (m_importTokenButton) {
        QString importButtonStyle = QString(R"(
            QPushButton {
                background-color: %1;
                color: %2;
                border: 1px solid %3;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: 600;
            }
            QPushButton:hover {
                background-color: %4;
            }
            QPushButton:pressed {
                background-color: %5;
            }
        )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->textColor().name())
        .arg(m_themeManager->accentColor().name())
        .arg(m_themeManager->secondaryColor().name())
        .arg(m_themeManager->accentColor().name());
        m_importTokenButton->setStyleSheet(importButtonStyle);
    }
    
    // Stablecoin section header styling
    if (m_stablecoinSectionHeader) {
        QString sectionHeaderStyle = QString(R"(
            QLabel#sectionHeader {
                color: %1;
                font-size: 18px;
                font-weight: 700;
                background-color: transparent;
                padding: 16px 0 8px 0;
                margin-top: 16px;
            }
        )").arg(text);
        m_stablecoinSectionHeader->setStyleSheet(sectionHeaderStyle);
        
        QFont sectionFont = m_themeManager->titleFont();
        sectionFont.setPointSize(18);
        sectionFont.setBold(true);
        m_stablecoinSectionHeader->setFont(sectionFont);
    }
    
    // Status label styling - ensure proper contrast
    if (m_statusLabel && m_statusLabel->isVisible()) {
        updateStatusLabel();
    }
}

void QtWalletUI::onLogoutClicked() { emit logoutRequested(); }

void QtWalletUI::onToggleBalanceClicked() {
    if (!m_toggleBalanceButton) {
        return;
    }

    m_balanceVisible = !m_balanceVisible;

    // Update button text (no icons for now)
    m_toggleBalanceButton->setText(m_balanceVisible ? "👁" : "🚫");

    // Update balance display
    updateUSDBalance();
}

// PHASE 3: Manual refresh handler
void QtWalletUI::onRefreshClicked() {
    if (!m_refreshButton) {
        return;
    }

    // Refresh both balances and prices
    fetchAllPrices();

    // Refresh wallet balances if in real mode
    if (!m_mockMode && m_wallet && !m_currentAddress.isEmpty()) {
        fetchRealBalance();
    }

    // Visual feedback: briefly disable button
    m_refreshButton->setEnabled(false);
    m_refreshButton->setText("⏳");

    // Re-enable after 2 seconds
    QTimer::singleShot(2000, this, [this]() {
        if (m_refreshButton) {
            m_refreshButton->setEnabled(true);
            m_refreshButton->setText("🔄");
        }
    });
}

void QtWalletUI::resizeEvent(QResizeEvent* event) {
    // Add safety check to prevent access violation during resize
    if (!event || !this) {
        return;
    }

    QWidget::resizeEvent(event);

    // Add safety checks before calling layout functions to prevent access violations
    if (m_scrollArea && m_leftSpacer && m_rightSpacer) {
        updateScrollAreaWidth();
    }

    if (m_mainLayout && m_contentLayout) {
        updateResponsiveLayout();
    }

    // Call updateStyles on resize to apply responsive font sizes
    updateStyles();

    // These functions should also have their own safety checks
    adjustButtonLayout();
    updateCardSizes();
}

// PHASE 3: Keyboard shortcuts and accessibility
void QtWalletUI::keyPressEvent(QKeyEvent* event) {
    // F5 or Ctrl+R: Refresh balances and prices
    if (event->key() == Qt::Key_F5 ||
        (event->key() == Qt::Key_R && event->modifiers() & Qt::ControlModifier)) {
        onRefreshClicked();
        event->accept();
        return;
    }

    // Ctrl+H: Toggle balance visibility
    if (event->key() == Qt::Key_H && event->modifiers() & Qt::ControlModifier) {
        onToggleBalanceClicked();
        event->accept();
        return;
    }

    // Ctrl+S: Send Bitcoin (focus on Bitcoin wallet card send button)
    if (event->key() == Qt::Key_S && event->modifiers() & Qt::ControlModifier) {
        onSendBitcoinClicked();
        event->accept();
        return;
    }

    // Ctrl+C: Copy Bitcoin address (when not in a text field)
    if (event->key() == Qt::Key_C && event->modifiers() & Qt::ControlModifier) {
        if (!qApp->focusWidget() || !qobject_cast<QLineEdit*>(qApp->focusWidget())) {
            if (!m_currentAddress.isEmpty()) {
                QApplication::clipboard()->setText(m_currentAddress);
                // Show brief notification
                if (m_statusLabel) {
                    QString oldText = m_statusLabel->text();
                    bool wasVisible = m_statusLabel->isVisible();
                    m_statusLabel->setText("✅ Bitcoin address copied to clipboard");
                    m_statusLabel->setVisible(true);
                    QTimer::singleShot(2000, this, [this, oldText, wasVisible]() {
                        if (m_statusLabel) {
                            m_statusLabel->setText(oldText);
                            m_statusLabel->setVisible(wasVisible);
                        }
                    });
                }
            }
            event->accept();
            return;
        }
    }

    // Pass to base class
    QWidget::keyPressEvent(event);
}

void QtWalletUI::updateScrollAreaWidth() {
    // Enhanced null checking to prevent access violations
    if (!m_scrollArea || !m_leftSpacer || !m_rightSpacer) {
        return;
    }

    // Additional safety check for this pointer
    if (!this) {
        return;
    }

    int windowWidth = this->width();
    int windowHeight = this->height();

    // Validate dimensions to prevent division by zero or invalid calculations
    if (windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    double aspectRatio = static_cast<double>(windowWidth) / windowHeight;

    // Sidebar collapsed width - this is always present as an overlay
    const int SIDEBAR_WIDTH = 70;

    // Reset constraints first with safety check
    if (m_scrollArea) {
        m_scrollArea->setMinimumWidth(0);
        m_scrollArea->setMaximumWidth(QWIDGETSIZE_MAX);
    }

    // Optimized responsive width calculation for multiple screen sizes
    // Laptop screens typically have aspect ratios between 1.5 and 1.8
    bool isLaptopScreen = (aspectRatio >= 1.5 && aspectRatio <= 1.8);
    bool useFullWidth = false;

    if (windowWidth <= 768) {
        // Mobile/Small tablets (portrait) - Full width with optimized constraints
        useFullWidth = true;
        m_scrollArea->setMinimumWidth(qMax(320, windowWidth - SIDEBAR_WIDTH - 20));
    } else if (windowWidth <= 1024) {
        // Tablets (landscape) / Small laptops - Full width with optimized constraints
        useFullWidth = true;
        m_scrollArea->setMinimumWidth(qMax(480, windowWidth - SIDEBAR_WIDTH - 30));
    } else if (windowWidth <= 1366) {
        // Common laptop size (1366x768) - Full width with optimized constraints
        useFullWidth = true;
        m_scrollArea->setMinimumWidth(qMax(600, windowWidth - SIDEBAR_WIDTH - 40));
    } else if (windowWidth <= 1600 && isLaptopScreen) {
        // Medium laptops (1440p, 1536p, 1600p) - Balanced width
        int availableWidth = windowWidth - SIDEBAR_WIDTH;
        int targetWidth =
            static_cast<int>(availableWidth * 0.85);  // Increased from 0.85 for better utilization
        m_scrollArea->setMaximumWidth(targetWidth);
        m_scrollArea->setMinimumWidth(qMax(700, targetWidth));
        useFullWidth = false;
    } else if (windowWidth <= 1920 && isLaptopScreen) {
        // Full HD laptops - Balanced width for optimal use of space
        int availableWidth = windowWidth - SIDEBAR_WIDTH;
        int targetWidth = static_cast<int>(availableWidth * 0.80);  // Increased from 0.80
        m_scrollArea->setMaximumWidth(targetWidth);
        m_scrollArea->setMinimumWidth(qMax(800, targetWidth));
        useFullWidth = false;
    } else if (windowWidth <= 1920) {
        // Desktop monitors (16:9, 16:10) - Constrained for readability but more generous
        int availableWidth = windowWidth - SIDEBAR_WIDTH;
        int targetWidth = static_cast<int>(availableWidth * 0.75);  // Increased from 0.70
        m_scrollArea->setMaximumWidth(targetWidth);
        m_scrollArea->setMinimumWidth(qMax(850, targetWidth));
        useFullWidth = false;
    } else if (windowWidth <= 2560) {
        // Large desktop monitors (1440p, QHD) - More generous constraint
        int availableWidth = windowWidth - SIDEBAR_WIDTH;
        int targetWidth = static_cast<int>(availableWidth * 0.70);  // Increased from 0.65
        m_scrollArea->setMaximumWidth(targetWidth);
        m_scrollArea->setMinimumWidth(qMax(1000, targetWidth));
        useFullWidth = false;
    } else if (aspectRatio > 2.2) {
        // Ultra-wide monitors (21:9, 32:9) - More generous constraint
        int availableWidth = windowWidth - SIDEBAR_WIDTH;
        int targetWidth = static_cast<int>(availableWidth * 0.65);  // Increased from 0.55
        m_scrollArea->setMaximumWidth(targetWidth);
        m_scrollArea->setMinimumWidth(qMax(1200, targetWidth));
        useFullWidth = false;
    } else {
        // 4K and larger desktop monitors - More generous constraint
        int availableWidth = windowWidth - SIDEBAR_WIDTH;
        int targetWidth = static_cast<int>(availableWidth * 0.68);  // Increased from 0.60
        m_scrollArea->setMaximumWidth(targetWidth);
        m_scrollArea->setMinimumWidth(qMax(1200, targetWidth));
        useFullWidth = false;
    }

    // Control spacers based on whether we want full width or centered
    if (useFullWidth) {
        // Disable spacers for full width - set to fixed size 0
        m_leftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        m_rightSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    } else {
        // Enable spacers for centered content - set to expanding
        m_leftSpacer->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        m_rightSpacer->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

    // Invalidate the layout to apply spacer changes
    if (m_centeringLayout) {
        m_centeringLayout->invalidate();
    }
}

void QtWalletUI::initializeMockUsers() {
    m_mockUsers.clear();

    MockUserData alice;
    alice.username = "alice";
    alice.password = "password123";
    alice.primaryAddress = "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa";
    alice.addresses << alice.primaryAddress << "1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2";
    alice.balance = 0.15234567;

    MockTransaction tx1;
    tx1.type = "RECEIVED";
    tx1.address = "1HB5XMLmzFVj8ALj6mfBsbifRoD4miY36v";
    tx1.amount = 0.05;
    tx1.timestamp = "2024-01-15 10:30:00";
    tx1.status = "Confirmed";
    tx1.txId = "abc123def456";
    alice.transactions << tx1;

    m_mockUsers["alice"] = alice;

    MockUserData bob;
    bob.username = "bob";
    bob.password = "securepass";
    bob.primaryAddress = "1BoBMSEYstWetqTFn5Au4m4GFg7xJaNVN3";
    bob.addresses << bob.primaryAddress;
    bob.balance = 0.28456789;
    m_mockUsers["bob"] = bob;
}

bool QtWalletUI::authenticateMockUser(const QString& username, const QString& password) {
    if (m_mockUsers.contains(username)) {
        const MockUserData& user = m_mockUsers[username];
        if (user.password == password) {
            setMockUser(username);
            return true;
        }
    }
    return false;
}

void QtWalletUI::setMockUser(const QString& username) {
    if (m_mockUsers.contains(username)) {
        m_currentMockUser = &m_mockUsers[username];
        m_mockMode = true;
        setUserInfo(username, m_currentMockUser->primaryAddress);

        // Use real-time BTC price
        updateUSDBalance();

        // Update Bitcoin wallet card balance
        if (m_bitcoinWalletCard) {
            m_bitcoinWalletCard->setBalance(
                QString("%1 BTC").arg(m_currentMockUser->balance, 0, 'f', 8));
        }

        updateMockTransactionHistory();
    }
}

void QtWalletUI::updateMockTransactionHistory() {
    if (!m_bitcoinWalletCard)
        return;

    if (!m_currentMockUser || m_currentMockUser->transactions.isEmpty()) {
        m_bitcoinWalletCard->setTransactionHistory(
            "No transactions yet.<br><br>This is a demo wallet.");
        return;
    }

    QString historyHtml;
    for (const MockTransaction& tx : m_currentMockUser->transactions) {
        historyHtml +=
            QString("<b>%1:</b> %2 BTC %3 %4<br>Time: %5<br>Status: %6<br>Tx ID: %7<br><br>")
                .arg(tx.type)
                .arg(QString::number(tx.amount, 'f', 8))
                .arg(tx.type == "SENT" ? "to" : "from")
                .arg(tx.address.left(20) + "...")
                .arg(tx.timestamp)
                .arg(tx.status)
                .arg(tx.txId);
    }

    m_bitcoinWalletCard->setTransactionHistory(historyHtml);
}

void QtWalletUI::updateResponsiveLayout() {
    // Add comprehensive safety checks to prevent access violations
    if (!m_mainLayout || !m_contentLayout || !this) {
        qDebug() << "updateResponsiveLayout: Invalid layout pointers detected";
        return;
    }

    int windowWidth = this->width();
    int windowHeight = this->height();

    // Safety check for valid dimensions
    if (windowWidth <= 0 || windowHeight <= 0) {
        qDebug() << "updateResponsiveLayout: Invalid window dimensions";
        return;
    }

    double aspectRatio = static_cast<double>(windowWidth) / windowHeight;

    // Determine if this is a laptop screen
    bool isLaptopScreen = (aspectRatio >= 1.5 && aspectRatio <= 1.8);

    // Sidebar collapsed width that we need to account for
    const int SIDEBAR_WIDTH = 70;
    // Floating Sign Out button space - reduced from 100 to 60 for better space utilization
    const int SIGNOUT_BUTTON_SPACE = 60;

    // Responsive margins and spacing based on screen size - optimized for better visual balance
    int topMargin, rightMargin, bottomMargin, leftMargin;
    int contentMargin, spacing;

    if (windowWidth <= 480) {
        // Very small screens (phones portrait) - compact layout
        topMargin = 20;  // Reduced from 40 - floating button only needs minimal space
        rightMargin = SIGNOUT_BUTTON_SPACE + 5;
        bottomMargin = 5;
        leftMargin = SIDEBAR_WIDTH + 5;  // Account for sidebar + margin
        contentMargin = 8;
        spacing = 10;  // Reduced from 12 for tighter spacing
    } else if (windowWidth <= 768) {
        // Small screens (phones landscape, small tablets) - compact layout
        topMargin = 25;  // Reduced from 45
        rightMargin = SIGNOUT_BUTTON_SPACE + 8;
        bottomMargin = 8;
        leftMargin = SIDEBAR_WIDTH + 8;
        contentMargin = 12;
        spacing = 12;  // Reduced from 15
    } else if (windowWidth <= 1024) {
        // Tablets and small laptops - balanced layout
        topMargin = 30;  // Reduced from 50
        rightMargin = SIGNOUT_BUTTON_SPACE + 12;
        bottomMargin = 12;
        leftMargin = SIDEBAR_WIDTH + 12;
        contentMargin = 16;
        spacing = 16;  // Reduced from 18
    } else if (windowWidth <= 1366 && isLaptopScreen) {
        // Common laptop screens (1366x768) - balanced layout
        topMargin = 35;  // Reduced from 55
        rightMargin = SIGNOUT_BUTTON_SPACE + 15;
        bottomMargin = 15;
        leftMargin = SIDEBAR_WIDTH + 15;
        contentMargin = 18;  // Reduced from 20
        spacing = 18;        // Reduced from 20
    } else if (windowWidth <= 1600 && isLaptopScreen) {
        // Medium laptop screens (1440p, 1536p, 1600p) - spacious layout
        topMargin = 40;  // Reduced from 60
        rightMargin = SIGNOUT_BUTTON_SPACE + 18;
        bottomMargin = 18;
        leftMargin = SIDEBAR_WIDTH + 18;
        contentMargin = 20;  // Reduced from 22
        spacing = 20;        // Reduced from 22
    } else if (windowWidth <= 1920 && isLaptopScreen) {
        // Full HD laptops - spacious layout
        topMargin = 45;  // Reduced from 65
        rightMargin = SIGNOUT_BUTTON_SPACE + 20;
        bottomMargin = 20;
        leftMargin = SIDEBAR_WIDTH + 20;
        contentMargin = 22;  // Reduced from 24
        spacing = 22;        // Reduced from 24
    } else if (windowWidth <= 1920) {
        // Desktop monitors up to Full HD - generous layout
        topMargin = 50;  // Reduced from 70
        rightMargin = SIGNOUT_BUTTON_SPACE + 25;
        bottomMargin = 25;
        leftMargin = SIDEBAR_WIDTH + 25;
        contentMargin = 24;  // Reduced from 28
        spacing = 22;        // Reduced from 24
    } else if (windowWidth <= 2560) {
        // Large desktop monitors (1440p, QHD) - generous layout
        topMargin = 55;  // Reduced from 75
        rightMargin = SIGNOUT_BUTTON_SPACE + 30;
        bottomMargin = 30;
        leftMargin = SIDEBAR_WIDTH + 30;
        contentMargin = 28;  // Reduced from 32
        spacing = 24;        // Reduced from 26
    } else {
        // 4K and ultra-wide monitors - luxurious layout
        topMargin = 60;  // Reduced from 80
        rightMargin = SIGNOUT_BUTTON_SPACE + 35;
        bottomMargin = 35;
        leftMargin = SIDEBAR_WIDTH + 35;
        contentMargin = 32;  // Reduced from 36
        spacing = 26;        // Reduced from 28
    }

    // Apply calculated margins with optimized spacing
    m_mainLayout->setContentsMargins(leftMargin, topMargin, rightMargin, bottomMargin);
    m_contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
    m_mainLayout->setSpacing(spacing);
    m_contentLayout->setSpacing(spacing);

    // Adjust header and card sizing for better proportions - more refined padding with safety
    // checks
    if (m_headerSection) {
        int headerVerticalPadding;
        if (windowWidth <= 768) {
            headerVerticalPadding = 12;
        } else if (windowWidth <= 1366) {
            headerVerticalPadding = 16;
        } else if (windowWidth <= 1920) {
            headerVerticalPadding = 24;
        } else {
            headerVerticalPadding = 30;
        }

        // Add additional safety check to prevent access violation
        if (headerVerticalPadding >= 0 && headerVerticalPadding <= 100) {
            m_headerSection->setContentsMargins(0, headerVerticalPadding, 0, headerVerticalPadding);
        } else {
            qDebug() << "updateResponsiveLayout: Invalid header padding value"
                     << headerVerticalPadding;
        }
    }
}

void QtWalletUI::adjustButtonLayout() {
    // Responsive layout handled by the reusable wallet card component
}

void QtWalletUI::updateCardSizes() {
    // Card sizing handled by the reusable wallet card component
}

void QtWalletUI::fetchBTCPrice() {
    if (!m_priceFetcher) {
        return;
    }

    // Fetch price in background (this may take a moment)
    auto priceOpt = m_priceFetcher->GetBTCPrice();
    if (priceOpt.has_value()) {
        m_currentBTCPrice = priceOpt.value();
        updateUSDBalance();
    } else {
        // If fetch fails, keep using the last known price
        // or set a default fallback price
        if (m_currentBTCPrice == 0.0) {
            m_currentBTCPrice = 43000.0;  // Fallback price
        }
    }
}

void QtWalletUI::fetchLTCPrice() {
    if (!m_priceFetcher) {
        return;
    }

    // Fetch LTC price using the generic GetCryptoPrice method
    auto priceDataOpt = m_priceFetcher->GetCryptoPrice("litecoin");
    if (priceDataOpt.has_value()) {
        m_currentLTCPrice = priceDataOpt.value().usd_price;
        updateUSDBalance();
    } else {
        // If fetch fails, keep using the last known price
        // or set a default fallback price
        if (m_currentLTCPrice == 0.0) {
            m_currentLTCPrice = 70.0;  // Fallback price
        }
    }
}

// PHASE 2: Fetch Ethereum price
void QtWalletUI::fetchETHPrice() {
    if (!m_priceFetcher) {
        return;
    }

    // Fetch ETH price using the generic GetCryptoPrice method
    auto priceDataOpt = m_priceFetcher->GetCryptoPrice("ethereum");
    if (priceDataOpt.has_value()) {
        m_currentETHPrice = priceDataOpt.value().usd_price;
        updateUSDBalance();
    } else {
        // If fetch fails, keep using the last known price
        // or set a default fallback price
        if (m_currentETHPrice == 0.0) {
            m_currentETHPrice = 2500.0;  // Fallback price
        }
    }
}

// PHASE 2: Fetch all cryptocurrency prices
void QtWalletUI::fetchAllPrices() {
    fetchBTCPrice();
    fetchLTCPrice();
    fetchETHPrice();
}

void QtWalletUI::updateUSDBalance() {
    // Early return if balance label doesn't exist yet
    if (!m_balanceLabel) {
        return;
    }

    // Always respect the visibility toggle, regardless of user state
    if (!m_balanceVisible) {
        m_balanceLabel->setText("••••••");
        return;
    }

    // Calculate total balance including BTC, LTC, and ETH

    // Use fallback prices if current prices are not available
    double btcPriceToUse = (m_currentBTCPrice > 0.0) ? m_currentBTCPrice : 43000.0;
    double ltcPriceToUse = (m_currentLTCPrice > 0.0) ? m_currentLTCPrice : 70.0;
    double ethPriceToUse = (m_currentETHPrice > 0.0) ? m_currentETHPrice : 2500.0;

    double btcBalance = 0.0;
    double ltcBalance = 0.0;
    double ethBalance = 0.0;

    // Use real balance if not in mock mode
    if (!m_mockMode) {
        btcBalance = m_realBalanceBTC;
        ltcBalance = m_realBalanceLTC;
        ethBalance = m_realBalanceETH;
    } else if (m_currentMockUser) {
        // Use mock balance if in mock mode (only BTC for now)
        btcBalance = m_currentMockUser->balance;
        ltcBalance = 0.0;  // Mock mode doesn't have LTC yet
        ethBalance = 0.0;  // Mock mode doesn't have ETH yet
    }

    // Calculate USD value for each currency
    double btcUsdValue = btcBalance * btcPriceToUse;
    double ltcUsdValue = ltcBalance * ltcPriceToUse;
    double ethUsdValue = ethBalance * ethPriceToUse;

    // Calculate total USD balance
    double totalUsdBalance = btcUsdValue + ltcUsdValue + ethUsdValue;

    m_balanceLabel->setText(QString("$%L1 USD").arg(totalUsdBalance, 0, 'f', 2));
}

void QtWalletUI::onPriceUpdateTimer() {
    // PHASE 2: This runs every 60 seconds to refresh both BTC and ETH prices
    fetchAllPrices();
}

// === Real Wallet Integration Methods ===

void QtWalletUI::setWallet(WalletAPI::SimpleWallet* wallet) {
    m_wallet = wallet;
    // Fetch initial balance when wallet is set
    if (m_wallet && !m_currentAddress.isEmpty()) {
        fetchRealBalance();
    }
}

void QtWalletUI::setLitecoinWallet(WalletAPI::LitecoinWallet* ltcWallet) {
    m_litecoinWallet = ltcWallet;
    // Fetch initial balance when wallet is set
    if (m_litecoinWallet && !m_litecoinAddress.isEmpty()) {
        fetchRealBalance();
    }
}

void QtWalletUI::setLitecoinAddress(const QString& address) {
    m_litecoinAddress = address;
    // Fetch balance if wallet is already set
    if (m_litecoinWallet && !m_litecoinAddress.isEmpty()) {
        fetchRealBalance();
    }
}

// PHASE 1 FIX: Ethereum wallet initialization
void QtWalletUI::setEthereumWallet(WalletAPI::EthereumWallet* ethWallet) {
    m_ethereumWallet = ethWallet;
    // Fetch initial balance when wallet is set
    if (m_ethereumWallet && !m_ethereumAddress.isEmpty()) {
        fetchRealBalance();
    }
}

void QtWalletUI::setEthereumAddress(const QString& address) {
    m_ethereumAddress = address;
    // Fetch balance if wallet is already set
    if (m_ethereumWallet && !m_ethereumAddress.isEmpty()) {
        fetchRealBalance();
    }
}

void QtWalletUI::fetchRealBalance() {
    if (!m_wallet || m_currentAddress.isEmpty()) {
        return;
    }

    // Disable mock mode when fetching real balance
    m_mockMode = false;

    // PHASE 2: Set loading state for Bitcoin
    setLoadingState(true, "Bitcoin");

    try {
        // Fetch Bitcoin balance from blockchain
        std::string address = m_currentAddress.toStdString();
        uint64_t balanceSatoshis = m_wallet->GetBalance(address);

        // Convert satoshis to BTC
        m_realBalanceBTC = m_wallet->ConvertSatoshisToBTC(balanceSatoshis);

        // Update UI
        updateUSDBalance();

        // Update Bitcoin wallet card
        if (m_bitcoinWalletCard) {
            m_bitcoinWalletCard->setBalance(QString("%1 BTC").arg(m_realBalanceBTC, 0, 'f', 8));
        }

        // PHASE 3: Fetch and display Bitcoin transaction history with improved formatting
        auto txHistory = m_wallet->GetTransactionHistory(address, 10);
        if (m_bitcoinWalletCard) {
            m_bitcoinWalletCard->setTransactionHistory(formatBitcoinTransactionHistory(txHistory));
        }

        // PHASE 2: Clear loading state for Bitcoin
        setLoadingState(false, "Bitcoin");

    } catch (const std::exception& e) {
        setErrorState(QString("Failed to fetch Bitcoin balance: %1").arg(e.what()));
        return;
    }

    // Fetch Litecoin balance if we have a Litecoin wallet and address
    if (m_litecoinWallet && !m_litecoinAddress.isEmpty()) {
        setLoadingState(true, "Litecoin");

        try {
            std::string ltcAddress = m_litecoinAddress.toStdString();
            uint64_t balanceLitoshis = m_litecoinWallet->GetBalance(ltcAddress);

            // Convert litoshis to LTC
            m_realBalanceLTC = m_litecoinWallet->ConvertLitoshisToLTC(balanceLitoshis);

            // Update Litecoin wallet card
            if (m_litecoinWalletCard) {
                m_litecoinWalletCard->setBalance(QString("%1 LTC").arg(m_realBalanceLTC, 0, 'f', 8));
            }

            // Fetch and display Litecoin transaction history
            auto ltcTxHistory = m_litecoinWallet->GetTransactionHistory(ltcAddress, 10);
            if (m_litecoinWalletCard) {
                m_litecoinWalletCard->setTransactionHistory(formatBitcoinTransactionHistory(ltcTxHistory));
            }

            setLoadingState(false, "Litecoin");

        } catch (const std::exception& e) {
            setErrorState(QString("Failed to fetch Litecoin balance: %1").arg(e.what()));
        }
    }

    // Fetch Ethereum balance if we have an Ethereum wallet and address
    if (m_ethereumWallet && !m_ethereumAddress.isEmpty()) {
        // PHASE 2: Set loading state for Ethereum
        setLoadingState(true, "Ethereum");

        try {
            std::string ethAddress = m_ethereumAddress.toStdString();
            m_realBalanceETH = m_ethereumWallet->GetBalance(ethAddress);

            // Update Ethereum wallet card
            if (m_ethereumWalletCard) {
                m_ethereumWalletCard->setBalance(
                    QString("%1 ETH").arg(m_realBalanceETH, 0, 'f', 8));
            }

            // PHASE 3: Fetch and display Ethereum transaction history with improved formatting
            auto ethTxHistory = m_ethereumWallet->GetTransactionHistory(ethAddress, 10);
            if (m_ethereumWalletCard) {
                m_ethereumWalletCard->setTransactionHistory(
                    formatEthereumTransactionHistory(ethTxHistory, ethAddress));
            }

            // PHASE 2: Clear loading state for Ethereum
            setLoadingState(false, "Ethereum");

        } catch (const std::exception& e) {
            setErrorState(QString("Failed to fetch Ethereum balance: %1").arg(e.what()));
        }
    }
}

void QtWalletUI::onBalanceUpdateTimer() {
    // Periodically refresh balance if we're in real wallet mode
    if (!m_mockMode && m_wallet && !m_currentAddress.isEmpty()) {
        fetchRealBalance();
    }
}

void QtWalletUI::setRepositories(Repository::UserRepository* userRepo,
                                 Repository::WalletRepository* walletRepo) {
    m_userRepository = userRepo;
    m_walletRepository = walletRepo;
}

void QtWalletUI::setTokenRepository(Repository::TokenRepository* tokenRepo) {
    m_tokenRepository = tokenRepo;

    // Setup token management after repository is set
    if (m_tokenRepository && m_ethereumWallet && m_ethereumWalletCard) {
        setupTokenManagement();
    }
}

void QtWalletUI::setCurrentUserId(int userId) { m_currentUserId = userId; }

void QtWalletUI::sendRealTransaction(const QString& recipientAddress, uint64_t amountSatoshis,
                                     uint64_t feeSatoshis, const QString& password) {
    if (!m_wallet || !m_walletRepository || m_currentUserId < 0) {
        throw std::runtime_error("Wallet or repositories not properly initialized");
    }

    // Step 1: Retrieve and decrypt the user's seed phrase
    auto seedResult =
        m_walletRepository->retrieveDecryptedSeed(m_currentUserId, password.toStdString());
    if (!seedResult.success) {
        throw std::runtime_error("Failed to decrypt seed: " + seedResult.errorMessage);
    }

    std::vector<std::string> mnemonic = seedResult.data;

    // Step 2: Derive master key from seed
    std::array<uint8_t, 64> seed;
    if (!Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed)) {
        throw std::runtime_error("Failed to derive seed from mnemonic");
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        throw std::runtime_error("Failed to derive master key");
    }

    // Step 3: Derive the private key for the current address (testnet BIP44 path: m/44'/1'/0'/0/0)
    Crypto::BIP32ExtendedKey addressKey;
    if (!Crypto::BIP44_DeriveAddressKey(masterKey, 0, false, 0, addressKey, true)) {
        throw std::runtime_error("Failed to derive address key");
    }

    // Step 4: Get WIF private key
    std::string wifPrivateKey;
    if (!Crypto::BIP32_GetWIF(addressKey, wifPrivateKey, true)) {
        throw std::runtime_error("Failed to get WIF private key");
    }

    // Step 5: Create private keys map for WalletAPI
    std::map<std::string, std::vector<uint8_t>> privateKeysMap;
    std::string fromAddress = m_currentAddress.toStdString();

    // Convert WIF to raw private key bytes (WIF is base58 encoded, we need raw bytes)
    // For simplicity, we'll use the key directly from the extended key
    std::vector<uint8_t> privateKeyBytes(addressKey.key.begin(), addressKey.key.end());
    privateKeysMap[fromAddress] = privateKeyBytes;

    // Step 6: Prepare addresses
    std::vector<std::string> fromAddresses = {fromAddress};
    std::string toAddress = recipientAddress.toStdString();

    // Step 7: Send the transaction using WalletAPI
    WalletAPI::SendTransactionResult result =
        m_wallet->SendFunds(fromAddresses, toAddress, amountSatoshis, privateKeysMap, feeSatoshis);

    // Step 8: Handle result
    if (!result.success) {
        throw std::runtime_error("Transaction failed: " + result.error_message);
    }

    // Step 9: Show success message
    QMessageBox::information(
        this, "Transaction Sent",
        QString("Transaction broadcast successfully!\n\n"
                "Transaction Hash:\n%1\n\n"
                "Amount: %2 BTC\n"
                "Fee: %3 BTC\n\n"
                "The transaction will appear in your history once confirmed.")
            .arg(QString::fromStdString(result.transaction_hash))
            .arg(static_cast<double>(amountSatoshis) / 100000000.0, 0, 'f', 8)
            .arg(static_cast<double>(result.total_fees) / 100000000.0, 0, 'f', 8));

    // Step 10: Clean up sensitive data
    // TODO: Re-enable secure memory cleanup after fixing linkage
    // Crypto::SecureWipeVector(privateKeyBytes);
    // Crypto::SecureZeroMemory(seed.data(), seed.size());
    privateKeyBytes.clear();
}

std::vector<uint8_t> QtWalletUI::derivePrivateKeyForAddress(const QString& address,
                                                            const QString& password) {
    if (!m_walletRepository || m_currentUserId < 0) {
        throw std::runtime_error("Wallet repository not properly initialized");
    }

    // Step 1: Retrieve and decrypt the user's seed phrase
    auto seedResult =
        m_walletRepository->retrieveDecryptedSeed(m_currentUserId, password.toStdString());
    if (!seedResult.success) {
        throw std::runtime_error("Failed to decrypt seed: " + seedResult.errorMessage);
    }

    std::vector<std::string> mnemonic = seedResult.data;

    // Step 2: Derive master key from seed
    std::array<uint8_t, 64> seed;
    if (!Crypto::BIP39_SeedFromMnemonic(mnemonic, "", seed)) {
        throw std::runtime_error("Failed to derive seed from mnemonic");
    }

    Crypto::BIP32ExtendedKey masterKey;
    if (!Crypto::BIP32_MasterKeyFromSeed(seed, masterKey)) {
        throw std::runtime_error("Failed to derive master key");
    }

    // Step 3: Determine if this is Bitcoin or Ethereum address and derive accordingly
    bool isEthereum = address.startsWith("0x");

    Crypto::BIP32ExtendedKey addressKey;
    if (isEthereum) {
        // Ethereum BIP44 path: m/44'/60'/0'/0/0
        if (!Crypto::BIP44_DeriveEthereumAddressKey(masterKey, 0, false, 0, addressKey)) {
            throw std::runtime_error("Failed to derive Ethereum address key");
        }
    } else {
        // Bitcoin BIP44 path: m/44'/0'/0'/0/0 (mainnet) or m/44'/1'/0'/0/0 (testnet)
        if (!Crypto::BIP44_DeriveAddressKey(masterKey, 0, false, 0, addressKey, true)) {
            throw std::runtime_error("Failed to derive Bitcoin address key");
        }
    }

    // Step 4: Extract private key bytes
    std::vector<uint8_t> privateKeyBytes(addressKey.key.begin(), addressKey.key.end());

    return privateKeyBytes;
}

// PHASE 2: Loading and error state management

void QtWalletUI::setLoadingState(bool loading, const QString& chain) {
    if (chain.toLower() == "bitcoin" || chain.toLower() == "btc") {
        m_isLoadingBTC = loading;
    } else if (chain.toLower() == "litecoin" || chain.toLower() == "ltc") {
        m_isLoadingLTC = loading;
    } else if (chain.toLower() == "ethereum" || chain.toLower() == "eth") {
        m_isLoadingETH = loading;
    }

    clearErrorState();
    updateStatusLabel();
}

void QtWalletUI::setErrorState(const QString& errorMessage) {
    m_lastErrorMessage = errorMessage;
    m_isLoadingBTC = false;
    m_isLoadingLTC = false;
    m_isLoadingETH = false;
    updateStatusLabel();
}

void QtWalletUI::clearErrorState() {
    m_lastErrorMessage.clear();
    updateStatusLabel();
}

void QtWalletUI::updateStatusLabel() {
    if (!m_statusLabel) {
        return;
    }

    QString statusText;
    QString styleClass;

    // Check for errors first
    if (!m_lastErrorMessage.isEmpty()) {
        statusText = "⚠️ " + m_lastErrorMessage;
        styleClass = "error";
        m_statusLabel->setVisible(true);
    }
    // Check for loading states
    else if (m_isLoadingBTC && m_isLoadingLTC && m_isLoadingETH) {
        statusText = "Loading Bitcoin, Litecoin, and Ethereum balances...";
        styleClass = "loading";
        m_statusLabel->setVisible(true);
    } else if (m_isLoadingBTC && m_isLoadingLTC) {
        statusText = "Loading Bitcoin and Litecoin balances...";
        styleClass = "loading";
        m_statusLabel->setVisible(true);
    } else if (m_isLoadingBTC && m_isLoadingETH) {
        statusText = "Loading Bitcoin and Ethereum balances...";
        styleClass = "loading";
        m_statusLabel->setVisible(true);
    } else if (m_isLoadingLTC && m_isLoadingETH) {
        statusText = "Loading Litecoin and Ethereum balances...";
        styleClass = "loading";
        m_statusLabel->setVisible(true);
    } else if (m_isLoadingBTC) {
        statusText = "Loading Bitcoin balance...";
        styleClass = "loading";
        m_statusLabel->setVisible(true);
    } else if (m_isLoadingLTC) {
        statusText = "Loading Litecoin balance...";
        styleClass = "loading";
        m_statusLabel->setVisible(true);
    } else if (m_isLoadingETH) {
        statusText = "Loading Ethereum balance...";
        styleClass = "loading";
        m_statusLabel->setVisible(true);
    }
    // No status to show
    else {
        m_statusLabel->setVisible(false);
        return;
    }

    m_statusLabel->setText(statusText);

    // Apply styling based on state
    if (m_themeManager) {
        QColor textColor, bgColor;
        if (styleClass == "error") {
            textColor = m_themeManager->errorColor();
            bgColor = m_themeManager->lightError();
        } else {
            textColor = m_themeManager->textColor();
            bgColor = m_themeManager->backgroundColor();
        }

        m_statusLabel->setStyleSheet(QString("QLabel {"
                                             "  color: %1;"
                                             "  background-color: %2;"
                                             "  padding: 8px;"
                                             "  border-radius: 4px;"
                                             "  font-size: 12px;"
                                             "}")
                                         .arg(textColor.name())
                                         .arg(bgColor.name()));
    }
}

// PHASE 3: Transaction history formatting helpers

QString QtWalletUI::formatBitcoinTransactionHistory(const std::vector<std::string>& txHashes) {
    if (txHashes.empty()) {
        return "No transactions yet.<br><br>Send testnet Bitcoin to your address to see it appear "
               "here!";
    }

    QString historyHtml;
    historyHtml += "<div style='font-size: 12px;'>";
    historyHtml += QString("<b>Recent Transactions</b> (%1 total)<br><br>").arg(txHashes.size());

    int count = 0;
    for (const auto& txHash : txHashes) {
        if (count >= 5)
            break;  // Show only first 5 transactions

        QString shortHash = QString::fromStdString(txHash).left(16) + "...";
        historyHtml +=
            QString(
                "<div style='margin-bottom: 12px; padding: 8px; background: rgba(128,128,128,0.1); "
                "border-radius: 4px;'>"
                "<b>TX:</b> <span style='font-family: monospace;'>%1</span><br>"
                "<span style='font-size: 10px; color: #666;'>Tap to view on block explorer</span>"
                "</div>")
                .arg(shortHash);

        count++;
    }

    if (txHashes.size() > 5) {
        historyHtml += QString("<br><i>... and %1 more transactions</i>").arg(txHashes.size() - 5);
    }

    historyHtml += "</div>";
    return historyHtml;
}

QString QtWalletUI::formatEthereumTransactionHistory(
    const std::vector<EthereumService::Transaction>& txs, const std::string& userAddress) {
    if (txs.empty()) {
        return "No transactions yet.<br><br>Send Ethereum to your address to see it appear here!";
    }

    QString historyHtml;
    historyHtml += "<div style='font-size: 12px;'>";
    historyHtml += QString("<b>Recent Transactions</b> (%1 total)<br><br>").arg(txs.size());

    int count = 0;
    for (const auto& tx : txs) {
        if (count >= 5)
            break;  // Show only first 5 transactions

        QString type = (tx.to == userAddress) ? "Received" : "Sent";
        QString typeColor = (tx.to == userAddress)
                                ? m_themeManager->positiveColor().name()   // Green for received
                                : m_themeManager->negativeColor().name();  // Red for sent
        QString statusIcon = tx.is_error ? "❌" : "✅";
        QString shortHash = QString::fromStdString(tx.hash).left(16) + "...";

        historyHtml += QString(
                           "<div style='margin-bottom: 12px; padding: 8px; background: "
                           "rgba(128,128,128,0.1); border-radius: 4px;'>"
                           "<b style='color: %1;'>%2:</b> %3 ETH %4<br>"
                           "<span style='font-family: monospace; font-size: 10px;'>%5</span><br>"
                           "<span style='font-size: 10px; color: #666;'>Block: %6</span>"
                           "</div>")
                           .arg(typeColor)
                           .arg(type)
                           .arg(tx.value_eth, 0, 'f', 6)
                           .arg(statusIcon)
                           .arg(shortHash)
                           .arg(QString::fromStdString(tx.block_number));

        count++;
    }

    if (txs.size() > 5) {
        historyHtml += QString("<br><i>... and %1 more transactions</i>").arg(txs.size() - 5);
    }

    historyHtml += "</div>";
    return historyHtml;
}

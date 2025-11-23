// QtWalletUI.cpp
#include "QtWalletUI.h"
#include "QtExpandableWalletCard.h"
#include "QtThemeManager.h"
#include "QtSendDialog.h"
#include "QtReceiveDialog.h"
#include "PriceService.h"
#include "Crypto.h"

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
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

QtWalletUI::QtWalletUI(QWidget *parent)
    : QWidget(parent), m_themeManager(nullptr),
      m_centeringLayout(nullptr), m_leftSpacer(nullptr), m_rightSpacer(nullptr),
      m_headerSection(nullptr), m_headerTitle(nullptr),
      m_balanceTitle(nullptr), m_balanceLabel(nullptr),
      m_toggleBalanceButton(nullptr), m_refreshButton(nullptr), m_bitcoinWalletCard(nullptr),
      m_ethereumWalletCard(nullptr), m_currentMockUser(nullptr), m_wallet(nullptr),
      m_ethereumWallet(nullptr), m_balanceUpdateTimer(nullptr),
      m_realBalanceBTC(0.0), m_realBalanceETH(0.0), m_userRepository(nullptr),
      m_walletRepository(nullptr), m_currentUserId(-1), m_priceFetcher(nullptr),
      m_priceUpdateTimer(nullptr), m_currentBTCPrice(43000.0), m_currentETHPrice(2500.0),
      m_isLoadingBTC(false), m_isLoadingETH(false), m_statusLabel(nullptr),
      m_balanceVisible(true), m_mockMode(false) {

  // Get theme manager SAFELY
  m_themeManager = &QtThemeManager::instance();

  // Initialize mock users first (doesn't touch UI)
  initializeMockUsers();

  // Create all UI widgets
  setupUI();

  // Defer all complex initialization to after event loop starts
  QTimer::singleShot(100, this, [this]() {
    // Initialize price fetcher
    m_priceFetcher = std::make_unique<PriceService::PriceFetcher>();

    // Setup price update timer
    m_priceUpdateTimer = new QTimer(this);
    connect(m_priceUpdateTimer, &QTimer::timeout, this, &QtWalletUI::onPriceUpdateTimer);
    m_priceUpdateTimer->start(60000); // 60 seconds

    // Setup balance update timer (refresh every 30 seconds)
    m_balanceUpdateTimer = new QTimer(this);
    connect(m_balanceUpdateTimer, &QTimer::timeout, this, &QtWalletUI::onBalanceUpdateTimer);
    m_balanceUpdateTimer->start(30000); // 30 seconds

    // Apply theme
    if (m_themeManager) {
      applyTheme();
    }

    // PHASE 2: Fetch initial prices for both BTC and ETH
    fetchAllPrices();
  });

  // Connect theme changed signal
  if (m_themeManager) {
    connect(m_themeManager, &QtThemeManager::themeChanged, this,
            &QtWalletUI::onThemeChanged);
  }
}

void QtWalletUI::setupUI() {
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(
      m_themeManager->standardMargin(),
      m_themeManager->standardMargin(),
      m_themeManager->standardMargin(),
      m_themeManager->standardMargin()
  );
  m_mainLayout->setSpacing(m_themeManager->standardSpacing());

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
  m_contentLayout->setContentsMargins(20, 20, 20, 20);
  m_contentLayout->setSpacing(20);

  createHeaderSection();
  createActionButtons();

  m_contentLayout->addItem(
      new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

  m_scrollArea->setWidget(m_scrollContent);

  // Add scroll area to centering layout
  m_centeringLayout->addWidget(m_scrollArea);

  // Add right spacer (will be dynamically controlled for full-width on laptops)
  m_rightSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_centeringLayout->addItem(m_rightSpacer);

  m_mainLayout->addLayout(m_centeringLayout);

  // Initialize responsive layout after UI setup
  QTimer::singleShot(0, this, [this]() {
    updateResponsiveLayout();
    adjustButtonLayout();
    updateCardSizes();
  });
}

void QtWalletUI::createHeaderSection() {
  m_headerSection = new QWidget(m_scrollContent);

  QVBoxLayout *headerLayout = new QVBoxLayout(m_headerSection);
  headerLayout->setContentsMargins(0, 20, 0, 30);
  headerLayout->setSpacing(15);

  // Digital Wallets title
  m_headerTitle = new QLabel("Digital Wallets", m_headerSection);
  m_headerTitle->setAlignment(Qt::AlignCenter);
  headerLayout->addWidget(m_headerTitle);

  // Balance section
  QVBoxLayout *balanceVerticalLayout = new QVBoxLayout();
  balanceVerticalLayout->setSpacing(5);

  m_balanceTitle = new QLabel("Total Balance", m_headerSection);
  m_balanceTitle->setAlignment(Qt::AlignCenter);
  balanceVerticalLayout->addWidget(m_balanceTitle);

  // Horizontal layout for amount and toggle button
  QHBoxLayout *balanceRowLayout = new QHBoxLayout();
  balanceRowLayout->setSpacing(10);
  balanceRowLayout->setAlignment(Qt::AlignCenter);

  // Balance amount
  m_balanceLabel = new QLabel("$0.00 USD", m_headerSection);
  m_balanceLabel->setAlignment(Qt::AlignCenter);
  balanceRowLayout->addWidget(m_balanceLabel);

  // Toggle button
  m_toggleBalanceButton = new QPushButton(m_headerSection);
  m_toggleBalanceButton->setIconSize(QSize(20, 20));
  m_toggleBalanceButton->setFixedSize(32, 32);
  m_toggleBalanceButton->setToolTip("Hide/Show Balance (Ctrl+H)");  // PHASE 3: Accessibility tooltip
  m_toggleBalanceButton->setCursor(Qt::PointingHandCursor);
  m_toggleBalanceButton->setAccessibleName("Toggle Balance Visibility");  // PHASE 3: Screen reader support
  m_toggleBalanceButton->setAccessibleDescription("Press to toggle balance visibility");
  balanceRowLayout->addWidget(m_toggleBalanceButton);

  // PHASE 3: Manual refresh button
  m_refreshButton = new QPushButton(m_headerSection);
  m_refreshButton->setText("🔄");
  m_refreshButton->setFixedSize(32, 32);
  m_refreshButton->setToolTip("Refresh balances and prices (F5 or Ctrl+R)");  // PHASE 3: Accessibility tooltip
  m_refreshButton->setCursor(Qt::PointingHandCursor);
  m_refreshButton->setAccessibleName("Refresh Balances");  // PHASE 3: Screen reader support
  m_refreshButton->setAccessibleDescription("Press to refresh wallet balances and cryptocurrency prices");
  balanceRowLayout->addWidget(m_refreshButton);

  balanceVerticalLayout->addLayout(balanceRowLayout);
  headerLayout->addLayout(balanceVerticalLayout);

  // PHASE 2: Status label for loading/error messages
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
}


void QtWalletUI::setUserInfo(const QString &username, const QString &address) {
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
  double currentBalance = m_mockMode ? (m_currentMockUser ? m_currentMockUser->balance : 0.0)
                                     : m_realBalanceBTC;

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
          m_bitcoinWalletCard->setBalance(QString("%1 BTC").arg(m_currentMockUser->balance, 0, 'f', 8));
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

// PHASE 2: Separate Ethereum receive handler
void QtWalletUI::onReceiveEthereumClicked() {
  // Show Ethereum receive dialog with Ethereum address
  if (m_ethereumAddress.isEmpty()) {
    QMessageBox::warning(this, "No Ethereum Address",
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

    int response = QMessageBox::question(this, "Confirm Ethereum Transaction",
                                         confirmMsg,
                                         QMessageBox::Yes | QMessageBox::No);

    if (response == QMessageBox::Yes) {
      try {
        // Show progress indicator
        QMessageBox* progressBox = new QMessageBox(this);
        progressBox->setWindowTitle("Sending Ethereum Transaction");
        progressBox->setText("Broadcasting transaction to the Ethereum network...\nPlease wait.");
        progressBox->setStandardButtons(QMessageBox::NoButton);
        progressBox->setModal(true);
        progressBox->show();
        QApplication::processEvents();

        // Derive private key for the Ethereum address
        std::vector<uint8_t> privateKeyBytes = derivePrivateKeyForAddress(m_ethereumAddress, txData.password);

        // Convert private key to hex string
        QString privateKeyHex;
        for (uint8_t byte : privateKeyBytes) {
          privateKeyHex += QString("%1").arg(byte, 2, 16, QChar('0'));
        }

        // Send the Ethereum transaction
        auto result = m_ethereumWallet->SendFunds(
            m_ethereumAddress.toStdString(),
            txData.recipientAddress.toStdString(),
            txData.amountETH,
            privateKeyHex.toStdString(),
            txData.gasPriceGwei.toStdString(),
            txData.gasLimit
        );

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
                  .arg(QString::number(result.total_cost_eth * m_currentETHPrice, 'f', 2)));

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
  if (m_ethereumWalletCard) {
    m_ethereumWalletCard->applyTheme();
  }
}

void QtWalletUI::applyTheme() { updateStyles(); }

QIcon QtWalletUI::createColoredIcon(const QString &svgPath,
                                    const QColor &color) {
  QSvgRenderer renderer(svgPath);

  // Check if SVG is valid
  if (!renderer.isValid()) {
    return QIcon(); // Return empty icon if SVG can't be loaded
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
  bool isDarkTheme = m_themeManager->surfaceColor().lightness() < 128;

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
    )").arg(background)
       .arg(m_themeManager->secondaryColor().name())
       .arg(m_themeManager->accentColor().name()));
  }

  if (m_scrollContent) {
    m_scrollContent->setStyleSheet(QString(R"(
      QWidget {
        background-color: %1;
      }
    )").arg(background));
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
    // Very small screens
    headerTitleSize = 24;
    balanceTitleSize = 12;
    balanceAmountSize = 20;
    toggleButtonSize = 22;
  } else if (windowWidth <= 768) {
    // Small screens
    headerTitleSize = 28;
    balanceTitleSize = 13;
    balanceAmountSize = 22;
    toggleButtonSize = 24;
  } else if (windowWidth <= 1024) {
    // Tablets and small laptops
    headerTitleSize = 32;
    balanceTitleSize = 14;
    balanceAmountSize = 24;
    toggleButtonSize = 26;
  } else if (windowWidth <= 1366) {
    // Common laptop screens
    headerTitleSize = 36;
    balanceTitleSize = 15;
    balanceAmountSize = 26;
    toggleButtonSize = 28;
  } else if (windowWidth <= 1600) {
    // Medium laptop/desktop screens
    headerTitleSize = 38;
    balanceTitleSize = 16;
    balanceAmountSize = 28;
    toggleButtonSize = 28;
  } else if (windowWidth <= 1920) {
    // Full HD screens
    headerTitleSize = 40;
    balanceTitleSize = 16;
    balanceAmountSize = 30;
    toggleButtonSize = 30;
  } else if (windowWidth <= 2560) {
    // QHD screens
    headerTitleSize = 44;
    balanceTitleSize = 18;
    balanceAmountSize = 34;
    toggleButtonSize = 32;
  } else {
    // 4K and larger
    headerTitleSize = 48;
    balanceTitleSize = 20;
    balanceAmountSize = 38;
    toggleButtonSize = 34;
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
    )").arg(text).arg(headerTitleSize);
    m_headerTitle->setStyleSheet(headerTitleStyle);

    QFont headerFont = m_themeManager->titleFont();
    headerFont.setPointSize(headerTitleSize);
    headerFont.setBold(true);
    m_headerTitle->setFont(headerFont);
  }

  // Balance title - subtle and smaller
  if (m_balanceTitle) {
    QString balanceTitleStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: %2px;
            font-weight: 600;
            background-color: transparent;
        }
    )").arg(m_themeManager->dimmedTextColor().name())  // Theme-aware dimmed text
       .arg(balanceTitleSize);
    m_balanceTitle->setStyleSheet(balanceTitleStyle);

    QFont balanceTitleFont = m_themeManager->textFont();
    balanceTitleFont.setPointSize(balanceTitleSize);
    balanceTitleFont.setBold(true);
    m_balanceTitle->setFont(balanceTitleFont);
  }

  // Balance amount - large and prominent with accent color
  if (m_balanceLabel) {
    QString balanceColor = m_themeManager->accentColor().name();  // Theme provides proper contrast

    QString balanceAmountStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: %2px;
            font-weight: 700;
            background-color: transparent;
        }
    )").arg(balanceColor).arg(balanceAmountSize);
    m_balanceLabel->setStyleSheet(balanceAmountStyle);

    QFont balanceAmountFont = m_themeManager->titleFont();
    balanceAmountFont.setPointSize(balanceAmountSize);
    balanceAmountFont.setBold(true);
    m_balanceLabel->setFont(balanceAmountFont);
  }

  // Toggle button - simple and clean with responsive icon size
  if (m_toggleBalanceButton) {
    QString toggleButtonStyle = QString(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            border-radius: 16px;
            font-size: %2px;
        }
        QPushButton:hover {
            background-color: %1;
        }
    )").arg(m_themeManager->secondaryColor().name())  // Theme-aware hover background
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
    )").arg(m_themeManager->secondaryColor().name())
       .arg(toggleButtonSize);
    m_refreshButton->setStyleSheet(refreshButtonStyle);
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

void QtWalletUI::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateScrollAreaWidth();
  updateResponsiveLayout();
  adjustButtonLayout();
  updateCardSizes();
}

// PHASE 3: Keyboard shortcuts and accessibility
void QtWalletUI::keyPressEvent(QKeyEvent *event) {
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
  if (!m_scrollArea || !m_leftSpacer || !m_rightSpacer) {
    return;
  }

  int windowWidth = this->width();
  int windowHeight = this->height();
  double aspectRatio = static_cast<double>(windowWidth) / windowHeight;

  // Sidebar collapsed width - this is always present as an overlay
  const int SIDEBAR_WIDTH = 70;

  // Reset constraints first
  m_scrollArea->setMinimumWidth(0);
  m_scrollArea->setMaximumWidth(QWIDGETSIZE_MAX);

  // Optimized responsive width calculation for multiple screen sizes
  // Laptop screens typically have aspect ratios between 1.5 and 1.8
  bool isLaptopScreen = (aspectRatio >= 1.5 && aspectRatio <= 1.8);
  bool useFullWidth = false;

  if (windowWidth <= 768) {
    // Mobile/Small tablets (portrait) - Full width
    useFullWidth = true;
  } else if (windowWidth <= 1024) {
    // Tablets (landscape) / Small laptops - Full width
    useFullWidth = true;
  } else if (windowWidth <= 1366) {
    // Common laptop size (1366x768) - Full width
    useFullWidth = true;
  } else if (windowWidth <= 1600 && isLaptopScreen) {
    // Medium laptops (1440p, 1536p, 1600p) - Full width
    useFullWidth = true;
  } else if (windowWidth <= 1920 && isLaptopScreen) {
    // Full HD laptops - Full width for optimal use of space
    useFullWidth = true;
  } else if (windowWidth <= 1920) {
    // Desktop monitors (16:9, 16:10) - Constrained for readability
    // Subtract sidebar width from available width
    int availableWidth = windowWidth - SIDEBAR_WIDTH;
    int targetWidth = static_cast<int>(availableWidth * 0.70);
    m_scrollArea->setMaximumWidth(targetWidth);
    m_scrollArea->setMinimumWidth(900);
    useFullWidth = false;
  } else if (windowWidth <= 2560) {
    // Large desktop monitors (1440p, QHD) - More constraint
    int availableWidth = windowWidth - SIDEBAR_WIDTH;
    int targetWidth = static_cast<int>(availableWidth * 0.65);
    m_scrollArea->setMaximumWidth(targetWidth);
    m_scrollArea->setMinimumWidth(1000);
    useFullWidth = false;
  } else if (aspectRatio > 2.2) {
    // Ultra-wide monitors (21:9, 32:9) - Significant constraint
    int availableWidth = windowWidth - SIDEBAR_WIDTH;
    int targetWidth = static_cast<int>(availableWidth * 0.55);
    m_scrollArea->setMaximumWidth(targetWidth);
    m_scrollArea->setMinimumWidth(1200);
    useFullWidth = false;
  } else {
    // 4K and larger desktop monitors - Moderate constraint
    int availableWidth = windowWidth - SIDEBAR_WIDTH;
    int targetWidth = static_cast<int>(availableWidth * 0.60);
    m_scrollArea->setMaximumWidth(targetWidth);
    m_scrollArea->setMinimumWidth(1200);
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
  alice.addresses << alice.primaryAddress
                  << "1BvBMSEYstWetqTFn5Au4m4GFg7xJaNVN2";
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

bool QtWalletUI::authenticateMockUser(const QString &username,
                                      const QString &password) {
  if (m_mockUsers.contains(username)) {
    const MockUserData &user = m_mockUsers[username];
    if (user.password == password) {
      setMockUser(username);
      return true;
    }
  }
  return false;
}

void QtWalletUI::setMockUser(const QString &username) {
  if (m_mockUsers.contains(username)) {
    m_currentMockUser = &m_mockUsers[username];
    m_mockMode = true;
    setUserInfo(username, m_currentMockUser->primaryAddress);

    // Use real-time BTC price
    updateUSDBalance();

    // Update Bitcoin wallet card balance
    if (m_bitcoinWalletCard) {
      m_bitcoinWalletCard->setBalance(QString("%1 BTC").arg(m_currentMockUser->balance, 0, 'f', 8));
    }

    updateMockTransactionHistory();
  }
}

void QtWalletUI::updateMockTransactionHistory() {
  if (!m_bitcoinWalletCard)
    return;

  if (!m_currentMockUser || m_currentMockUser->transactions.isEmpty()) {
    m_bitcoinWalletCard->setTransactionHistory("No transactions yet.<br><br>This is a demo wallet.");
    return;
  }

  QString historyHtml;
  for (const MockTransaction &tx : m_currentMockUser->transactions) {
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
  if (!m_mainLayout || !m_contentLayout) {
    return;
  }

  int windowWidth = this->width();
  int windowHeight = this->height();
  double aspectRatio = static_cast<double>(windowWidth) / windowHeight;

  // Determine if this is a laptop screen
  bool isLaptopScreen = (aspectRatio >= 1.5 && aspectRatio <= 1.8);

  // Sidebar collapsed width that we need to account for
  const int SIDEBAR_WIDTH = 70;

  // Responsive margins and spacing based on screen size
  int topMargin, rightMargin, bottomMargin, leftMargin;
  int contentMargin, spacing;

  if (windowWidth <= 480) {
    // Very small screens (phones portrait)
    topMargin = rightMargin = bottomMargin = 5;
    leftMargin = SIDEBAR_WIDTH + 5; // Account for sidebar + margin
    contentMargin = 8;
    spacing = 12;
  } else if (windowWidth <= 768) {
    // Small screens (phones landscape, small tablets)
    topMargin = rightMargin = bottomMargin = 8;
    leftMargin = SIDEBAR_WIDTH + 8;
    contentMargin = 12;
    spacing = 15;
  } else if (windowWidth <= 1024) {
    // Tablets and small laptops
    topMargin = rightMargin = bottomMargin = 12;
    leftMargin = SIDEBAR_WIDTH + 12;
    contentMargin = 16;
    spacing = 18;
  } else if (windowWidth <= 1366 && isLaptopScreen) {
    // Common laptop screens (1366x768)
    topMargin = rightMargin = bottomMargin = 15;
    leftMargin = SIDEBAR_WIDTH + 15;
    contentMargin = 20;
    spacing = 20;
  } else if (windowWidth <= 1600 && isLaptopScreen) {
    // Medium laptop screens (1440p, 1536p, 1600p)
    topMargin = rightMargin = bottomMargin = 18;
    leftMargin = SIDEBAR_WIDTH + 18;
    contentMargin = 22;
    spacing = 22;
  } else if (windowWidth <= 1920 && isLaptopScreen) {
    // Full HD laptops
    topMargin = rightMargin = bottomMargin = 20;
    leftMargin = SIDEBAR_WIDTH + 20;
    contentMargin = 24;
    spacing = 24;
  } else if (windowWidth <= 1920) {
    // Desktop monitors up to Full HD
    topMargin = rightMargin = bottomMargin = 25;
    leftMargin = SIDEBAR_WIDTH + 25;
    contentMargin = 28;
    spacing = 24;
  } else if (windowWidth <= 2560) {
    // Large desktop monitors (1440p, QHD)
    topMargin = rightMargin = bottomMargin = 30;
    leftMargin = SIDEBAR_WIDTH + 30;
    contentMargin = 32;
    spacing = 26;
  } else {
    // 4K and ultra-wide monitors
    topMargin = rightMargin = bottomMargin = 35;
    leftMargin = SIDEBAR_WIDTH + 35;
    contentMargin = 36;
    spacing = 28;
  }

  // Apply calculated margins with sidebar offset on the left
  m_mainLayout->setContentsMargins(leftMargin, topMargin, rightMargin, bottomMargin);
  m_contentLayout->setContentsMargins(contentMargin, contentMargin, contentMargin, contentMargin);
  m_mainLayout->setSpacing(spacing);
  m_contentLayout->setSpacing(spacing);

  // Adjust header and card sizing for better proportions
  if (m_headerSection) {
    int headerVerticalPadding = windowWidth <= 768 ? 15 : (windowWidth <= 1366 ? 20 : 25);
    m_headerSection->setContentsMargins(0, headerVerticalPadding, 0, headerVerticalPadding);
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
      m_currentBTCPrice = 43000.0; // Fallback price
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
      m_currentETHPrice = 2500.0; // Fallback price
    }
  }
}

// PHASE 2: Fetch both BTC and ETH prices
void QtWalletUI::fetchAllPrices() {
  fetchBTCPrice();
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

  // PHASE 2: Calculate total balance including both BTC and ETH

  // Use fallback prices if current prices are not available
  double btcPriceToUse = (m_currentBTCPrice > 0.0) ? m_currentBTCPrice : 43000.0;
  double ethPriceToUse = (m_currentETHPrice > 0.0) ? m_currentETHPrice : 2500.0;

  double btcBalance = 0.0;
  double ethBalance = 0.0;

  // Use real balance if not in mock mode
  if (!m_mockMode) {
    btcBalance = m_realBalanceBTC;
    ethBalance = m_realBalanceETH;
  } else if (m_currentMockUser) {
    // Use mock balance if in mock mode (only BTC for now)
    btcBalance = m_currentMockUser->balance;
    ethBalance = 0.0;  // Mock mode doesn't have ETH yet
  }

  // Calculate USD value for each currency
  double btcUsdValue = btcBalance * btcPriceToUse;
  double ethUsdValue = ethBalance * ethPriceToUse;

  // Calculate total USD balance
  double totalUsdBalance = btcUsdValue + ethUsdValue;

  m_balanceLabel->setText(QString("$%L1 USD").arg(totalUsdBalance, 0, 'f', 2));
}

void QtWalletUI::onPriceUpdateTimer() {
  // PHASE 2: This runs every 60 seconds to refresh both BTC and ETH prices
  fetchAllPrices();
}

// === Real Wallet Integration Methods ===

void QtWalletUI::setWallet(WalletAPI::SimpleWallet *wallet) {
  m_wallet = wallet;
  // Fetch initial balance when wallet is set
  if (m_wallet && !m_currentAddress.isEmpty()) {
    fetchRealBalance();
  }
}

// PHASE 1 FIX: Ethereum wallet initialization
void QtWalletUI::setEthereumWallet(WalletAPI::EthereumWallet *ethWallet) {
  m_ethereumWallet = ethWallet;
  // Fetch initial balance when wallet is set
  if (m_ethereumWallet && !m_ethereumAddress.isEmpty()) {
    fetchRealBalance();
  }
}

void QtWalletUI::setEthereumAddress(const QString &address) {
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

  // Fetch Ethereum balance if we have an Ethereum wallet and address
  if (m_ethereumWallet && !m_ethereumAddress.isEmpty()) {
    // PHASE 2: Set loading state for Ethereum
    setLoadingState(true, "Ethereum");

    try {
      std::string ethAddress = m_ethereumAddress.toStdString();
      m_realBalanceETH = m_ethereumWallet->GetBalance(ethAddress);

      // Update Ethereum wallet card
      if (m_ethereumWalletCard) {
        m_ethereumWalletCard->setBalance(QString("%1 ETH").arg(m_realBalanceETH, 0, 'f', 8));
      }

      // PHASE 3: Fetch and display Ethereum transaction history with improved formatting
      auto ethTxHistory = m_ethereumWallet->GetTransactionHistory(ethAddress, 10);
      if (m_ethereumWalletCard) {
        m_ethereumWalletCard->setTransactionHistory(
          formatEthereumTransactionHistory(ethTxHistory, ethAddress)
        );
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

void QtWalletUI::setRepositories(Repository::UserRepository* userRepo, Repository::WalletRepository* walletRepo) {
  m_userRepository = userRepo;
  m_walletRepository = walletRepo;
}

void QtWalletUI::setCurrentUserId(int userId) {
  m_currentUserId = userId;
}

void QtWalletUI::sendRealTransaction(const QString& recipientAddress, uint64_t amountSatoshis,
                                     uint64_t feeSatoshis, const QString& password) {
  if (!m_wallet || !m_walletRepository || m_currentUserId < 0) {
    throw std::runtime_error("Wallet or repositories not properly initialized");
  }

  // Step 1: Retrieve and decrypt the user's seed phrase
  auto seedResult = m_walletRepository->retrieveDecryptedSeed(m_currentUserId, password.toStdString());
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
  std::vector<std::string> fromAddresses = { fromAddress };
  std::string toAddress = recipientAddress.toStdString();

  // Step 7: Send the transaction using WalletAPI
  WalletAPI::SendTransactionResult result = m_wallet->SendFunds(
      fromAddresses,
      toAddress,
      amountSatoshis,
      privateKeysMap,
      feeSatoshis
  );

  // Step 8: Handle result
  if (!result.success) {
    throw std::runtime_error("Transaction failed: " + result.error_message);
  }

  // Step 9: Show success message
  QMessageBox::information(this, "Transaction Sent",
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

std::vector<uint8_t> QtWalletUI::derivePrivateKeyForAddress(const QString& address, const QString& password) {
  if (!m_walletRepository || m_currentUserId < 0) {
    throw std::runtime_error("Wallet repository not properly initialized");
  }

  // Step 1: Retrieve and decrypt the user's seed phrase
  auto seedResult = m_walletRepository->retrieveDecryptedSeed(m_currentUserId, password.toStdString());
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
  } else if (chain.toLower() == "ethereum" || chain.toLower() == "eth") {
    m_isLoadingETH = loading;
  }

  clearErrorState();
  updateStatusLabel();
}

void QtWalletUI::setErrorState(const QString& errorMessage) {
  m_lastErrorMessage = errorMessage;
  m_isLoadingBTC = false;
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
  else if (m_isLoadingBTC && m_isLoadingETH) {
    statusText = "Loading Bitcoin and Ethereum balances...";
    styleClass = "loading";
    m_statusLabel->setVisible(true);
  }
  else if (m_isLoadingBTC) {
    statusText = "Loading Bitcoin balance...";
    styleClass = "loading";
    m_statusLabel->setVisible(true);
  }
  else if (m_isLoadingETH) {
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

    m_statusLabel->setStyleSheet(QString(
      "QLabel {"
      "  color: %1;"
      "  background-color: %2;"
      "  padding: 8px;"
      "  border-radius: 4px;"
      "  font-size: 12px;"
      "}"
    ).arg(textColor.name()).arg(bgColor.name()));
  }
}

// PHASE 3: Transaction history formatting helpers

QString QtWalletUI::formatBitcoinTransactionHistory(const std::vector<std::string>& txHashes) {
  if (txHashes.empty()) {
    return "No transactions yet.<br><br>Send testnet Bitcoin to your address to see it appear here!";
  }

  QString historyHtml;
  historyHtml += "<div style='font-size: 12px;'>";
  historyHtml += QString("<b>Recent Transactions</b> (%1 total)<br><br>").arg(txHashes.size());

  int count = 0;
  for (const auto& txHash : txHashes) {
    if (count >= 5) break; // Show only first 5 transactions

    QString shortHash = QString::fromStdString(txHash).left(16) + "...";
    historyHtml += QString(
      "<div style='margin-bottom: 12px; padding: 8px; background: rgba(128,128,128,0.1); border-radius: 4px;'>"
      "<b>TX:</b> <span style='font-family: monospace;'>%1</span><br>"
      "<span style='font-size: 10px; color: #666;'>Tap to view on block explorer</span>"
      "</div>"
    ).arg(shortHash);

    count++;
  }

  if (txHashes.size() > 5) {
    historyHtml += QString("<br><i>... and %1 more transactions</i>").arg(txHashes.size() - 5);
  }

  historyHtml += "</div>";
  return historyHtml;
}

QString QtWalletUI::formatEthereumTransactionHistory(const std::vector<EthereumService::Transaction>& txs,
                                                      const std::string& userAddress) {
  if (txs.empty()) {
    return "No transactions yet.<br><br>Send Ethereum to your address to see it appear here!";
  }

  QString historyHtml;
  historyHtml += "<div style='font-size: 12px;'>";
  historyHtml += QString("<b>Recent Transactions</b> (%1 total)<br><br>").arg(txs.size());

  int count = 0;
  for (const auto& tx : txs) {
    if (count >= 5) break; // Show only first 5 transactions

    QString type = (tx.to == userAddress) ? "Received" : "Sent";
    QString typeColor = (tx.to == userAddress)
        ? m_themeManager->positiveColor().name()  // Green for received
        : m_themeManager->negativeColor().name(); // Red for sent
    QString statusIcon = tx.is_error ? "❌" : "✅";
    QString shortHash = QString::fromStdString(tx.hash).left(16) + "...";

    historyHtml += QString(
      "<div style='margin-bottom: 12px; padding: 8px; background: rgba(128,128,128,0.1); border-radius: 4px;'>"
      "<b style='color: %1;'>%2:</b> %3 ETH %4<br>"
      "<span style='font-family: monospace; font-size: 10px;'>%5</span><br>"
      "<span style='font-size: 10px; color: #666;'>Block: %6</span>"
      "</div>"
    ).arg(typeColor)
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

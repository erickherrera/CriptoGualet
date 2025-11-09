// QtWalletUI.cpp
#include "QtWalletUI.h"
#include "QtExpandableWalletCard.h"
#include "QtThemeManager.h"
#include "QtSendDialog.h"
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
      m_toggleBalanceButton(nullptr), m_bitcoinWalletCard(nullptr),
      m_currentMockUser(nullptr), m_wallet(nullptr), m_balanceUpdateTimer(nullptr),
      m_realBalanceBTC(0.0), m_userRepository(nullptr), m_walletRepository(nullptr),
      m_currentUserId(-1), m_priceFetcher(nullptr),
      m_priceUpdateTimer(nullptr), m_currentBTCPrice(43000.0),
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

    // Fetch initial price
    fetchBTCPrice();
  });

  // Connect theme changed signal
  if (m_themeManager) {
    connect(m_themeManager, &QtThemeManager::themeChanged, this,
            &QtWalletUI::onThemeChanged);
  }
}

void QtWalletUI::setupUI() {
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(20, 20, 20, 20);
  m_mainLayout->setSpacing(20);

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
  m_toggleBalanceButton->setToolTip("Hide/Show Balance");
  m_toggleBalanceButton->setCursor(Qt::PointingHandCursor);
  balanceRowLayout->addWidget(m_toggleBalanceButton);

  balanceVerticalLayout->addLayout(balanceRowLayout);
  headerLayout->addLayout(balanceVerticalLayout);

  connect(m_toggleBalanceButton, &QPushButton::clicked, this,
          &QtWalletUI::onToggleBalanceClicked);

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
  QtSendDialog dialog(currentBalance, m_currentBTCPrice, this);
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
        double feeBTC = txData.estimatedFeeSatoshis / 100000000.0;
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

void QtWalletUI::onReceiveBitcoinClicked() { emit receiveBitcoinRequested(); }

void QtWalletUI::onThemeChanged() {
  applyTheme();
  if (m_bitcoinWalletCard) {
    m_bitcoinWalletCard->applyTheme();
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
  bool isDarkTheme = m_themeManager->surfaceColor().lightness() < 128;

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
    )").arg(isDarkTheme ? m_themeManager->textColor().darker(120).name() : text)
       .arg(balanceTitleSize);
    m_balanceTitle->setStyleSheet(balanceTitleStyle);

    QFont balanceTitleFont = m_themeManager->textFont();
    balanceTitleFont.setPointSize(balanceTitleSize);
    balanceTitleFont.setBold(true);
    m_balanceTitle->setFont(balanceTitleFont);
  }

  // Balance amount - large and prominent with accent color
  if (m_balanceLabel) {
    QString balanceColor = isDarkTheme
        ? m_themeManager->accentColor().lighter(120).name()
        : m_themeManager->accentColor().darker(105).name();

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
    )").arg(m_themeManager->surfaceColor().lighter(isDarkTheme ? 120 : 95).name())
       .arg(toggleButtonSize);
    m_toggleBalanceButton->setStyleSheet(toggleButtonStyle);

    // Use emoji icons (SVG loading disabled for stability)
    m_toggleBalanceButton->setText(m_balanceVisible ? "👁" : "🚫");
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

void QtWalletUI::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateScrollAreaWidth();
  updateResponsiveLayout();
  adjustButtonLayout();
  updateCardSizes();
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

  // Use fallback price if current price is not available
  double priceToUse = (m_currentBTCPrice > 0.0) ? m_currentBTCPrice : 43000.0;
  double btcBalance = 0.0;

  // Use real balance if not in mock mode
  if (!m_mockMode) {
    btcBalance = m_realBalanceBTC;
  } else if (m_currentMockUser) {
    // Use mock balance if in mock mode
    btcBalance = m_currentMockUser->balance;
  }

  // Calculate and display USD balance
  double usdBalance = btcBalance * priceToUse;
  m_balanceLabel->setText(QString("$%L1 USD").arg(usdBalance, 0, 'f', 2));
}

void QtWalletUI::onPriceUpdateTimer() {
  // This runs every 60 seconds to refresh the BTC price
  fetchBTCPrice();
}

// === Real Wallet Integration Methods ===

void QtWalletUI::setWallet(WalletAPI::SimpleWallet *wallet) {
  m_wallet = wallet;
  // Fetch initial balance when wallet is set
  if (m_wallet && !m_currentAddress.isEmpty()) {
    fetchRealBalance();
  }
}

void QtWalletUI::fetchRealBalance() {
  if (!m_wallet || m_currentAddress.isEmpty()) {
    return;
  }

  // Disable mock mode when fetching real balance
  m_mockMode = false;

  // Fetch balance from blockchain
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

  // Fetch and display transaction history
  auto txHistory = m_wallet->GetTransactionHistory(address, 10);
  if (!txHistory.empty()) {
    QString historyHtml;
    for (const auto& txHash : txHistory) {
      // In production, you'd fetch full transaction details for each hash
      historyHtml += QString("<b>Transaction:</b> %1<br><br>")
                         .arg(QString::fromStdString(txHash));
    }
    if (m_bitcoinWalletCard) {
      m_bitcoinWalletCard->setTransactionHistory(historyHtml);
    }
  } else {
    if (m_bitcoinWalletCard) {
      m_bitcoinWalletCard->setTransactionHistory(
          "No transactions yet.<br><br>Send testnet Bitcoin to your address to see it appear here!");
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
          .arg(amountSatoshis / 100000000.0, 0, 'f', 8)
          .arg(result.total_fees / 100000000.0, 0, 'f', 8));

  // Step 10: Clean up sensitive data
  // TODO: Re-enable secure memory cleanup after fixing linkage
  // Crypto::SecureWipeVector(privateKeyBytes);
  // Crypto::SecureZeroMemory(seed.data(), seed.size());
  privateKeyBytes.clear();
}

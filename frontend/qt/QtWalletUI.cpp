// QtWalletUI.cpp
#include "QtWalletUI.h"
#include "QtExpandableWalletCard.h"
#include "QtThemeManager.h"

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
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()),
      m_currentMockUser(nullptr), m_balanceVisible(true), m_mockMode(false) {
  initializeMockUsers();
  setupUI();
  applyTheme();

  connect(m_themeManager, &QtThemeManager::themeChanged, this,
          &QtWalletUI::onThemeChanged);
}

void QtWalletUI::setupUI() {
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(20, 20, 20, 20);
  m_mainLayout->setSpacing(20);

  // Create a horizontal layout to center content with max width
  QHBoxLayout *centeringLayout = new QHBoxLayout();

  // Add left spacer
  centeringLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

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
  centeringLayout->addWidget(m_scrollArea);

  // Add right spacer
  centeringLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

  m_mainLayout->addLayout(centeringLayout);

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
  m_headerTitle->setProperty("class", "header-title");
  m_headerTitle->setAlignment(Qt::AlignCenter);
  headerLayout->addWidget(m_headerTitle);

  // Balance section with hide button
  // Vertical layout for title above amount
  QVBoxLayout *balanceVerticalLayout = new QVBoxLayout();
  balanceVerticalLayout->setSpacing(5);

  m_balanceTitle = new QLabel("Total Balance:", m_headerSection);
  m_balanceTitle->setProperty("class", "balance-title");
  m_balanceTitle->setAlignment(Qt::AlignCenter);
  balanceVerticalLayout->addWidget(m_balanceTitle);

  // Horizontal layout for amount and toggle button
  QHBoxLayout *balanceRowLayout = new QHBoxLayout();
  balanceRowLayout->setSpacing(0);

  // Left spacer to offset the icon width (centers the text)
  balanceRowLayout->addSpacing(20); // Half of icon width (32/2 + spacing)

  // Left stretch
  balanceRowLayout->addStretch();

  // Balance amount
  m_balanceLabel = new QLabel("$0.00 USD", m_headerSection);
  m_balanceLabel->setProperty("class", "balance-amount");
  m_balanceLabel->setAlignment(Qt::AlignCenter);
  balanceRowLayout->addWidget(m_balanceLabel);

  // Small spacing between amount and icon
  balanceRowLayout->addSpacing(8);

  // Toggle button
  m_toggleBalanceButton = new QPushButton(m_headerSection);
  m_toggleBalanceButton->setIconSize(QSize(20, 20));
  m_toggleBalanceButton->setMaximumWidth(32);
  m_toggleBalanceButton->setMaximumHeight(32);
  m_toggleBalanceButton->setProperty("class", "toggle-balance");
  m_toggleBalanceButton->setToolTip("Hide/Show Balance");
  balanceRowLayout->addWidget(m_toggleBalanceButton);

  // Right stretch
  balanceRowLayout->addStretch();

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
}

void QtWalletUI::onViewBalanceClicked() {
  // Mock balance data for testing
  double mockBalance = 0.15234567;
  double btcPrice = 43000.0; // Mock BTC price
  double usdBalance = mockBalance * btcPrice;

  if (m_balanceVisible) {
    m_balanceLabel->setText(QString("$%L1 USD").arg(usdBalance, 0, 'f', 2));
  }

  QMessageBox::information(
      this, "Balance Updated",
      QString("Your current balance:\n%1 BTC\n$%L2 USD\n\nThis is "
              "mock data for testing purposes.")
          .arg(QString::number(mockBalance, 'f', 8))
          .arg(usdBalance, 0, 'f', 2));

  emit viewBalanceRequested();
}

void QtWalletUI::onSendBitcoinClicked() {
  // Mock send bitcoin dialog for testing
  bool ok;
  QString recipient = QInputDialog::getText(
      this, "Send Bitcoin", "Enter recipient address:", QLineEdit::Normal, "",
      &ok);

  if (ok && !recipient.isEmpty()) {
    QString amount = QInputDialog::getText(
        this, "Send Bitcoin", "Enter amount (BTC):", QLineEdit::Normal, "0.001",
        &ok);

    if (ok && !amount.isEmpty()) {
      QMessageBox::StandardButton reply = QMessageBox::question(
          this, "Confirm Transaction",
          QString("Send %1 BTC to:\n%2\n\nThis is a mock transaction for "
                  "testing. No actual Bitcoin will be sent.\n\nConfirm?")
              .arg(amount, recipient),
          QMessageBox::Yes | QMessageBox::No);

      if (reply == QMessageBox::Yes) {
        QMessageBox::information(
            this, "Transaction Sent",
            QString("Mock transaction of %1 BTC sent "
                    "successfully!\n\nIn a real implementation, this would be added to the transaction history.")
                .arg(amount));
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
  setStyleSheet(m_themeManager->getMainWindowStyleSheet());

  const QString accent = m_themeManager->accentColor().name();
  const QString text = m_themeManager->textColor().name();

  // Header section styling
  QString headerTitleStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: 42px;
            font-weight: 700;
            letter-spacing: -1px;
            background-color: transparent;
            padding: 0px;
            margin: 0px;
        }
    )")
                                 .arg(text);

  QString balanceTitleStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: 21px;
            font-weight: 700;
            background-color: transparent;
            padding: 5px 10px;
            margin: 0px;
        }
    )")
                                  .arg(text);

  // Better contrast for balance amount
  bool isDarkTheme = m_themeManager->surfaceColor().lightness() < 128;
  QString balanceTextColor = isDarkTheme ? m_themeManager->textColor().lighter(115).name() : text;

  QString balanceAmountStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: 21px;
            font-weight: 600;
            background-color: transparent;
            padding: 5px 15px;
            margin: 0px;
        }
    )")
                                   .arg(balanceTextColor);

  // Toggle button uses theme colors with better contrast
  QString iconColor = isDarkTheme ? m_themeManager->accentColor().lighter(110).name() : text;

  QString toggleButtonStyle =
      QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: none;
            border-radius: 16px;
            padding: 0px;
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
        }
        QPushButton:hover {
            background-color: %2;
        }
        QPushButton:pressed {
            background-color: %3;
        }
    )")
          .arg(iconColor)
          .arg(m_themeManager->surfaceColor().lighter(120).name())
          .arg(m_themeManager->accentColor().lighter(160).name());

  m_headerTitle->setStyleSheet(headerTitleStyle);
  m_balanceTitle->setStyleSheet(balanceTitleStyle);
  m_balanceLabel->setStyleSheet(balanceAmountStyle);
  m_toggleBalanceButton->setStyleSheet(toggleButtonStyle);

  // Set the icon color based on theme with better contrast
  QColor iconColorValue = isDarkTheme
      ? m_themeManager->accentColor().lighter(110)
      : m_themeManager->textColor();
  QIcon eyeOpenIcon =
      createColoredIcon(":/icons/icons/eye-open.svg", iconColorValue);

  // Update the icon if balance is visible
  if (m_balanceVisible) {
    m_toggleBalanceButton->setIcon(eyeOpenIcon);
  } else {
    QIcon eyeClosedIcon =
        createColoredIcon(":/icons/icons/eye-closed.svg", iconColorValue);
    m_toggleBalanceButton->setIcon(eyeClosedIcon);
  }

  // Set fonts for header section
  QFont headerFont = m_themeManager->titleFont();
  headerFont.setPointSize(42);
  headerFont.setBold(true);
  m_headerTitle->setFont(headerFont);

  QFont balanceTitleFont = m_themeManager->titleFont();
  balanceTitleFont.setPointSize(21);
  balanceTitleFont.setBold(true);
  m_balanceTitle->setFont(balanceTitleFont);

  QFont balanceAmountFont = m_themeManager->textFont();
  balanceAmountFont.setPointSize(21);
  balanceAmountFont.setBold(false);
  m_balanceLabel->setFont(balanceAmountFont);
}

void QtWalletUI::onLogoutClicked() { emit logoutRequested(); }

void QtWalletUI::onToggleBalanceClicked() {
  m_balanceVisible = !m_balanceVisible;

  // Determine icon color based on theme with better contrast
  bool isDarkTheme = m_themeManager->surfaceColor().lightness() < 128;
  QColor iconColor = isDarkTheme
      ? m_themeManager->accentColor().lighter(110)
      : m_themeManager->textColor();

  if (m_balanceVisible) {
    // Show balance - open eye icon
    m_toggleBalanceButton->setIcon(
        createColoredIcon(":/icons/icons/eye-open.svg", iconColor));
    // Restore the actual balance value
    if (m_currentMockUser) {
      double btcPrice = 43000.0; // Mock BTC price
      double usdBalance = m_currentMockUser->balance * btcPrice;
      m_balanceLabel->setText(QString("$%L1 USD").arg(usdBalance, 0, 'f', 2));
    } else {
      m_balanceLabel->setText("$0.00 USD");
    }
  } else {
    // Hide balance - closed/crossed eye icon
    m_toggleBalanceButton->setIcon(
        createColoredIcon(":/icons/icons/eye-closed.svg", iconColor));
    m_balanceLabel->setText("••••••");
  }
}

void QtWalletUI::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateScrollAreaWidth();
  updateResponsiveLayout();
  adjustButtonLayout();
  updateCardSizes();
}

void QtWalletUI::updateScrollAreaWidth() {
  if (m_scrollArea) {
    int windowWidth = this->width();
    int windowHeight = this->height();
    double aspectRatio = static_cast<double>(windowWidth) / windowHeight;

    // Enhanced responsive width calculation
    if (windowWidth > 2560 || aspectRatio > 2.4) {
      // Ultra-wide screens
      int targetWidth = static_cast<int>(windowWidth * 0.575);
      m_scrollArea->setFixedWidth(targetWidth);
    } else if (windowWidth > 1920) {
      // Large screens
      int targetWidth = static_cast<int>(windowWidth * 0.65);
      m_scrollArea->setMaximumWidth(targetWidth);
      m_scrollArea->setMinimumWidth(800);
    } else if (windowWidth > 1200) {
      // Medium screens
      int targetWidth = static_cast<int>(windowWidth * 0.75);
      m_scrollArea->setMaximumWidth(targetWidth);
      m_scrollArea->setMinimumWidth(600);
    } else {
      // Small screens - use 100% of the width
      m_scrollArea->setMaximumWidth(QWIDGETSIZE_MAX);
      m_scrollArea->setMinimumWidth(windowWidth);
      m_scrollArea->setFixedWidth(windowWidth);
    }
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

    // Convert BTC balance to USD for header
    double btcPrice = 43000.0; // Mock BTC price
    double usdBalance = m_currentMockUser->balance * btcPrice;
    m_balanceLabel->setText(QString("$%L1 USD").arg(usdBalance, 0, 'f', 2));

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
  if (!m_mainLayout)
    return;

  int windowWidth = this->width();

  // Adjust main layout margins based on screen size
  if (windowWidth < 600) {
    // Mobile/small screens - minimal margins for 100% usage
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setContentsMargins(5, 5, 5, 5);
  } else if (windowWidth < 1200) {
    // Tablet/medium screens - standard margins
    m_mainLayout->setContentsMargins(15, 15, 15, 15);
    m_contentLayout->setContentsMargins(15, 15, 15, 15);
  } else {
    // Desktop/large screens - larger margins
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_contentLayout->setContentsMargins(20, 20, 20, 20);
  }

  // Adjust spacing between elements
  int spacing = windowWidth < 600 ? 15 : (windowWidth < 1200 ? 18 : 20);
  m_mainLayout->setSpacing(spacing);
  m_contentLayout->setSpacing(spacing);
}

void QtWalletUI::adjustButtonLayout() {
  // Responsive layout handled by the reusable wallet card component
}

void QtWalletUI::updateCardSizes() {
  // Card sizing handled by the reusable wallet card component
}

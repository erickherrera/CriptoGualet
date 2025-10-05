// QtWalletUI.cpp
#include "../include/QtWalletUI.h"
#include "../include/QtThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
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
      m_currentMockUser(nullptr), m_mockMode(false), m_balanceVisible(true) {
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
  createAddressSection();
  createActionButtons();
  createTransactionHistory();

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

void QtWalletUI::createAddressSection() {
  m_addressCard = new QFrame(m_scrollContent);
  m_addressCard->setProperty("class", "card");
  m_addressCard->setObjectName("addressCard");

  QVBoxLayout *addressLayout = new QVBoxLayout(m_addressCard);
  addressLayout->setContentsMargins(25, 25, 25, 25);
  addressLayout->setSpacing(15);

  m_addressTitleLabel = new QLabel("Your Bitcoin Address", m_addressCard);
  m_addressTitleLabel->setProperty("class", "title");
  addressLayout->addWidget(m_addressTitleLabel);

  QHBoxLayout *addressRowLayout = new QHBoxLayout();

  m_addressLabel = new QLabel("", m_addressCard);
  m_addressLabel->setProperty("class", "address");
  m_addressLabel->setWordWrap(false);
  m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  addressRowLayout->addWidget(m_addressLabel, 1);

  m_copyAddressButton = new QPushButton("Copy", m_addressCard);
  m_copyAddressButton->setMaximumWidth(80);
  addressRowLayout->addWidget(m_copyAddressButton);

  addressLayout->addLayout(addressRowLayout);

  connect(m_copyAddressButton, &QPushButton::clicked, [this]() {
    if (!m_currentAddress.isEmpty()) {
      QApplication::clipboard()->setText(m_currentAddress);
      QMessageBox::information(this, "Copied", "Address copied to clipboard!");
    }
  });

  m_contentLayout->addWidget(m_addressCard);
}

void QtWalletUI::createActionButtons() {
  m_actionsCard = new QFrame(m_scrollContent);
  m_actionsCard->setProperty("class", "card");
  m_actionsCard->setObjectName("actionsCard");

  QVBoxLayout *actionsMainLayout = new QVBoxLayout(m_actionsCard);
  actionsMainLayout->setContentsMargins(25, 25, 25, 25);
  actionsMainLayout->setSpacing(20);

  // Title styled the same as other sections
  m_actionsTitle = new QLabel("Wallet Actions", m_actionsCard);
  m_actionsTitle->setProperty("class", "title");
  actionsMainLayout->addWidget(m_actionsTitle);

  m_actionsLayout = new QHBoxLayout();
  m_actionsLayout->setSpacing(15);

  m_viewBalanceButton = new QPushButton("View Balance", m_actionsCard);
  m_viewBalanceButton->setMinimumHeight(60);
  m_viewBalanceButton->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Fixed);
  m_actionsLayout->addWidget(m_viewBalanceButton);

  m_sendButton = new QPushButton("Send Bitcoin", m_actionsCard);
  m_sendButton->setMinimumHeight(60);
  m_sendButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_actionsLayout->addWidget(m_sendButton);

  m_receiveButton = new QPushButton("Receive Bitcoin", m_actionsCard);
  m_receiveButton->setMinimumHeight(60);
  m_receiveButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_actionsLayout->addWidget(m_receiveButton);

  actionsMainLayout->addLayout(m_actionsLayout);

  connect(m_viewBalanceButton, &QPushButton::clicked, this,
          &QtWalletUI::onViewBalanceClicked);
  connect(m_sendButton, &QPushButton::clicked, this,
          &QtWalletUI::onSendBitcoinClicked);
  connect(m_receiveButton, &QPushButton::clicked, this,
          &QtWalletUI::onReceiveBitcoinClicked);

  m_contentLayout->addWidget(m_actionsCard);
}

void QtWalletUI::createTransactionHistory() {
  m_historyCard = new QFrame(m_scrollContent);
  m_historyCard->setProperty("class", "card");
  m_historyCard->setObjectName("historyCard");

  QVBoxLayout *historyLayout = new QVBoxLayout(m_historyCard);
  historyLayout->setContentsMargins(25, 25, 25, 25);
  historyLayout->setSpacing(15);

  m_historyTitleLabel = new QLabel("Transaction History", m_historyCard);
  m_historyTitleLabel->setProperty("class", "title");
  historyLayout->addWidget(m_historyTitleLabel);

  m_historyText = new QTextEdit(m_historyCard);
  m_historyText->setReadOnly(true);
  m_historyText->setMaximumHeight(200);
  m_historyText->setText(
      "No transactions yet.\n\nThis is a demo wallet. In a real "
      "implementation, transaction history would be displayed here.");
  historyLayout->addWidget(m_historyText);

  m_contentLayout->addWidget(m_historyCard);
}

void QtWalletUI::setUserInfo(const QString &username, const QString &address) {
  m_currentUsername = username;
  m_currentAddress = address;

  m_addressLabel->setText(address);
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
        // Update transaction history with mock data
        QString currentHistory = m_historyText->toPlainText();
        if (currentHistory.contains("No transactions yet.")) {
          currentHistory = "";
        }

        QString newTransaction =
            QString("SENT: %1 BTC to %2\nTime: %3\nStatus: Pending (Mock)\n\n")
                .arg(amount)
                .arg(recipient.left(20) + "...")
                .arg(QDateTime::currentDateTime().toString(
                    "yyyy-MM-dd hh:mm:ss"));

        m_historyText->setText(newTransaction + currentHistory);

        QMessageBox::information(
            this, "Transaction Sent",
            QString("Mock transaction of %1 BTC sent "
                    "successfully!\n\nTransaction added to history.")
                .arg(amount));
      }
    }
  }

  emit sendBitcoinRequested();
}

void QtWalletUI::onReceiveBitcoinClicked() { emit receiveBitcoinRequested(); }

void QtWalletUI::onThemeChanged() { applyTheme(); }

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
  const QString surface = m_themeManager->surfaceColor().name();
  const QString text = m_themeManager->textColor().name();

  auto cardCss = [&](const QString &objName) {
    return QString(R"(
            QFrame#%1 {
                background-color: %2;
                border: 2px solid %3;
                border-radius: 12px;
                padding: 20px;
            }
        )")
        .arg(objName, surface, accent);
  };

  m_addressCard->setStyleSheet(cardCss("addressCard"));
  m_actionsCard->setStyleSheet(cardCss("actionsCard"));
  m_historyCard->setStyleSheet(cardCss("historyCard"));

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

  QString balanceAmountStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: 21px;
            font-weight: 400;
            background-color: transparent;
            padding: 5px 15px;
            margin: 0px;
        }
    )")
                                   .arg(text);

  // Toggle button uses theme colors
  // For dark themes, icon color should be accent color
  // For light themes, icon color should be text color
  bool isDarkTheme =
      (m_themeManager->getCurrentTheme() == ThemeType::DARK ||
       m_themeManager->getCurrentTheme() == ThemeType::CRYPTO_DARK);

  QString iconColor = isDarkTheme ? accent : text;

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

  // Set the icon color based on theme
  QColor iconColorValue =
      isDarkTheme ? m_themeManager->accentColor() : m_themeManager->textColor();
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

  QString labelStyle = m_themeManager->getLabelStyleSheet();
  m_addressTitleLabel->setStyleSheet(labelStyle);
  m_addressLabel->setStyleSheet(labelStyle);
  m_actionsTitle->setStyleSheet(labelStyle);
  m_historyTitleLabel->setStyleSheet(labelStyle);

  QString buttonStyle = m_themeManager->getButtonStyleSheet();
  m_viewBalanceButton->setStyleSheet(buttonStyle);
  m_sendButton->setStyleSheet(buttonStyle);
  m_receiveButton->setStyleSheet(buttonStyle);
  m_copyAddressButton->setStyleSheet(buttonStyle);

  QString textEditStyle = QString(R"(
        QTextEdit {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 6px;
            padding: 8px;
            font-family: %3;
            font-size: %4px;
        }
    )")
                              .arg(surface)
                              .arg(m_themeManager->textColor().name())
                              .arg(m_themeManager->textFont().family())
                              .arg(m_themeManager->textFont().pointSize());

  m_historyText->setStyleSheet(textEditStyle);

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

  m_addressTitleLabel->setFont(m_themeManager->titleFont());
  m_addressLabel->setFont(m_themeManager->monoFont());
  m_actionsTitle->setFont(m_themeManager->titleFont());
  m_historyTitleLabel->setFont(m_themeManager->titleFont());
  m_historyText->setFont(m_themeManager->textFont());

  m_viewBalanceButton->setFont(m_themeManager->buttonFont());
  m_sendButton->setFont(m_themeManager->buttonFont());
  m_receiveButton->setFont(m_themeManager->buttonFont());
  m_copyAddressButton->setFont(m_themeManager->buttonFont());
}

void QtWalletUI::onLogoutClicked() { emit logoutRequested(); }

void QtWalletUI::onToggleBalanceClicked() {
  m_balanceVisible = !m_balanceVisible;

  // Determine icon color based on theme
  bool isDarkTheme =
      (m_themeManager->getCurrentTheme() == ThemeType::DARK ||
       m_themeManager->getCurrentTheme() == ThemeType::CRYPTO_DARK);
  QColor iconColor =
      isDarkTheme ? m_themeManager->accentColor() : m_themeManager->textColor();

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

    // Convert BTC balance to USD
    double btcPrice = 43000.0; // Mock BTC price
    double usdBalance = m_currentMockUser->balance * btcPrice;
    m_balanceLabel->setText(QString("$%L1 USD").arg(usdBalance, 0, 'f', 2));

    updateMockTransactionHistory();
  }
}

void QtWalletUI::updateMockTransactionHistory() {
  if (!m_currentMockUser || m_currentMockUser->transactions.isEmpty()) {
    m_historyText->setText("No transactions yet.\n\nThis is a demo wallet.");
    return;
  }

  QString historyText;
  for (const MockTransaction &tx : m_currentMockUser->transactions) {
    historyText +=
        QString("%1: %2 BTC %3 %4\nTime: %5\nStatus: %6\nTx ID: %7\n\n")
            .arg(tx.type)
            .arg(QString::number(tx.amount, 'f', 8))
            .arg(tx.type == "SENT" ? "to" : "from")
            .arg(tx.address.left(20) + "...")
            .arg(tx.timestamp)
            .arg(tx.status)
            .arg(tx.txId);
  }

  m_historyText->setText(historyText);
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
  if (!m_actionsLayout)
    return;

  int windowWidth = this->width();

  // Switch between horizontal and vertical button layout based on width
  if (windowWidth < 600) {
    // Small screens - stack buttons vertically
    m_actionsLayout->setDirection(QBoxLayout::TopToBottom);
    m_viewBalanceButton->setMinimumHeight(50);
    m_sendButton->setMinimumHeight(50);
    m_receiveButton->setMinimumHeight(50);
  } else {
    // Larger screens - horizontal button layout
    m_actionsLayout->setDirection(QBoxLayout::LeftToRight);
    m_viewBalanceButton->setMinimumHeight(60);
    m_sendButton->setMinimumHeight(60);
    m_receiveButton->setMinimumHeight(60);
  }

  // Adjust button spacing
  int buttonSpacing = windowWidth < 600 ? 10 : 15;
  m_actionsLayout->setSpacing(buttonSpacing);
}

void QtWalletUI::updateCardSizes() {
  if (!m_addressCard || !m_actionsCard || !m_historyCard)
    return;

  int windowWidth = this->width();
  int windowHeight = this->height();

  // Calculate responsive card margins and padding
  int cardMargin, cardPadding;
  if (windowWidth < 600) {
    cardMargin = 10;
    cardPadding = 10;
  } else if (windowWidth < 1200) {
    cardMargin = 20;
    cardPadding = 20;
  } else {
    cardMargin = 25;
    cardPadding = 25;
  }

  // Update card layouts
  auto updateCardLayout = [cardMargin, cardPadding](QFrame *card) {
    if (card && card->layout()) {
      card->layout()->setContentsMargins(cardPadding, cardPadding, cardPadding,
                                         cardPadding);
      if (auto *vbox = qobject_cast<QVBoxLayout *>(card->layout())) {
        vbox->setSpacing(cardMargin - 5); // Slightly less spacing than padding
      }
    }
  };

  updateCardLayout(m_addressCard);
  updateCardLayout(m_actionsCard);
  updateCardLayout(m_historyCard);

  // Adjust transaction history height based on available space
  int historyHeight;
  if (windowWidth < 600) {
    // For small screens, use more of the available height
    historyHeight = qMax(150, static_cast<int>(windowHeight * 0.25));
  } else {
    historyHeight = windowWidth < 1200 ? 175 : 200;
  }
  m_historyText->setMaximumHeight(historyHeight);
}

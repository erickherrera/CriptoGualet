#include "CriptoGualetQt.h"
#include "Auth.h"
#include "QtLoginUI.h"
#include "QtSeedDisplayDialog.h"
#include "QtSettingsUI.h"
#include "QtSidebar.h"
#include "QtThemeManager.h"
#include "QtTopCryptosPage.h"
#include "QtWalletUI.h"
#include "SharedTypes.h"
#include "WalletAPI.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <map>
#include <string>

extern std::map<std::string, User> g_users;
extern std::string g_currentUser;

CriptoGualetQt::CriptoGualetQt(QWidget *parent)
    : QMainWindow(parent), m_stackedWidget(nullptr), m_loginUI(nullptr),
      m_walletUI(nullptr), m_themeManager(&QtThemeManager::instance()) {
  setWindowTitle("CriptoGualet - Secure Bitcoin Wallet");
  setMinimumSize(800, 600);

  // Set window to windowed fullscreen (maximized)
  setWindowState(Qt::WindowMaximized);

  // Ensure window is visible and properly positioned
  setWindowFlags(Qt::Window);
  setAttribute(Qt::WA_ShowWithoutActivating, false);

  // Initialize wallet
  m_wallet = std::make_unique<WalletAPI::SimpleWallet>("btc/test3");

  setupUI();
  setupMenuBar();
  setupStatusBar();

  // Apply initial styling
  applyNavbarStyling();

  connect(m_themeManager, &QtThemeManager::themeChanged, this,
          &CriptoGualetQt::onThemeChanged);

  showLoginScreen();
}

void CriptoGualetQt::setupUI() {
  // Create central widget with main layout
  m_centralWidget = new QWidget(this);
  setCentralWidget(m_centralWidget);

  m_mainLayout = new QVBoxLayout(m_centralWidget);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(0);

  // Create navbar
  createNavbar();

  // Create a container widget that will hold the stacked widget
  QWidget *contentContainer = new QWidget(m_centralWidget);
  contentContainer->setObjectName("contentContainer");

  // Use vertical layout for just the stacked widget
  QVBoxLayout *contentLayout = new QVBoxLayout(contentContainer);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  // Create stacked widget for pages (fills the entire container)
  m_stackedWidget = new QStackedWidget(contentContainer);

  m_loginUI = new QtLoginUI(this);
  m_walletUI = new QtWalletUI(this);
  m_settingsUI = new QtSettingsUI(this);
  m_topCryptosPage = new QtTopCryptosPage(this);

  // Pass wallet instance to wallet UI for real balance fetching
  m_walletUI->setWallet(m_wallet.get());

  m_stackedWidget->addWidget(m_loginUI);
  m_stackedWidget->addWidget(m_walletUI);
  m_stackedWidget->addWidget(m_settingsUI);
  m_stackedWidget->addWidget(m_topCryptosPage);

  contentLayout->addWidget(m_stackedWidget);

  m_mainLayout->addWidget(contentContainer);

  // Create sidebar as an OVERLAY on top of the content container
  m_sidebar = new QtSidebar(m_themeManager, contentContainer);
  m_sidebar->raise(); // Ensure sidebar is on top
  m_sidebar->hide();  // Initially hidden

  // Connect sidebar navigation signals
  createSidebar();

  connect(
      m_loginUI, &QtLoginUI::loginRequested,
      [this](const QString &username, const QString &password) {
        std::string stdUsername = username.toStdString();
        std::string stdPassword = password.toStdString();

        // Check for mock user first
        if (m_walletUI->authenticateMockUser(username, password)) {
          g_currentUser = stdUsername;
          showWalletScreen();
          statusBar()->showMessage("Mock login successful (testuser)", 3000);
          m_loginUI->onLoginResult(
              true, "Mock login successful - testuser authenticated");
          return;
        }

        // Normal user authentication
        Auth::AuthResponse response = Auth::LoginUser(stdUsername, stdPassword);
        QString message = QString::fromStdString(response.message);

        if (response.success()) {
          g_currentUser = stdUsername;
          m_walletUI->setUserInfo(
              username,
              QString::fromStdString(g_users[stdUsername].walletAddress));
          showWalletScreen();
          statusBar()->showMessage("Login successful", 3000);
        } else {
          statusBar()->showMessage("Login failed", 3000);
        }

        // Send result back to login UI for visual feedback
        m_loginUI->onLoginResult(response.success(), message);
      });

  connect(
      m_loginUI, &QtLoginUI::registerRequested,
      [this](const QString &username, const QString &email,
             const QString &password) {
        std::string stdUsername = username.toStdString();
        std::string stdEmail = email.toStdString();
        std::string stdPassword = password.toStdString();

        // Debug output
        qDebug() << "Registration attempt - Username:" << username
                 << "Email:" << email
                 << "Password length:" << password.length();

        std::vector<std::string> mnemonic;
        Auth::AuthResponse response = Auth::RegisterUserWithMnemonic(
            stdUsername, stdEmail, stdPassword, mnemonic);
        QString message = QString::fromStdString(response.message);

        // Debug output
        qDebug() << "Registration response - Success:" << response.success()
                 << "Message:" << message;
        qDebug() << "Auth result code:" << static_cast<int>(response.result);

        if (response.success() && !mnemonic.empty()) {
          // Show secure seed phrase display dialog
          QtSeedDisplayDialog seedDialog(mnemonic, this);
          int result = seedDialog.exec();

          if (result == QDialog::Accepted && seedDialog.userConfirmedBackup()) {
            statusBar()->showMessage("Registration and backup completed", 3000);
            QMessageBox::information(
                this, "Registration Successful",
                QString("Account created for %1!\n\nYour seed phrase has been "
                        "securely backed up.\nYou can now sign in with your "
                        "credentials.")
                    .arg(username));
            m_loginUI->onRegisterResult(
                true,
                "Account created and seed phrase backed up successfully!");
          } else {
            // User cancelled or didn't confirm backup
            statusBar()->showMessage("Registration completed - backup required",
                                     5000);
            QMessageBox::warning(
                this, "Backup Required",
                "Your account has been created successfully, but you must "
                "backup your seed phrase!\n\n"
                "⚠️ WARNING: Without a backup of your seed phrase, you may "
                "lose access to your wallet permanently.\n\n"
                "Please use the 'Reveal Seed' button after signing in to "
                "backup your seed phrase.");
            m_loginUI->onRegisterResult(true,
                                        "Account created - please backup your "
                                        "seed phrase using 'Reveal Seed'");
          }
        } else if (response.success()) {
          statusBar()->showMessage("Registration successful", 3000);
          QMessageBox::information(
              this, "Registration Successful",
              QString("Account created for %1!\n\nNote: Seed phrase generation "
                      "had issues. Please use 'Reveal Seed' after signing in.")
                  .arg(username));
          m_loginUI->onRegisterResult(response.success(), message);
        } else {
          statusBar()->showMessage("Registration failed", 3000);
          qDebug() << "Registration failed with message:" << message;
          m_loginUI->onRegisterResult(response.success(), message);
        }
      });

  // Logout handled by navbar sign out button

  connect(m_walletUI, &QtWalletUI::viewBalanceRequested, [this]() {
    if (!m_wallet || g_currentUser.empty() ||
        g_users.find(g_currentUser) == g_users.end()) {
      QMessageBox::warning(this, "Error",
                           "Wallet not initialized or user not logged in");
      return;
    }

    const User &user = g_users[g_currentUser];

    // Get balance information
    WalletAPI::ReceiveInfo info = m_wallet->GetAddressInfo(user.walletAddress);

    // Convert satoshis to BTC for display
    double balanceBTC = m_wallet->ConvertSatoshisToBTC(info.confirmed_balance);
    double unconfirmedBTC =
        m_wallet->ConvertSatoshisToBTC(info.unconfirmed_balance);

    QString balanceText = QString("Confirmed Balance: %1 BTC\n"
                                  "Unconfirmed Balance: %2 BTC\n"
                                  "Total Transactions: %3\n"
                                  "Address: %4")
                              .arg(balanceBTC, 0, 'f', 8)
                              .arg(unconfirmedBTC, 0, 'f', 8)
                              .arg(info.transaction_count)
                              .arg(QString::fromStdString(info.address));

    QMessageBox::information(this, "Wallet Balance", balanceText);
  });

  connect(m_walletUI, &QtWalletUI::sendBitcoinRequested, [this]() {
    if (!m_wallet || g_currentUser.empty() ||
        g_users.find(g_currentUser) == g_users.end()) {
      QMessageBox::warning(this, "Error",
                           "Wallet not initialized or user not logged in");
      return;
    }

    const User &user = g_users[g_currentUser];

    // For demo purposes, show what would happen
    uint64_t currentBalance = m_wallet->GetBalance(user.walletAddress);
    double balanceBTC = m_wallet->ConvertSatoshisToBTC(currentBalance);
    uint64_t estimatedFee = m_wallet->EstimateTransactionFee();
    double feeBTC = m_wallet->ConvertSatoshisToBTC(estimatedFee);

    QString demoText = QString("Current Balance: %1 BTC\n"
                               "Estimated Fee: %2 BTC\n"
                               "Available to Send: %3 BTC\n\n"
                               "Note: This is a demo. Real sending requires:\n"
                               "- Private key signing\n"
                               "- Transaction broadcasting\n"
                               "- Proper input validation")
                           .arg(balanceBTC, 0, 'f', 8)
                           .arg(feeBTC, 0, 'f', 8)
                           .arg(balanceBTC - feeBTC, 0, 'f', 8);

    QMessageBox::information(this, "Send Bitcoin (Demo)", demoText);
  });

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

  // Connect top cryptos page back button
  connect(m_topCryptosPage, &QtTopCryptosPage::backRequested, this,
          &CriptoGualetQt::showWalletScreen);
}

void CriptoGualetQt::createNavbar() {
  m_navbar = new QFrame(m_centralWidget);
  m_navbar->setProperty("class", "navbar");
  m_navbar->setFixedHeight(60);

  QHBoxLayout *navLayout = new QHBoxLayout(m_navbar);
  navLayout->setContentsMargins(20, 10, 20, 10);
  navLayout->setSpacing(10);

  // App title/logo
  m_appTitleLabel = new QLabel("CriptoGualet", m_navbar);
  m_appTitleLabel->setProperty("class", "navbar-title");
  navLayout->addWidget(m_appTitleLabel);

  // Spacer to push sign out button to the right
  navLayout->addStretch();

  // Sign out button
  m_signOutButton = new QPushButton("Sign Out", m_navbar);
  m_signOutButton->setProperty("class", "navbar-button");
  m_signOutButton->setMaximumWidth(100);
  navLayout->addWidget(m_signOutButton);

  // Connect sign out button
  connect(m_signOutButton, &QPushButton::clicked, this,
          &CriptoGualetQt::showLoginScreen);

  // Add navbar to main layout (at the top)
  m_mainLayout->insertWidget(0, m_navbar);

  // Initially hidden until user logs in
  m_navbar->hide();
}

void CriptoGualetQt::createSidebar() {
  // Sidebar is now created in setupUI() since it needs to be a child of
  // contentContainer Connect navigation signals
  connect(m_sidebar, &QtSidebar::navigateToWallet, this,
          &CriptoGualetQt::showWalletScreen);
  connect(m_sidebar, &QtSidebar::navigateToTopCryptos, this,
          &CriptoGualetQt::showTopCryptosPage);
  connect(m_sidebar, &QtSidebar::navigateToSettings, this,
          &CriptoGualetQt::showSettingsScreen);
}

void CriptoGualetQt::setupMenuBar() {
  QMenu *themeMenu = menuBar()->addMenu("&Theme");

  QAction *darkAction = themeMenu->addAction("Dark Theme");
  QAction *lightAction = themeMenu->addAction("Light Theme");
  QAction *cryptoDarkAction = themeMenu->addAction("Crypto Dark");
  QAction *cryptoLightAction = themeMenu->addAction("Crypto Light");

  connect(darkAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::DARK); });

  connect(lightAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::LIGHT); });

  connect(cryptoDarkAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::CRYPTO_DARK); });

  connect(cryptoLightAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::CRYPTO_LIGHT); });

  QMenu *helpMenu = menuBar()->addMenu("&Help");
  QAction *aboutAction = helpMenu->addAction("&About");

  connect(aboutAction, &QAction::triggered, [this]() {
    QMessageBox::about(
        this, "About CriptoGualet",
        "CriptoGualet v1.0\n\nA secure Bitcoin wallet application built with "
        "Qt.\n\n"
        "Features:\n• Modern Qt UI with theming\n• Secure authentication\n• "
        "Bitcoin address generation\n• Demo wallet functionality");
  });
}

void CriptoGualetQt::setupStatusBar() { statusBar()->showMessage("Ready"); }

void CriptoGualetQt::showLoginScreen() {
  g_currentUser.clear();
  m_stackedWidget->setCurrentWidget(m_loginUI);
  updateNavbarVisibility();
  updateSidebarVisibility();
  statusBar()->showMessage("Please log in or create an account");
}

void CriptoGualetQt::showWalletScreen() {
  m_stackedWidget->setCurrentWidget(m_walletUI);
  updateNavbarVisibility();
  updateSidebarVisibility();
  statusBar()->showMessage(
      QString("Logged in as: %1").arg(QString::fromStdString(g_currentUser)));
}

void CriptoGualetQt::showSettingsScreen() {
  m_stackedWidget->setCurrentWidget(m_settingsUI);
  updateNavbarVisibility();
  updateSidebarVisibility();
  statusBar()->showMessage("Settings");
}

void CriptoGualetQt::showTopCryptosPage() {
  m_stackedWidget->setCurrentWidget(m_topCryptosPage);
  updateNavbarVisibility();
  updateSidebarVisibility();
  statusBar()->showMessage("Top Cryptocurrencies by Market Cap");
  m_topCryptosPage->refreshData();
}

void CriptoGualetQt::updateNavbarVisibility() {
  // Show navbar only when not on login screen
  if (m_stackedWidget->currentWidget() != m_loginUI) {
    m_navbar->show();
  } else {
    m_navbar->hide();
  }
}

void CriptoGualetQt::updateSidebarVisibility() {
  // Show sidebar only when not on login screen
  if (m_stackedWidget->currentWidget() != m_loginUI) {
    m_sidebar->show();
    // Position sidebar as overlay at left edge
    QWidget *contentContainer = m_sidebar->parentWidget();
    if (contentContainer) {
      m_sidebar->setGeometry(0, 0, m_sidebar->width(),
                             contentContainer->height());
    }
  } else {
    m_sidebar->hide();
  }
}

void CriptoGualetQt::onThemeChanged() {
  m_loginUI->applyTheme();
  m_walletUI->applyTheme();
  m_settingsUI->applyTheme();
  if (m_sidebar) {
    m_sidebar->applyTheme();
  }
  applyNavbarStyling();
}

void CriptoGualetQt::applyNavbarStyling() {
  // Apply main window stylesheet (includes navbar styles)
  setStyleSheet(m_themeManager->getMainWindowStyleSheet());

  // Apply fonts to navbar components
  m_appTitleLabel->setFont(m_themeManager->titleFont());
  m_signOutButton->setFont(m_themeManager->buttonFont());
}

void CriptoGualetQt::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);

  // Update sidebar geometry as overlay when window is resized
  if (m_sidebar && m_sidebar->isVisible()) {
    QWidget *contentContainer = m_sidebar->parentWidget();
    if (contentContainer) {
      m_sidebar->setGeometry(0, 0, m_sidebar->width(),
                             contentContainer->height());
    }
  }
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  app.setApplicationName("CriptoGualet");
  app.setApplicationVersion("1.0");
  app.setOrganizationName("CriptoGualet");

  qDebug() << "Creating main window...";
  CriptoGualetQt window;

  qDebug() << "Showing window...";
  window.show();
  window.raise();
  window.activateWindow();

  qDebug() << "Window should be visible now. Starting event loop...";
  return app.exec();
}
